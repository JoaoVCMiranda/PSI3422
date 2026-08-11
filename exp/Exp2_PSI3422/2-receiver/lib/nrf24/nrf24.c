#include "nrf24.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>

#include <errno.h>
#include <string.h>


/*
 * ============================================================
 * DEVICETREE
 * ============================================================
 *
 * Esperamos um nó parecido com:
 *
 * nrf24: nrf24@0 {
 *     compatible = "custom,nrf24";
 *     reg = <0>;
 *     spi-max-frequency = <8000000>;
 *
 *     ce-gpios = <...>;
 *     irq-gpios = <...>;
 * };
 *
 * O CSN será controlado pelo próprio SPI/Devicetree.
 */

#define NRF24_NODE DT_NODELABEL(nrf24)


static const struct spi_dt_spec nrf24_spi =
	SPI_DT_SPEC_GET(
		NRF24_NODE,
		SPI_WORD_SET(8) |
		SPI_TRANSFER_MSB |
		SPI_OP_MODE_MASTER
	);


static const struct gpio_dt_spec nrf24_ce =
	GPIO_DT_SPEC_GET(
		NRF24_NODE,
		ce_gpios
	);


static const struct gpio_dt_spec nrf24_irq =
	GPIO_DT_SPEC_GET(
		NRF24_NODE,
		irq_gpios
	);


/*
 * ============================================================
 * REGISTRADORES DO NRF24
 * ============================================================
 */

#define NRF24_REG_CONFIG       0x00
#define NRF24_REG_EN_AA        0x01
#define NRF24_REG_EN_RXADDR    0x02
#define NRF24_REG_SETUP_AW     0x03
#define NRF24_REG_SETUP_RETR   0x04
#define NRF24_REG_RF_CH        0x05
#define NRF24_REG_RF_SETUP     0x06
#define NRF24_REG_STATUS       0x07

#define NRF24_REG_RX_ADDR_P0   0x0A
#define NRF24_REG_TX_ADDR      0x10
#define NRF24_REG_RX_PW_P0     0x11

#define NRF24_REG_FIFO_STATUS  0x17


/*
 * ============================================================
 * COMANDOS SPI DO NRF24
 * ============================================================
 */

#define NRF24_CMD_R_REGISTER      0x00
#define NRF24_CMD_W_REGISTER      0x20

#define NRF24_CMD_R_RX_PAYLOAD    0x61
#define NRF24_CMD_W_TX_PAYLOAD    0xA0

#define NRF24_CMD_FLUSH_TX        0xE1
#define NRF24_CMD_FLUSH_RX        0xE2

#define NRF24_CMD_NOP             0xFF


/*
 * ============================================================
 * BITS DO REGISTRADOR STATUS
 * ============================================================
 */

#define NRF24_STATUS_RX_DR        BIT(6)
#define NRF24_STATUS_TX_DS        BIT(5)
#define NRF24_STATUS_MAX_RT       BIT(4)


/*
 * ============================================================
 * BITS DO REGISTRADOR CONFIG
 * ============================================================
 */

#define NRF24_CONFIG_MASK_RX_DR   BIT(6)
#define NRF24_CONFIG_MASK_TX_DS   BIT(5)
#define NRF24_CONFIG_MASK_MAX_RT  BIT(4)

#define NRF24_CONFIG_EN_CRC       BIT(3)
#define NRF24_CONFIG_CRCO         BIT(2)
#define NRF24_CONFIG_PWR_UP       BIT(1)
#define NRF24_CONFIG_PRIM_RX      BIT(0)


/*
 * ============================================================
 * OBJETOS DO ZEPHYR
 * ============================================================
 */

/*
 * Semáforo utilizado pela interrupção IRQ.
 *
 * Quando o nRF24 coloca IRQ em LOW:
 *
 *     IRQ
 *      |
 *      v
 * callback
 *      |
 *      v
 * k_sem_give()
 *
 * A função send() ou receive() que estiver esperando
 * será liberada.
 */
