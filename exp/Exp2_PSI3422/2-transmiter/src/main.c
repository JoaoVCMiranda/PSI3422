#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdint.h>

#include "nrf24.h"

#define PAYLOAD_SIZE 32

#define CMD_RED     'R'
#define CMD_GREEN   'G'
#define CMD_OFF     'O'

static const struct device *uart_dev =
	DEVICE_DT_GET(DT_CHOSEN(zephyr_console));


static uint8_t key_capture(uint8_t key)
{
	switch (key) {

	case 'q':
	case 'Q':
		printk("Tecla: VERMELHO\n");
		return CMD_RED;

	case 'w':
	case 'W':
		printk("Tecla: VERDE\n");
		return CMD_GREEN;

	case 'e':
	case 'E':
		printk("Tecla: DESLIGADO\n");
		return CMD_OFF;

	default:
		printk("Tecla '%c' ignorada\n", key);
		return 0;
	}
}


static void make_payload(uint8_t *payload, uint8_t command)
{
	memset(payload, 0, PAYLOAD_SIZE);

	payload[0] = command;
}


static int init(void)
{
	int ret;

	printk("\n");
	printk("=============================\n");
	printk(" nRF24 - TRANSMISSOR\n");
	printk("=============================\n");

	/*
	 * Inicializa UART.
	 */
	if (!device_is_ready(uart_dev)) {
		printk("ERRO: UART nao esta pronta\n");
		return -ENODEV;
	}

	/*
	 * Inicializa nRF24.
	 */
	ret = nrf24_init();

	if (ret < 0) {
		printk("ERRO: nrf24_init() = %d\n", ret);
		return ret;
	}

	printk("nRF24 inicializado.\n");

	printk("\n");
	printk("Comandos:\n");
	printk("  q -> vermelho\n");
	printk("  w -> verde\n");
	printk("  e -> desligado\n");
	printk("\n");
	printk("Aguardando tecla...\n");

	return 0;
}


int main(void)
{
	int ret;
	uint8_t key;
	uint8_t command;
	uint8_t payload[PAYLOAD_SIZE];

	ret = init();

	if (ret < 0) {
		return ret;
	}

	while (1) {

		if (uart_poll_in(uart_dev, &key) == 0) {

			command = key_capture(key);

			if (command == 0) {
				continue;
			}

			make_payload(payload, command);

			ret = nrf24_send(
				payload,
				PAYLOAD_SIZE
			);

			if (ret == 0) {
				printk("Transmissao OK\n");
			} else {
				printk(
					"ERRO na transmissao: %d\n",
					ret
				);
			}
		}

		k_sleep(K_MSEC(10));
	}

	return 0;
}