/*
 * Base histórica: uma primeira versão deste projeto (Exp2, protótipo
 * anterior) usava `spi_dt_spec` + `spi_transceive_dt` do Zephyr,
 * resolvidos a partir de um nó devicetree customizado `nrf24` — nó
 * que nunca existiu de fato em lugar nenhum do repositório (nenhum
 * .overlay o declarava) e que, mesmo se declarado, não teria como
 * funcionar: este board/framework (frdm_kl25z,
 * framework-zephyr@2.20701.220422) não tem NENHUM nó SPI na
 * devicetree (conferido em dts/arm/nxp/nxp_kl25z.dtsi) — não existe
 * driver SPI Zephyr disponível aqui, então `spi_dt_spec`/
 * `SPI_DT_SPEC_GET` não tinha como compilar contra um `&spi0` que não
 * existe. Protocolo/registradores do nRF24L01+ mantidos; só a camada
 * de I/O (nrf24_spi_write/nrf24_spi_transceive e o controle de
 * CE/CSN) foi reescrita.
 *
 * Substituído por spi.c (bare-metal, Prof. Gustavo Rehder — mesmo
 * autor de pwm_z42): SPI0/PORTC, chip-select manual via GPIO comum
 * (CE/IRQ continuam GPIO Zephyr nativo, que funciona normalmente
 * aqui — só SPI e TPM não têm nó de devicetree neste framework).
 */

#include "nrf24.h"
#include "spi.h"

#include <zephyr.h>
#include <sys/util.h>

#include <errno.h>
#include <string.h>


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

#define NRF24_CONFIG_EN_CRC       BIT(3)
#define NRF24_CONFIG_CRCO         BIT(2)
#define NRF24_CONFIG_PWR_UP       BIT(1)
#define NRF24_CONFIG_PRIM_RX      BIT(0)


/*
 * ============================================================
 * OBJETOS
 * ============================================================
 */

static struct gpio_dt_spec nrf24_ce;
static struct gpio_dt_spec nrf24_csn;
static struct gpio_dt_spec nrf24_irq;

static struct k_sem nrf24_irq_sem;
static struct gpio_callback nrf24_irq_callback;

static const uint8_t nrf24_address[5] = { 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 };


/*
 * ============================================================
 * IRQ CALLBACK
 * ============================================================
 */

static void nrf24_irq_handler(const struct device *port,
                               struct gpio_callback *cb,
                               gpio_port_pins_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	/* Não fazemos SPI dentro da interrupção — só avisamos a thread. */
	k_sem_give(&nrf24_irq_sem);
}


/*
 * ============================================================
 * SPI (bare-metal via spi.c) - WRITE / TRANSCEIVE
 * ============================================================
 * CSN é controlado manualmente aqui (CS_MAN no spi_init), envolvendo
 * cada transação — é o análogo bare-metal de "CSN controlado pelo
 * SPI/Devicetree" que o comentário original mencionava.
 */

static void nrf24_csn_select(bool active)
{
	/* csn.dt_flags = GPIO_ACTIVE_LOW; gpio_pin_set_dt(spec, 1) já
	 * aplica a inversão, então "1" aqui significa "CSN eletricamente
	 * baixo" = selecionado. */
	gpio_pin_set_dt(&nrf24_csn, active ? 1 : 0);
}

static int nrf24_spi_write(const uint8_t *data, size_t length)
{
	nrf24_csn_select(true);
	for (size_t i = 0; i < length; i++) {
		spi_exchange(SPI_0, data[i]);
	}
	nrf24_csn_select(false);
	return 0;
}

static int nrf24_spi_transceive(const uint8_t *tx, uint8_t *rx, size_t length)
{
	nrf24_csn_select(true);
	for (size_t i = 0; i < length; i++) {
		rx[i] = spi_exchange(SPI_0, tx[i]);
	}
	nrf24_csn_select(false);
	return 0;
}


/*
 * ============================================================
 * REGISTRADOR / PAYLOAD (protocolo nRF24 — inalterado)
 * ============================================================
 */

