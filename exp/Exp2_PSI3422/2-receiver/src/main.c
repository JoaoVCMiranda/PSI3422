#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>

#include "nrf24.h"

#define PAYLOAD_SIZE 32

#define CMD_RED     'R'
#define CMD_GREEN   'G'
#define CMD_OFF     'O'

/*
 * LEDs da FRDM-KL25Z
 *
 * PTB18 -> vermelho
 * PTB19 -> verde
 * PTD1  -> azul
 *
 * Os LEDs da placa são ativos em LOW.
 */

static const struct gpio_dt_spec led_red =
	GPIO_DT_SPEC_GET(DT_NODELABEL(led_red), gpios);

static const struct gpio_dt_spec led_green =
	GPIO_DT_SPEC_GET(DT_NODELABEL(led_green), gpios);


static void leds_off(void)
{
	gpio_pin_set_dt(&led_red, 0);
	gpio_pin_set_dt(&led_green, 0);
}


static void set_red(void)
{
	leds_off();

	gpio_pin_set_dt(&led_red, 1);
}


static void set_green(void)
{
	leds_off();

	gpio_pin_set_dt(&led_green, 1);
}


static void process_command(uint8_t command)
{
	switch (command) {

	case CMD_RED:
		printk("Comando: VERMELHO\n");
		set_red();
		break;

	case CMD_GREEN:
		printk("Comando: VERDE\n");
		set_green();
		break;

	case CMD_OFF:
		printk("Comando: DESLIGADO\n");
		leds_off();
		break;

	default:
		printk("Comando desconhecido: 0x%02X\n", command);
		break;
	}
}


static int process_packet(void)
{
	int ret;
	uint8_t payload[PAYLOAD_SIZE];

	ret = nrf24_receive(
		payload,
		sizeof(payload)
	);

	if (ret < 0) {
		printk("ERRO recebendo pacote: %d\n", ret);
		return ret;
	}

	/*
	 * O primeiro byte contém nosso comando.
	 */
	process_command(payload[0]);

	return 0;
}


static int init(void)
{
	int ret;

	printk("\n");
	printk("=============================\n");
	printk(" nRF24 - RECEPTOR\n");
	printk("=============================\n");

	/*
	 * Verifica LED vermelho.
	 */
	if (!gpio_is_ready_dt(&led_red)) {
		printk("ERRO: LED vermelho nao esta pronto\n");
		return -ENODEV;
	}

	/*
	 * Verifica LED verde.
	 */
	if (!gpio_is_ready_dt(&led_green)) {
		printk("ERRO: LED verde nao esta pronto\n");
		return -ENODEV;
	}

	/*
	 * Configura LED vermelho.
	 */
	ret = gpio_pin_configure_dt(
		&led_red,
		GPIO_OUTPUT_INACTIVE
	);

	if (ret < 0) {
		printk(
			"ERRO configurando LED vermelho: %d\n",
			ret
		);
		return ret;
	}

	/*
	 * Configura LED verde.
	 */
	ret = gpio_pin_configure_dt(
		&led_green,
		GPIO_OUTPUT_INACTIVE
	);

	if (ret < 0) {
		printk(
			"ERRO configurando LED verde: %d\n",
			ret
		);
		return ret;
	}

	leds_off();

	/*
	 * Inicializa nRF24.
	 */
	ret = nrf24_init();

	if (ret < 0) {
		printk("ERRO: nrf24_init() = %d\n", ret);
		return ret;
	}

	printk("nRF24 inicializado.\n");
	printk("Aguardando mensagens...\n");

	return 0;
}


int main(void)
{
	if (init() < 0) {
		return 0;
	}

	while (1) {
		process_packet();
	}

	return 0;
}