static struct k_sem nrf24_irq_sem;


/*
 * Callback GPIO utilizado para IRQ.
 */
static struct gpio_callback nrf24_irq_callback;


/*
 * ============================================================
 * ENDEREÇO DO RÁDIO
 * ============================================================
 *
 * Ambos os módulos precisam conhecer os endereços.
 *
 * Para um primeiro teste usaremos:
 *
 *     E7 E7 E7 E7 E7
 *
 * Posteriormente isso pode virar configuração.
 */

static const uint8_t nrf24_address[5] = {
	0xE7,
	0xE7,
	0xE7,
	0xE7,
	0xE7
};


/*
 * ============================================================
 * IRQ CALLBACK
 * ============================================================
 */

static void nrf24_irq_handler(
	const struct device *port,
	struct gpio_callback *cb,
	gpio_port_pins_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	/*
	 * O nRF24 colocou IRQ em LOW.
	 *
	 * Não fazemos SPI dentro da interrupção.
	 *
	 * Apenas avisamos a thread que existe
	 * um evento para ser processado.
	 */
	k_sem_give(&nrf24_irq_sem);
}


/*
 * ============================================================
 * SPI - WRITE
 * ============================================================
 */

static int nrf24_spi_write(
	const uint8_t *data,
	size_t length)
{
	struct spi_buf tx_buf = {
		.buf = (void *)data,
		.len = length,
	};

	struct spi_buf_set tx_set = {
		.buffers = &tx_buf,
		.count = 1,
	};

	return spi_write_dt(
		&nrf24_spi,
		&tx_set
	);
}


/*
 * ============================================================
 * SPI - TRANSCEIVE
 * ============================================================
 */

static int nrf24_spi_transceive(
	const uint8_t *tx,
	uint8_t *rx,
	size_t length)
{
	struct spi_buf tx_buf = {
		.buf = (void *)tx,
		.len = length,
	};

	struct spi_buf rx_buf = {
		.buf = rx,
		.len = length,
	};

	struct spi_buf_set tx_set = {
		.buffers = &tx_buf,
		.count = 1,
	};

	struct spi_buf_set rx_set = {
		.buffers = &rx_buf,
		.count = 1,
	};

	return spi_transceive_dt(
		&nrf24_spi,
		&tx_set,
		&rx_set
	);
}


/*
 * ============================================================
 * ESCREVER REGISTRADOR
 * ============================================================
 *
 * Exemplo:
 *
 *     nrf24_write_register(
 *         NRF24_REG_RF_CH,
 *         40
 *     );
 *
 * SPI:
 *
 *     CSN = 0
 *
 *     MOSI:
 *     0x25
 *     0x28
 *
 *     CSN = 1
 */

static int nrf24_write_register(
	uint8_t reg,
	uint8_t value)
{
	uint8_t tx[2];

	tx[0] =
		NRF24_CMD_W_REGISTER |
		(reg & 0x1F);

	tx[1] = value;

	return nrf24_spi_write(
		tx,
		sizeof(tx)
	);
}


/*
 * ============================================================
 * LER REGISTRADOR
 * ============================================================
 *
 * SPI:
 *
 *     CSN = 0
 *
 *     MOSI -> comando
 *     MOSI -> NOP
 *
 *     MISO <- STATUS
 *     MISO <- valor
 *
 *     CSN = 1
 */

static int nrf24_read_register(
	uint8_t reg,
	uint8_t *value)
{
	uint8_t tx[2];
	uint8_t rx[2];

	tx[0] =
		NRF24_CMD_R_REGISTER |
		(reg & 0x1F);

	tx[1] = NRF24_CMD_NOP;

	int ret = nrf24_spi_transceive(
		tx,
		rx,
		sizeof(tx)
	);

	if (ret < 0) {
		return ret;
	}

	/*
	 * O primeiro byte recebido é o STATUS.
	 *
	 * O segundo byte é o valor do registrador.
	 */
	*value = rx[1];

	return 0;
}


