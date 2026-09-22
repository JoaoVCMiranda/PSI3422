/*
 * debug_ponte_H_encoder_radio_controle
 *
 * Companheiro de debug_ponte_H_encoder_radio_carrinho — só rádio +
 * UART (SEM joystick: nem ADC, nem o botão de freio são lidos aqui,
 * de propósito, pra não misturar o defeito de hardware conhecido do
 * joystick — ver PENDENCIAS.md do Exp2 — com o que este firmware
 * quer isolar: será que o link nRF24 conecta de forma confiável?).
 *
 * w/a/s/d/x/espaço/q/o = mesmas teclas de projeto_final/Controle
 * (key_to_cmd); 'i' imprime a última telemetria recebida. Sem o ramo
 * de fallback pro joystick que existia em projeto_final/Controle —
 * aqui, sem tecla nova, o comando simplesmente PERSISTE (mantém o
 * último enviado) em vez de ser resetado a cada ciclo.
 *
 * LED_RED/LED_GREEN = mesmo indicador de conexão do
 * projeto_final/Controle (handshake + timeout de 2s + reconexão) —
 * é esse sinal que ficou difícil de interpretar no projeto_final
 * completo; aqui, isolado, deve ficar claro se/quando conecta.
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>
#include <sys/printk.h>

#include "nrf24.h"
#include "../../../projeto_final/protocol.h"

#define RADIO_CSN_PORT DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define RADIO_CSN_PIN  10   /* PTB10 */
#define RADIO_CE_PORT  DEVICE_DT_GET(DT_NODELABEL(gpioe))
#define RADIO_CE_PIN   31   /* PTE31 */
#define RADIO_IRQ_PORT DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define RADIO_IRQ_PIN  8    /* PTB8 */

#define LED_RED_PORT   DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define LED_RED_PIN    18   /* PTB18, active low */
#define LED_GREEN_PORT DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define LED_GREEN_PIN  19   /* PTB19, active low */

#define VELOCIDADE_PADRAO 16000
#define VELOCIDADE_GIRO   12000

/* Igual a projeto_final/Controle/src/main.c (sem o ramo de joystick). */
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
    printk("=== debug_ponte_H_encoder_radio_controle ===\n");
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
        /* sem tecla nova: cmd simplesmente persiste (sem fallback de joystick) */

        /*
         * Escuta ANTES de mandar comando (ordem invertida em relação a
         * projeto_final/Controle) — hipótese de que o link é
         * assimétrico: Carrinho fica em RX quase o tempo todo (só sai
         * pra mandar telemetria, rápido quando dá certo), enquanto
         * Controle antes só abria uma janela de escuta DEPOIS de
         * mandar comando — fração pequena do próprio ciclo (~20-40ms
         * nominal). Escutar primeiro, com janela mais larga (150ms >
         * o período de telemetria do Carrinho, tipicamente 50-100ms),
         * dá mais chance de pegar o pacote antes de voltar a
         * transmitir. Ainda é ping-pong sem relógio compartilhado, não
         * elimina o problema de raiz (ver ACK payload, considerado e
         * adiado) — só reequilibra as janelas de escuta dos dois
         * lados.
         */
        uint8_t rx_payload[NRF24_MAX_PAYLOAD_SIZE];
        ret = nrf24_receive(rx_payload, sizeof(rx_payload), K_MSEC(150));

        uint8_t tx_payload[NRF24_MAX_PAYLOAD_SIZE] = {0};
        *(radio_cmd_t *)tx_payload = cmd;
        nrf24_send(tx_payload, sizeof(tx_payload));

        if (ret >= 0) {
            last_telem = *(const radio_telemetry_t *)rx_payload;
            printk("dist_mock=%3ucm dutyL=%5d dutyR=%5d percorrida=%ucm\n",
                   last_telem.dist_cm, last_telem.duty_l, last_telem.duty_r, last_telem.dist_percorrida_cm);

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

        static int loop_cnt = 0;
        if (++loop_cnt % 25 == 0) { /* ~500ms */
            printk("heartbeat: tx_cmd = auto=%d freio=%d motL=%d motR=%d conectado=%d\n",
                   cmd.auto_mode, cmd.freio, cmd.motor_l, cmd.motor_r, handshake && radio_lost_time == 0);
        }

        k_msleep(20);
    }
}
