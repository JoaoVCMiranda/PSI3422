/*
 * PSI3422 — projeto_final / Monitoramento
 *
 * Terceiro board: só rádio nRF24L01+ em modo recepção. Não manda
 * nenhum radio_cmd_t (isso é papel do Controle) — só ouve a
 * telemetria (radio_telemetry_t, ver ../../protocol.h) que o
 * Carrinho já transmite a cada ciclo (~50ms) e imprime, sem
 * interpretar/agir sobre o conteúdo. Ver ../../protocol.h e
 * ../Carrinho/src/main.c pro formato do payload e o racional dos
 * campos.
 *
 * Pinos: ver ../../pinmap.h (gerado, ver ../../pinmap.yaml). Mesmos
 * pinos de rádio (CE/CSN/IRQ) e LEDs de status usados por
 * Carrinho/Controle.
 *
 * CAVEAT (não validado em bancada): endereço e canal do nRF24 são
 * fixos e únicos para todo o projeto (ver nrf24_address/RF_CH em
 * lib/nrf24/nrf24.c) e auto-ACK (EN_AA) está ligado no pipe 0. Com
 * este terceiro rádio também em RX no mesmo endereço/canal,
 * Monitoramento passa a responder com ACK a pacotes do Carrinho ao
 * mesmo tempo que o Controle — os dois ACKs podem colidir no ar e
 * fazer o Carrinho enxergar mais reenvios/timeouts do que enxergaria
 * só com o Controle. Não deveria derrubar o link (ACK é best-effort,
 * retransmissão já existe), mas está para confirmar em bancada com
 * os três rádios ligados ao mesmo tempo.
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/printk.h>

#include "nrf24.h"
#include "../../protocol.h"
#include "../../pinmap.h"

void main(void)
{
    struct gpio_dt_spec ce  = { .port = RADIO_CE_PORT, .pin = RADIO_CE_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec csn = { .port = RADIO_CSN_PORT, .pin = RADIO_CSN_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    struct gpio_dt_spec irq = { .port = RADIO_IRQ_PORT, .pin = RADIO_IRQ_PIN, .dt_flags = GPIO_ACTIVE_LOW };

    struct gpio_dt_spec led_red = { .port = LED_RED_PORT, .pin = LED_RED_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    struct gpio_dt_spec led_green = { .port = LED_GREEN_PORT, .pin = LED_GREEN_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    if (device_is_ready(led_red.port)) {
        gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    }

    printk("\n==================================\n");
    printk("=== BOOTING MONITORAMENTO APP  ===\n");
    printk("==================================\n");

    int ret = nrf24_init(&ce, &csn, &irq);
    if (ret < 0) {
        printk("ERRO: nrf24_init = %d. Monitoramento continuara tentando reconectar.\n", ret);
    }

    printk("PSI3422 projeto_final -- monitoramento pronto (so recepcao, nao transmite)\n\n");

    static radio_telemetry_t last_telem;
    bool handshake = false;
    int64_t radio_lost_time = 0;

    for (;;) {
        /* payload fixo de 32 bytes (ver nota em Carrinho/src/main.c);
         * radio_telemetry_t (protocol.h) so cobre o inicio dele. */
        uint8_t rx_payload[NRF24_MAX_PAYLOAD_SIZE];

        ret = nrf24_receive(rx_payload, sizeof(rx_payload), K_MSEC(100));
        if (ret >= 0) {
            last_telem = *(const radio_telemetry_t *)rx_payload;
            printk("dist=%3ucm dutyL=%5d dutyR=%5d percorrida=%ucm\n",
                   last_telem.dist_cm, last_telem.duty_l, last_telem.duty_r,
                   last_telem.dist_percorrida_cm);

            handshake = true;
            radio_lost_time = 0;
            gpio_pin_set_dt(&led_red, 0);
            gpio_pin_set_dt(&led_green, 1);
        } else {
            gpio_pin_set_dt(&led_red, 1);
            gpio_pin_set_dt(&led_green, 0);

            if (handshake) {
                if (radio_lost_time == 0) {
                    radio_lost_time = k_uptime_get();
                } else if (k_uptime_get() - radio_lost_time > 2000) {
                    printk("Reconectando modulo NRF24 do Monitoramento...\n");
                    nrf24_init(&ce, &csn, &irq);
                    radio_lost_time = k_uptime_get(); /* tenta de novo em 2s */
                }
            }
        }

        k_msleep(50);
    }
}