/*
 * ============================================================
 * ESCREVER ENDEREÇO
 * ============================================================
 */

static int nrf24_write_address(
	uint8_t reg,
	const uint8_t *address)
{
	uint8_t tx[6];

	tx[0] =
		NRF24_CMD_W_REGISTER |
		(reg & 0x1F);

	memcpy(
		&tx[1],
		address,
		5
	);

	return nrf24_spi_write(
		tx,
		sizeof(tx)
	);
}


/*
 * ============================================================
 * FLUSH TX
 * ============================================================
 */

static int nrf24_flush_tx(void)
{
	uint8_t command =
		NRF24_CMD_FLUSH_TX;

	return nrf24_spi_write(
		&command,
		1
	);
}


/*
 * ============================================================
 * FLUSH RX
 * ============================================================
 */

static int nrf24_flush_rx(void)
{
	uint8_t command =
		NRF24_CMD_FLUSH_RX;

	return nrf24_spi_write(
		&command,
		1
	);
}


/*
 * ============================================================
 * ESCREVER PAYLOAD NA TX FIFO
 * ============================================================
 */

static int nrf24_write_payload(
	const uint8_t *data,
	size_t length)
{
	if (data == NULL) {
		return -EINVAL;
	}

	if (length == 0 ||
	    length > NRF24_MAX_PAYLOAD_SIZE) {
		return -EINVAL;
	}

	/*
	 * Primeiro byte:
	 *
	 *     W_TX_PAYLOAD
	 *
	 * Depois:
	 *
	 *     PAYLOAD
	 */

	uint8_t tx[
		1 + NRF24_MAX_PAYLOAD_SIZE
	];

	tx[0] =
		NRF24_CMD_W_TX_PAYLOAD;

	memcpy(
		&tx[1],
		data,
		length
	);

	return nrf24_spi_write(
		tx,
		length + 1
	);
}


/*
 * ============================================================
 * LER PAYLOAD DA RX FIFO
 * ============================================================
 */

static int nrf24_read_payload(
	uint8_t *data,
	size_t length)
{
	if (data == NULL) {
		return -EINVAL;
	}

	if (length == 0 ||
	    length > NRF24_MAX_PAYLOAD_SIZE) {
		return -EINVAL;
	}

	uint8_t tx[
		1 + NRF24_MAX_PAYLOAD_SIZE
	];

	uint8_t rx[
		1 + NRF24_MAX_PAYLOAD_SIZE
	];

	memset(
		tx,
		NRF24_CMD_NOP,
		sizeof(tx)
	);

	tx[0] =
		NRF24_CMD_R_RX_PAYLOAD;

	int ret = nrf24_spi_transceive(
		tx,
		rx,
		length + 1
	);

	if (ret < 0) {
		return ret;
	}

	/*
	 * rx[0] = STATUS
	 * rx[1..] = payload
	 */

	memcpy(
		data,
		&rx[1],
		length
	);

	return 0;
}


/*
 * ============================================================
 * CONFIGURAÇÃO DO RÁDIO
 * ============================================================
 */