static int nrf24_write_register(uint8_t reg, uint8_t value)
{
	uint8_t tx[2];

	tx[0] = NRF24_CMD_W_REGISTER | (reg & 0x1F);
	tx[1] = value;

	return nrf24_spi_write(tx, sizeof(tx));
}

static int nrf24_read_register(uint8_t reg, uint8_t *value)
{
	uint8_t tx[2];
	uint8_t rx[2];

	tx[0] = NRF24_CMD_R_REGISTER | (reg & 0x1F);
	tx[1] = NRF24_CMD_NOP;

	int ret = nrf24_spi_transceive(tx, rx, sizeof(tx));

	if (ret < 0) {
		return ret;
	}

	*value = rx[1];
	return 0;
}

static int nrf24_write_address(uint8_t reg, const uint8_t *address)
{
	uint8_t tx[6];

	tx[0] = NRF24_CMD_W_REGISTER | (reg & 0x1F);
	memcpy(&tx[1], address, 5);

	return nrf24_spi_write(tx, sizeof(tx));
}

static int nrf24_flush_tx()
{
	uint8_t command = NRF24_CMD_FLUSH_TX;
	return nrf24_spi_write(&command, 1);
}

static int nrf24_flush_rx()
{
	uint8_t command = NRF24_CMD_FLUSH_RX;
	return nrf24_spi_write(&command, 1);
}

static int nrf24_write_payload(const uint8_t *data, size_t length)
{
	if (data == NULL || length == 0 || length > NRF24_MAX_PAYLOAD_SIZE) {
		return -EINVAL;
	}

	uint8_t tx[1 + NRF24_MAX_PAYLOAD_SIZE];

	tx[0] = NRF24_CMD_W_TX_PAYLOAD;
	memcpy(&tx[1], data, length);

	return nrf24_spi_write(tx, length + 1);
}

static int nrf24_read_payload(uint8_t *data, size_t length)
{
	if (data == NULL || length == 0 || length > NRF24_MAX_PAYLOAD_SIZE) {
		return -EINVAL;
	}

	uint8_t tx[1 + NRF24_MAX_PAYLOAD_SIZE];
	uint8_t rx[1 + NRF24_MAX_PAYLOAD_SIZE];

	memset(tx, NRF24_CMD_NOP, sizeof(tx));
	tx[0] = NRF24_CMD_R_RX_PAYLOAD;

	int ret = nrf24_spi_transceive(tx, rx, length + 1);

	if (ret < 0) {
		return ret;
	}

	memcpy(data, &rx[1], length);
	return 0;
}


/*
 * ============================================================
 * CONFIGURAÇÃO DO RÁDIO (protocolo — inalterado)
 * ============================================================
 */

static int nrf24_configure()
{
	int ret;

	ret = gpio_pin_set_dt(&nrf24_ce, 0);
	if (ret < 0) return ret;

	ret = nrf24_write_register(NRF24_REG_CONFIG,
		NRF24_CONFIG_EN_CRC | NRF24_CONFIG_CRCO |
		NRF24_CONFIG_PWR_UP | NRF24_CONFIG_PRIM_RX);
	if (ret < 0) return ret;

	ret = nrf24_write_register(NRF24_REG_EN_AA, BIT(0));
	if (ret < 0) return ret;

	ret = nrf24_write_register(NRF24_REG_EN_RXADDR, BIT(0));
	if (ret < 0) return ret;

	ret = nrf24_write_register(NRF24_REG_SETUP_AW, 0x03);
	if (ret < 0) return ret;

	/* ARD=2 -> (2+1)*250us = 750us, ARC=5 retransmissões */
	ret = nrf24_write_register(NRF24_REG_SETUP_RETR, (0x02 << 4) | 0x05);
	if (ret < 0) return ret;

	/* Canal 40 -> 2400+40 = 2440 MHz */
	ret = nrf24_write_register(NRF24_REG_RF_CH, 40);
	if (ret < 0) return ret;

	/* 1 Mbps, -6 dBm */
	ret = nrf24_write_register(NRF24_REG_RF_SETUP, 0x06);
	if (ret < 0) return ret;

	ret = nrf24_write_register(NRF24_REG_RX_PW_P0, NRF24_MAX_PAYLOAD_SIZE);
	if (ret < 0) return ret;

	ret = nrf24_write_address(NRF24_REG_RX_ADDR_P0, nrf24_address);
	if (ret < 0) return ret;

	ret = nrf24_write_address(NRF24_REG_TX_ADDR, nrf24_address);
	if (ret < 0) return ret;

	ret = nrf24_flush_tx();
	if (ret < 0) return ret;

	ret = nrf24_flush_rx();
	if (ret < 0) return ret;

	/* Limpa flags de IRQ (escreve 1 para limpar) */
	ret = nrf24_write_register(NRF24_REG_STATUS,
		NRF24_STATUS_RX_DR | NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT);
	if (ret < 0) return ret;

	k_sleep(K_MSEC(2)); /* aguarda PWR_UP entrar em regime */

	return 0;
}


