/*
 * debug_projeto_final — Controle
 *
 * Par do debug_projeto_final (Carrinho) — mesma ideia:
 * debug_ponte_H_encoder_radio_controle, agora com
 * ../../projeto_final/pinmap.h/protocol.h de verdade em vez de pinos
 * hardcoded (a PCB foi feita em cima de pinmap.yaml). Sem joystick de
 * propósito (defeito de hardware conhecido, ver PENDENCIAS.md do
 * Exp2 — nem ADC nem botão são lidos aqui).
 *
 * Mantém o fix de ordem do loop já validado em
 * debug_ponte_H_encoder_radio_controle: escuta ANTES de mandar
 * comando (Carrinho fica em RX quase o tempo todo; Controle antes só
 * abria uma janela de escuta pequena DEPOIS de transmitir — ver
 * comentário original nesse arquivo).
 *
 * Print de status deixa explícito os 3 valores pedidos: distância
 * percorrida, distância até obstáculo, e os comandos sendo enviados.
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>
#include <sys/printk.h>

#include "nrf24.h"
#include "../../../projeto_final/protocol.h"
#include "../../../projeto_final/pinmap.h"

#define VELOCIDADE_PADRAO 16000
#define VELOCIDADE_GIRO   12000

static bool key_to_cmd(uint8_t key, radio_cmd_t *cmd)
{
    switch (key) {
    case 'w': case 'W':
        cmd->auto_mode = false; cmd->freio = false;
        cmd->motor_l = VELOCIDADE_PADRAO; cmd->motor_r = VELOCIDADE_PADRAO;
        return true;
    case 's': case 'S':
        cmd->auto_mode = false; cmd->freio = false;
        cmd->motor_l = -VELOCIDADE_PADRAO; cmd->motor_r = -VELOCIDADE_PADRAO;
        return true;
    case 'a': case 'A':
        cmd->auto_mode = false; cmd->freio = false;
        cmd->motor_l = -VELOCIDADE_GIRO; cmd->motor_r = VELOCIDADE_GIRO;
        return true;
    case 'd': case 'D':
        cmd->auto_mode = false; cmd->freio = false;
        cmd->motor_l = VELOCIDADE_GIRO; cmd->motor_r = -VELOCIDADE_GIRO;
        return true;
    case 'q': case 'Q':
        cmd->auto_mode = false; cmd->freio = false;
        cmd->motor_l = 0; cmd->motor_r = 0;
        return true;
    case 'x': case 'X': case ' ':
        cmd->auto_mode = false; cmd->freio = true;
        return true;
    case 'o': case 'O':
        cmd->auto_mode = true; cmd->freio = false;
        return true;
    case 'c': case 'C':
        cmd->apagar_seq++;
        return true;
    default:
        return false;
    }
}

static const char *cmd_to_str(const radio_cmd_t *cmd)
{
    if (cmd->auto_mode) return "RUN (autonomo)";
    if (cmd->freio) return "STOP";
    if (cmd->motor_l == 0 && cmd->motor_r == 0) return "parado (q)";
    return "manual (w/a/s/d)";
}

void main(void)
{
    const struct device *console = DEVICE_DT_GET(DT_NODELABEL(uart0));

    struct gpio_dt_spec ce  = { .port = RADIO_CE_PORT, .pin = RADIO_CE_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec csn = { .port = RADIO_CSN_PORT, .pin = RADIO_CSN_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    struct gpio_dt_spec irq = { .port = RADIO_IRQ_PORT, .pin = RADIO_IRQ_PIN, .dt_flags = GPIO_ACTIVE_LOW };

    struct gpio_dt_spec led_red   = { .port = LED_RED_PORT, .pin = LED_RED_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    struct gpio_dt_spec led_green = { .port = LED_GREEN_PORT, .pin = LED_GREEN_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    if (device_is_ready(led_red.port)) {
        gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    }

    printk("\n==================================\n");
    printk("=== debug_projeto_final -- Controle (pinmap.h real) ===\n");
    printk("==================================\n");

    int ret = nrf24_init(&ce, &csn, &irq);
    if (ret < 0) {
        printk("ERRO: nrf24_init = %d\n", ret);
    }

    printk("w/a/s/d = mover, x/espaco = STOP, o = RUN, q = ponto morto\n");
    printk("i = mostrar distancia percorrida, c = apagar distancia\n\n");

    radio_cmd_t cmd = { .auto_mode = 1 };
    radio_telemetry_t last_telem = {0};
    bool handshake = false;
    int64_t radio_lost_time = 0;

    for (;;) {
        uint8_t key;
        if (uart_poll_in(console, &key) == 0) {
            if (key == 'i' || key == 'I') {
                printk("Distancia percorrida: %u cm\n", last_telem.dist_percorrida_cm);
            } else {
                key_to_cmd(key, &cmd);
            }
        }
        /* sem tecla nova: cmd persiste (sem fallback de joystick) */

        /* Escuta ANTES de mandar comando — ver comentário em
         * debug_ponte_H_encoder_radio_controle/src/main.c. */
        uint8_t rx_payload[NRF24_MAX_PAYLOAD_SIZE];
        ret = nrf24_receive(rx_payload, sizeof(rx_payload), K_MSEC(150));

        uint8_t tx_payload[NRF24_MAX_PAYLOAD_SIZE] = {0};
        *(radio_cmd_t *)tx_payload = cmd;
        nrf24_send(tx_payload, sizeof(tx_payload));

        if (ret >= 0) {
            last_telem = *(const radio_telemetry_t *)rx_payload;

            /* throttle pra não afogar o serial monitor (ciclo real é
             * ~20-170ms dependendo de quanto nrf24_receive espera) */
            static int print_cnt = 0;
            if (++print_cnt % 10 == 0) {
                if (last_telem.dist_cm == 0xFFFF) {
                    printk("obstaculo=sem_eco percorrida=%ucm comando=%s\n",
                           last_telem.dist_percorrida_cm, cmd_to_str(&cmd));
                } else {
                    printk("obstaculo=%ucm percorrida=%ucm comando=%s\n",
                           last_telem.dist_cm, last_telem.dist_percorrida_cm, cmd_to_str(&cmd));
                }
            }

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
                    printk("Reconectando modulo NRF24...\n");
                    nrf24_init(&ce, &csn, &irq);
                    radio_lost_time = k_uptime_get();
                }
            }
        }

        k_msleep(20);
    }
}