static int nrf24_configure(void)
{
	int ret;


	/*
	 * CE = 0
	 *
	 * Antes de configurar o rádio,
	 * deixamos o transceptor desabilitado.
	 */

	ret = gpio_pin_set_dt(
		&nrf24_ce,
		0
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * CONFIG
	 *
	 * EN_CRC
	 * CRCO
	 * PWR_UP
	 * PRIM_RX
	 *
	 * Começamos em RX.
	 */

	ret = nrf24_write_register(
		NRF24_REG_CONFIG,
		NRF24_CONFIG_EN_CRC |
		NRF24_CONFIG_CRCO |
		NRF24_CONFIG_PWR_UP |
		NRF24_CONFIG_PRIM_RX
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * EN_AA
	 *
	 * Habilita Auto ACK no pipe 0.
	 */

	ret = nrf24_write_register(
		NRF24_REG_EN_AA,
		BIT(0)
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * EN_RXADDR
	 *
	 * Habilita pipe 0.
	 */

	ret = nrf24_write_register(
		NRF24_REG_EN_RXADDR,
		BIT(0)
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * SETUP_AW
	 *
	 * 0x03 = endereço de 5 bytes.
	 */

	ret = nrf24_write_register(
		NRF24_REG_SETUP_AW,
		0x03
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * SETUP_RETR
	 *
	 * ARD = 2
	 *
	 * Delay:
	 *
	 *     (ARD + 1) * 250 us
	 *
	 *     = 750 us
	 *
	 * ARC = 5
	 *
	 * Até 5 retransmissões.
	 */

	ret = nrf24_write_register(
		NRF24_REG_SETUP_RETR,
		(0x02 << 4) | 0x05
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * RF_CH
	 *
	 * Canal 40.
	 *
	 * Frequência:
	 *
	 *     2400 + 40
	 *
	 *     = 2440 MHz
	 */

	ret = nrf24_write_register(
		NRF24_REG_RF_CH,
		40
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * RF_SETUP
	 *
	 * 1 Mbps
	 * -6 dBm
	 */

	ret = nrf24_write_register(
		NRF24_REG_RF_SETUP,
		0x06
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * RX_PW_P0
	 *
	 * Payload fixo de 32 bytes.
	 */

	ret = nrf24_write_register(
		NRF24_REG_RX_PW_P0,
		NRF24_MAX_PAYLOAD_SIZE
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * Endereço de recepção.
	 */

	ret = nrf24_write_address(
		NRF24_REG_RX_ADDR_P0,
		nrf24_address
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * Endereço de transmissão.
	 */

	ret = nrf24_write_address(
		NRF24_REG_TX_ADDR,
		nrf24_address
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * Limpa TX FIFO.
	 */

	ret = nrf24_flush_tx();

	if (ret < 0) {
		return ret;
	}


	/*
	 * Limpa RX FIFO.
	 */

	ret = nrf24_flush_rx();

	if (ret < 0) {
		return ret;
	}


	/*
	 * Limpa flags de IRQ.
	 *
	 * Para limpar uma flag no nRF24,
	 * escrevemos 1 nela.
	 */

	ret = nrf24_write_register(
		NRF24_REG_STATUS,
		NRF24_STATUS_RX_DR |
		NRF24_STATUS_TX_DS |
		NRF24_STATUS_MAX_RT
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * Depois de PWR_UP precisamos aguardar
	 * o nRF24 entrar no estado operacional.
	 */

	k_sleep(K_MSEC(2));


	return 0;
}


/*
 * ============================================================
 * INICIALIZAÇÃO PÚBLICA
 * ============================================================
 */

int nrf24_init(void)
{
	int ret;


	/*
	 * Inicializa semáforo.
	 */

	k_sem_init(
		&nrf24_irq_sem,
		0,
		1
	);


	/*
	 * Verifica SPI.
	 */

	if (!spi_is_ready_dt(&nrf24_spi)) {
		return -ENODEV;
	}


	/*
	 * Verifica CE.
	 */

	if (!gpio_is_ready_dt(&nrf24_ce)) {
		return -ENODEV;
	}


	/*
	 * Verifica IRQ.
	 */

	if (!gpio_is_ready_dt(&nrf24_irq)) {
		return -ENODEV;
	}


	/*
	 * CE como saída.
	 *
	 * Inicialmente LOW.
	 */

	ret = gpio_pin_configure_dt(
		&nrf24_ce,
		GPIO_OUTPUT_INACTIVE
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * IRQ como entrada.
	 */

	ret = gpio_pin_configure_dt(
		&nrf24_irq,
		GPIO_INPUT
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * Cria callback.
	 */

	gpio_init_callback(
		&nrf24_irq_callback,
		nrf24_irq_handler,
		BIT(nrf24_irq.pin)
	);


	/*
	 * Registra callback.
	 */

	ret = gpio_add_callback(
		nrf24_irq.port,
		&nrf24_irq_callback
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * IRQ é ativo em LOW.
	 *
	 * GPIO_ACTIVE_LOW foi configurado
	 * no Devicetree.
	 *
	 * EDGE_TO_ACTIVE significa:
	 *
	 *     detecte a transição para o
	 *     estado ativo.
	 */

	ret = gpio_pin_interrupt_configure_dt(
		&nrf24_irq,
		GPIO_INT_EDGE_TO_ACTIVE
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * Configura o nRF24.
	 */

	ret = nrf24_configure();

	if (ret < 0) {
		return ret;
	}


	/*
	 * Coloca o rádio em RX.
	 *
	 * CONFIG.PRIM_RX = 1
	 * CE = 1
	 */

	ret = gpio_pin_set_dt(
		&nrf24_ce,
		1
	);

	if (ret < 0) {
		return ret;
	}


	return 0;
}


/*
 * ============================================================
 * TRANSMISSÃO
 * ============================================================
 */

int nrf24_send(
	const uint8_t *data,
	size_t length)
{
	int ret;


	if (data == NULL) {
		return -EINVAL;
	}


	if (length == 0 ||
	    length > NRF24_MAX_PAYLOAD_SIZE) {
		return -EINVAL;
	}


	/*
	 * --------------------------------------------
	 * 1. CE = 0
	 *
	 * Saímos do modo RX.
	 * --------------------------------------------
	 */

	ret = gpio_pin_set_dt(
		&nrf24_ce,
		0
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * --------------------------------------------
	 * 2. CONFIGURA PRIM_RX = 0
	 *
	 * Agora o nRF24 será TX.
	 * --------------------------------------------
	 */

	ret = nrf24_write_register(
		NRF24_REG_CONFIG,
		NRF24_CONFIG_EN_CRC |
		NRF24_CONFIG_CRCO |
		NRF24_CONFIG_PWR_UP
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * --------------------------------------------
	 * 3. Limpa flags antigas.
	 * --------------------------------------------
	 */

	ret = nrf24_write_register(
		NRF24_REG_STATUS,
		NRF24_STATUS_TX_DS |
		NRF24_STATUS_MAX_RT
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * --------------------------------------------
	 * 4. Limpa TX FIFO.
	 * --------------------------------------------
	 */

	ret = nrf24_flush_tx();

	if (ret < 0) {
		return ret;
	}


	/*
	 * --------------------------------------------
	 * 5. Coloca payload na TX FIFO.
	 * --------------------------------------------
	 */

	ret = nrf24_write_payload(
		data,
		length
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * --------------------------------------------
	 * 6. CE = 1
	 *
	 * Inicia transmissão.
	 * --------------------------------------------
	 */

	ret = gpio_pin_set_dt(
		&nrf24_ce,
		1
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * --------------------------------------------
	 * 7. Espera IRQ.
	 *
	 * IRQ pode significar:
	 *
	 *     TX_DS
	 *
	 * ou
	 *
	 *     MAX_RT
	 *
	 * Timeout de 100 ms.
	 * --------------------------------------------
	 */

	ret = k_sem_take(
		&nrf24_irq_sem,
		K_MSEC(100)
	);


	/*
	 * CE volta para LOW.
	 */

	gpio_pin_set_dt(
		&nrf24_ce,
		0
	);


	if (ret < 0) {
		return -ETIMEDOUT;
	}


	/*
	 * --------------------------------------------
	 * 8. Lê STATUS.
	 * --------------------------------------------
	 */

	uint8_t status;

	ret = nrf24_read_register(
		NRF24_REG_STATUS,
		&status
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * --------------------------------------------
	 * 9. TX_DS
	 *
	 * Transmissão concluída.
	 * --------------------------------------------
	 */

	if (status & NRF24_STATUS_TX_DS) {

		/*
		 * Limpa flag.
		 */

		nrf24_write_register(
			NRF24_REG_STATUS,
			NRF24_STATUS_TX_DS
		);

		/*
		 * Volta para RX.
		 */

		nrf24_write_register(
			NRF24_REG_CONFIG,
			NRF24_CONFIG_EN_CRC |
			NRF24_CONFIG_CRCO |
			NRF24_CONFIG_PWR_UP |
			NRF24_CONFIG_PRIM_RX
		);

		gpio_pin_set_dt(
			&nrf24_ce,
			1
		);

		return 0;
	}


	/*
	 * --------------------------------------------
	 * 10. MAX_RT
	 *
	 * Falhou após as retransmissões.
	 * --------------------------------------------
	 */

	if (status & NRF24_STATUS_MAX_RT) {

		/*
		 * Limpa flag.
		 */

		nrf24_write_register(
			NRF24_REG_STATUS,
			NRF24_STATUS_MAX_RT
		);

		/*
		 * Remove pacote da TX FIFO.
		 */

		nrf24_flush_tx();

		/*
		 * Volta para RX.
		 */

		nrf24_write_register(
			NRF24_REG_CONFIG,
			NRF24_CONFIG_EN_CRC |
			NRF24_CONFIG_CRCO |
			NRF24_CONFIG_PWR_UP |
			NRF24_CONFIG_PRIM_RX
		);

		gpio_pin_set_dt(
			&nrf24_ce,
			1
		);

		return -EIO;
	}


	/*
	 * Não sabemos qual evento ocorreu.
	 */

	return -EIO;
}


/*
 * ============================================================
 * RECEPÇÃO
 * ============================================================
 */

int nrf24_receive(
	uint8_t *data,
	size_t max_length)
{
	int ret;


	if (data == NULL) {
		return -EINVAL;
	}


	if (max_length < NRF24_MAX_PAYLOAD_SIZE) {
		/*
		 * Esta primeira versão usa
		 * payload fixo de 32 bytes.
		 *
		 * Portanto o buffer precisa ter
		 * espaço para 32 bytes.
		 */
		return -EMSGSIZE;
	}


	/*
	 * --------------------------------------------
	 * Garantimos modo RX.
	 * --------------------------------------------
	 */

	ret = nrf24_write_register(
		NRF24_REG_CONFIG,
		NRF24_CONFIG_EN_CRC |
		NRF24_CONFIG_CRCO |
		NRF24_CONFIG_PWR_UP |
		NRF24_CONFIG_PRIM_RX
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * CE = 1
	 *
	 * Rádio fica escutando.
	 */

	ret = gpio_pin_set_dt(
		&nrf24_ce,
		1
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * --------------------------------------------
	 * Espera IRQ.
	 *
	 * O nRF24 baixa IRQ quando:
	 *
	 *     RX_DR
	 *
	 * significa:
	 *
	 *     "recebi um pacote".
	 * --------------------------------------------
	 */

	ret = k_sem_take(
		&nrf24_irq_sem,
		K_FOREVER
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * --------------------------------------------
	 * Lê STATUS.
	 * --------------------------------------------
	 */

	uint8_t status;

	ret = nrf24_read_register(
		NRF24_REG_STATUS,
		&status
	);

	if (ret < 0) {
		return ret;
	}


	/*
	 * --------------------------------------------
	 * RX_DR
	 * --------------------------------------------
	 */

	if (status & NRF24_STATUS_RX_DR) {

		/*
		 * Como estamos usando payload fixo,
		 * sempre lemos 32 bytes.
		 */

		ret = nrf24_read_payload(
			data,
			NRF24_MAX_PAYLOAD_SIZE
		);

		if (ret < 0) {
			return ret;
		}


		/*
		 * Limpa RX_DR.
		 */

		ret = nrf24_write_register(
			NRF24_REG_STATUS,
			NRF24_STATUS_RX_DR
		);

		if (ret < 0) {
			return ret;
		}


		return NRF24_MAX_PAYLOAD_SIZE;
	}


	/*
	 * IRQ ocorreu, mas não foi RX_DR.
	 */

	return -EIO;
}