/*
 * ============================================================
 * INICIALIZAÇÃO PÚBLICA
 * ============================================================
 */

int nrf24_init(const struct gpio_dt_spec *ce,
               const struct gpio_dt_spec *csn,
               const struct gpio_dt_spec *irq)
{
	int ret;

	nrf24_ce = *ce;
	nrf24_csn = *csn;
	nrf24_irq = *irq;

	k_sem_init(&nrf24_irq_sem, 0, 1);

	if (!device_is_ready(nrf24_ce.port) ||
	    !device_is_ready(nrf24_csn.port) ||
	    !device_is_ready(nrf24_irq.port)) {
		return -ENODEV;
	}

	/* CSN HIGH (desseleciona) ANTES de inicializar SPI — evita que o
	 * nRF24 interprete transições espúrias no clock durante o MUX
	 * setup de PTC5/6/7 como comandos SPI válidos. */
	ret = gpio_pin_configure_dt(&nrf24_csn, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) return ret;

	/* SPI0/PORTC (ALT_0), ~1 MHz, chip-select manual (ver spi.h/spi.c) */
	if (!spi_init(SPI_0, ALT_0, PRESCALE_0, DIVISOR_2, CS_MAN)) {
		return -ENODEV;
	}

	/* Power on reset do nRF24L01+: o datasheet exige >= 100 ms após
	 * power-on antes de aceitar comandos SPI (seção 6.1.7). Sem este
	 * delay, todas as escritas de registrador são ignoradas. */
	k_sleep(K_MSEC(100));

	ret = gpio_pin_configure_dt(&nrf24_ce, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) return ret;

	ret = gpio_pin_configure_dt(&nrf24_irq, GPIO_INPUT);
	if (ret < 0) return ret;

	gpio_init_callback(&nrf24_irq_callback, nrf24_irq_handler, BIT(nrf24_irq.pin));

	ret = gpio_add_callback(nrf24_irq.port, &nrf24_irq_callback);
	if (ret < 0) return ret;

	/* IRQ do nRF24 é ativo em LOW; GPIO_ACTIVE_LOW deve estar no dt_flags do spec */
	ret = gpio_pin_interrupt_configure_dt(&nrf24_irq, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0) return ret;

	ret = nrf24_configure();
	if (ret < 0) return ret;

	/* Coloca em RX: CONFIG.PRIM_RX=1, CE=1 */
	return gpio_pin_set_dt(&nrf24_ce, 1);
}


/*
 * ============================================================
 * TRANSMISSÃO / RECEPÇÃO (protocolo — inalterado)
 * ============================================================
 */

int nrf24_send(const uint8_t *data, size_t length)
{
	int ret;

	if (data == NULL || length == 0 || length > NRF24_MAX_PAYLOAD_SIZE) {
		return -EINVAL;
	}

	ret = gpio_pin_set_dt(&nrf24_ce, 0);
	if (ret < 0) return ret;

	ret = nrf24_write_register(NRF24_REG_CONFIG,
		NRF24_CONFIG_EN_CRC | NRF24_CONFIG_CRCO | NRF24_CONFIG_PWR_UP);
	if (ret < 0) return ret;

	ret = nrf24_write_register(NRF24_REG_STATUS,
		NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT);
	if (ret < 0) return ret;

	ret = nrf24_flush_tx();
	if (ret < 0) return ret;

	ret = nrf24_write_payload(data, length);
	if (ret < 0) return ret;

	ret = gpio_pin_set_dt(&nrf24_ce, 1);
	if (ret < 0) return ret;

	int64_t end = k_uptime_get() + 100;
	while (k_uptime_get() < end && gpio_pin_get_dt(&nrf24_irq) == 0) {
		k_sleep(K_MSEC(1));
	}
	ret = (gpio_pin_get_dt(&nrf24_irq) == 0) ? -ETIMEDOUT : 0;
	gpio_pin_set_dt(&nrf24_ce, 0);

	if (ret < 0) {
		nrf24_flush_tx();
		nrf24_write_register(NRF24_REG_STATUS, NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT);
		nrf24_write_register(NRF24_REG_CONFIG, NRF24_CONFIG_EN_CRC | NRF24_CONFIG_CRCO | NRF24_CONFIG_PWR_UP | NRF24_CONFIG_PRIM_RX);
		gpio_pin_set_dt(&nrf24_ce, 1);
		return -ETIMEDOUT;
	}

	uint8_t status;
	ret = nrf24_read_register(NRF24_REG_STATUS, &status);
	if (ret < 0) return ret;

	if (status & NRF24_STATUS_TX_DS) {
		nrf24_write_register(NRF24_REG_STATUS, NRF24_STATUS_TX_DS);
		nrf24_write_register(NRF24_REG_CONFIG,
			NRF24_CONFIG_EN_CRC | NRF24_CONFIG_CRCO |
			NRF24_CONFIG_PWR_UP | NRF24_CONFIG_PRIM_RX);
		gpio_pin_set_dt(&nrf24_ce, 1);
		return 0;
	}

	if (status & NRF24_STATUS_MAX_RT) {
		nrf24_write_register(NRF24_REG_STATUS, NRF24_STATUS_MAX_RT);
		nrf24_flush_tx();
		nrf24_write_register(NRF24_REG_CONFIG,
			NRF24_CONFIG_EN_CRC | NRF24_CONFIG_CRCO |
			NRF24_CONFIG_PWR_UP | NRF24_CONFIG_PRIM_RX);
		gpio_pin_set_dt(&nrf24_ce, 1);
		return -EIO;
	}

	return -EIO;
}

int nrf24_receive(uint8_t *data, size_t max_length, k_timeout_t timeout)
{
	int ret;

	if (data == NULL) {
		return -EINVAL;
	}

	if (max_length < NRF24_MAX_PAYLOAD_SIZE) {
		return -EMSGSIZE;
	}

	ret = nrf24_write_register(NRF24_REG_CONFIG,
		NRF24_CONFIG_EN_CRC | NRF24_CONFIG_CRCO |
		NRF24_CONFIG_PWR_UP | NRF24_CONFIG_PRIM_RX);
	if (ret < 0) return ret;

	ret = gpio_pin_set_dt(&nrf24_ce, 1);
	if (ret < 0) return ret;

	int64_t end_rx = k_uptime_get() + 100;
	while (k_uptime_get() < end_rx && gpio_pin_get_dt(&nrf24_irq) == 0) {
		k_sleep(K_MSEC(1));
	}
	if (gpio_pin_get_dt(&nrf24_irq) == 0) {
		return -ETIMEDOUT;
	}

	uint8_t status;
	ret = nrf24_read_register(NRF24_REG_STATUS, &status);
	if (ret < 0) return ret;

	if (status & NRF24_STATUS_RX_DR) {
		ret = nrf24_read_payload(data, NRF24_MAX_PAYLOAD_SIZE);
		if (ret < 0) return ret;

		ret = nrf24_write_register(NRF24_REG_STATUS, NRF24_STATUS_RX_DR);
		if (ret < 0) return ret;

		return NRF24_MAX_PAYLOAD_SIZE;
	}

	return -EIO;
}
