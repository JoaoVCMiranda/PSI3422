/*
 * debug_projeto_final — Carrinho
 *
 * Passo final do debug gradual: une debug_ponte_H_encoder_radio_carrinho
 * (motor+encoder+rádio) com debug_ponte_H_control_curvas_distancia
 * (ultrassom+decisão de 4 faixas via lib/volante), agora usando
 * ../../projeto_final/pinmap.h/protocol.h DE VERDADE em vez de pinos
 * hardcoded — a PCB foi feita em cima de pinmap.yaml, então este é o
 * primeiro firmware da série debug_ponte_H_* que roda no shield real.
 * Candidato a substituir projeto_final/Carrinho/src/main.c depois de
 * validado em bancada (ver "Divergências encontradas" abaixo).
 *
 * Diferenças em relação a projeto_final/Carrinho/src/main.c:
 *   - Desvio/freio/manual passam por lib/volante (volante_frente/re/
 *     direita/para/set) em vez de motor_set() direto — TRIM_L (motor
 *     esquerdo mais fraco) aplicado em toda manobra, não só na
 *     validação de bancada isolada.
 *   - control_fsm.c NÃO é usado aqui — a árvore de decisão (auto_mode
 *     > freio > manual) está inline, igual aos outros debug_ponte_H_*
 *     (mesma semântica de projeto_final/Carrinho/lib/control_fsm, só
 *     reescrita pra chamar volante_* em vez de motor_set()).
 *   - Um só ultrassom_read() por ciclo (fix já aplicado em
 *     control_fsm.c de produção — aqui nem existe o risco, a decisão
 *     usa a mesma leitura que vai pra telemetria).
 *
 * Divergências encontradas entre pinmap.yaml e o que os debug_ponte_H_*
 * anteriores tinham hardcoded (pedido explícito: reportar qualquer
 * divergência): NENHUMA. Motor L/R (IN1/IN2/ENA/ENB, canais TPM já
 * com o fix de MOTOR_L_ENA_CH/MOTOR_R_ENB_CH), ultrassom, encoder
 * direito, rádio (CE/CSN/IRQ) e LEDs batem pino a pino com
 * projeto_final/pinmap.yaml. O único ponto sem reconciliação
 * conhecida é o símbolo KiCad vs. o esquemático real da PCB — ver
 * projeto_final/kicad/README.md (não é pinmap.yaml, é o desenho da
 * placa; roteiro de verificação já documentado lá, não repetido
 * aqui).
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/printk.h>

#include "pwm_z42.h"
#include "motor.h"
#include "ultrassom.h"
#include "encoder.h"
#include "odometria.h"
#include "volante.h"
#include "nrf24.h"
#include "../../../projeto_final/protocol.h"
#include "../../../projeto_final/pinmap.h"

#define TPM_MOTOR_MOD 3999U

/* Mesma calibração de projeto_final/Carrinho/src/main.c — ver
 * "Parâmetros do carrinho" em projeto_final/README.md. */
#define DISTANCIA_ENTRE_RODAS_M 0.17f
#define PULSOS_POR_VOLTA 1
#define RODA_RAIO_M 0.035f
#define RODA_CIRCUNFERENCIA_M (2.0f * 3.14159265f * RODA_RAIO_M)

/* TRIM_L: validado em debug_ponte_H/debug_ponte_H_encoder — ver
 * lib/volante/volante.h. */
#define TRIM_L 3000
#define VELOCIDADE_AUTO (INT16_MAX / 2)

/* Mesmas 3 faixas de projeto_final/Carrinho/lib/control_fsm/control_fsm.h
 * — controle não usa o header pra não puxar radio_cmd_t indiretamente
 * duas vezes; se uma faixa mudar, muda as duas cópias. */
#define DISTANCIA_FRENTE_M 0.30f
#define DISTANCIA_CURVA_M  0.20f
#define DISTANCIA_PARADA_M 0.04f
#define CONTROL_TIMEOUT_MS 500

static motor_t motor_l;
static motor_t motor_r;
static volante_t volante;
static ultrassom_t sensor;
static encoder_t encoder_r;
static odometria_pose_t pose;

static int64_t last_heartbeat_ms;

void main(void)
{
    struct gpio_dt_spec l_in1 = { .port = MOTOR_L_IN1_PORT, .pin = MOTOR_L_IN1_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec l_in2 = { .port = MOTOR_L_IN2_PORT, .pin = MOTOR_L_IN2_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in1 = { .port = MOTOR_R_IN1_PORT, .pin = MOTOR_R_IN1_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in2 = { .port = MOTOR_R_IN2_PORT, .pin = MOTOR_R_IN2_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec trig  = { .port = ULTRASSOM_TRIG_PORT, .pin = ULTRASSOM_TRIG_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec echo  = { .port = ULTRASSOM_ECHO_PORT, .pin = ULTRASSOM_ECHO_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec enc_r = { .port = ENCODER_R_PORT, .pin = ENCODER_R_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec ce    = { .port = RADIO_CE_PORT, .pin = RADIO_CE_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec csn   = { .port = RADIO_CSN_PORT, .pin = RADIO_CSN_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    struct gpio_dt_spec irq   = { .port = RADIO_IRQ_PORT, .pin = RADIO_IRQ_PIN, .dt_flags = GPIO_ACTIVE_LOW };

    struct gpio_dt_spec led_red   = { .port = LED_RED_PORT, .pin = LED_RED_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    struct gpio_dt_spec led_green = { .port = LED_GREEN_PORT, .pin = LED_GREEN_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    if (device_is_ready(led_red.port)) {
        gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    }

    int ret;

    if (!pwm_tpm_Init(TPM0, TPM_MCGIRCLK, TPM_MOTOR_MOD, TPM_CLK, PS_1, EDGE_PWM)) {
        printk("ERRO: pwm_tpm_Init(TPM0) falhou\n"); return;
    }
    if (!pwm_tpm_Ch_Init(TPM0, MOTOR_L_ENA_CH, TPM_PWM_H, MOTOR_L_ENA_GPIO, MOTOR_L_ENA_PIN)) {
        printk("ERRO: pwm_tpm_Ch_Init(motor L) falhou\n"); return;
    }
    if (!pwm_tpm_Ch_Init(TPM0, MOTOR_R_ENB_CH, TPM_PWM_H, MOTOR_R_ENB_GPIO, MOTOR_R_ENB_PIN)) {
        printk("ERRO: pwm_tpm_Ch_Init(motor R) falhou\n"); return;
    }

    ret = motor_init(&motor_l, &l_in1, &l_in2, TPM0, MOTOR_L_ENA_CH, TPM_MOTOR_MOD);
    if (ret < 0) { printk("ERRO: motor_init(L) = %d\n", ret); return; }
    ret = motor_init(&motor_r, &r_in2, &r_in1, TPM0, MOTOR_R_ENB_CH, TPM_MOTOR_MOD);
    if (ret < 0) { printk("ERRO: motor_init(R) = %d\n", ret); return; }

    volante_init(&volante, &motor_l, &motor_r, TRIM_L);

    ret = ultrassom_init(&sensor, &trig, &echo);
    if (ret < 0) { printk("ERRO: ultrassom_init = %d\n", ret); return; }

    /* encoder_init depois de motor_init: a ISR do encoder lê motor->speed
     * (ver lib/encoder/encoder.c) — precisa existir antes, não precisa girar. */
    ret = encoder_init(&encoder_r, &enc_r, &motor_r);
    if (ret < 0) { printk("ERRO: encoder_init(R) = %d\n", ret); return; }

    odometria_calibracao_t odo_calib = {
        .circunferencia_roda_m = RODA_CIRCUNFERENCIA_M,
        .distancia_entre_rodas_m = DISTANCIA_ENTRE_RODAS_M,
        .pulsos_por_volta = PULSOS_POR_VOLTA,
    };
    odometria_init(&pose);

    printk("\n==================================\n");
    printk("=== debug_projeto_final -- Carrinho (pinmap.h real) ===\n");
    printk("==================================\n");

    ret = nrf24_init(&ce, &csn, &irq);
    if (ret < 0) {
        printk("ERRO: rádio falhou = %d. Carrinho continuará sem rádio.\n", ret);
    }

    radio_cmd_t cmd = { .auto_mode = 1 }; /* começa em RUN (modo seguro) */
    uint8_t last_apagar_seq = 0;
    last_heartbeat_ms = k_uptime_get();

    bool handshake = false;
    int64_t radio_lost_time = 0;

    for (;;) {
        uint8_t rx_payload[NRF24_MAX_PAYLOAD_SIZE];
        uint8_t tx_payload[NRF24_MAX_PAYLOAD_SIZE] = {0};

        ret = nrf24_receive(rx_payload, sizeof(rx_payload), K_MSEC(100));
        if (ret >= 0) {
            cmd = *(const radio_cmd_t *)rx_payload;
            last_heartbeat_ms = k_uptime_get();

            static int rx_cnt = 0;
            if (++rx_cnt % 10 == 0) {
                printk("-> recebido: auto=%d freio=%d motL=%d motR=%d\n",
                       cmd.auto_mode, cmd.freio, cmd.motor_l, cmd.motor_r);
            }
        }

        /* Watchdog: sem comando há mais de CONTROL_TIMEOUT_MS -> força RUN
         * (fallback de segurança por perda de conexão, mesma semântica de
         * control_fsm_watchdog() em projeto_final). */
        bool sem_radio = (k_uptime_get() - last_heartbeat_ms) > CONTROL_TIMEOUT_MS;
        if (sem_radio) {
            cmd.auto_mode = 1;
        }
        gpio_pin_set_dt(&led_red, sem_radio ? 1 : 0);
        gpio_pin_set_dt(&led_green, sem_radio ? 0 : 1);

        if (!sem_radio) {
            handshake = true;
            radio_lost_time = 0;
        } else if (handshake) {
            if (radio_lost_time == 0) {
                radio_lost_time = k_uptime_get();
            } else if (k_uptime_get() - radio_lost_time > 2000) {
                printk("Reconectando modulo NRF24 do Carrinho...\n");
                nrf24_init(&ce, &csn, &irq);
                radio_lost_time = k_uptime_get();
            }
        }

        /* ── Uma leitura de ultrassom por ciclo, reusada pela decisão
         * E pela telemetria (sem re-disparar o sensor, ver fix já
         * aplicado em control_fsm.c de produção). ── */
        float distancia = ultrassom_read(&sensor);
        const char *estado;

        if (cmd.auto_mode) {
            if (distancia <= DISTANCIA_PARADA_M) {
                volante_para(&volante); estado = "AUTO_PARA";
            } else if (distancia <= DISTANCIA_CURVA_M) {
                volante_re(&volante, VELOCIDADE_AUTO); estado = "AUTO_RE";
            } else if (distancia <= DISTANCIA_FRENTE_M) {
                volante_direita(&volante, VELOCIDADE_AUTO); estado = "AUTO_CURVA";
            } else {
                volante_frente(&volante, VELOCIDADE_AUTO); estado = "AUTO_FRENTE";
            }
        } else if (cmd.freio) {
            volante_para(&volante); estado = "FREIO";
        } else {
            volante_set(&volante, cmd.motor_l, cmd.motor_r); estado = "MANUAL";
        }

        /* ── Odometria: só ENCODER_R existe fisicamente, delta passado
         * pros dois parâmetros (ver comentário equivalente em
         * projeto_final/Carrinho/src/main.c — theta_rad fica 0,
         * distancia_percorrida_m continua correta). ── */
        int32_t delta_r = encoder_reset(&encoder_r);
        odometria_atualiza(&pose, &odo_calib, delta_r, delta_r);

        if (cmd.apagar_seq != last_apagar_seq) {
            last_apagar_seq = cmd.apagar_seq;
            odometria_init(&pose);
            printk("Distancia percorrida apagada (comando remoto).\n");
        }

        uint16_t dist_cm = (uint16_t)(distancia * 100.0f);
        int16_t out_l = (int16_t)(motor_get_duty(&motor_l) * 1000.0f);
        int16_t out_r = (int16_t)(motor_get_duty(&motor_r) * 1000.0f);
        uint32_t dist_percorrida_cm = (uint32_t)(pose.distancia_percorrida_m * 100.0f);

        *(radio_telemetry_t *)tx_payload = (radio_telemetry_t){
            .dist_cm = dist_cm, .duty_l = out_l, .duty_r = out_r,
            .dist_percorrida_cm = dist_percorrida_cm,
        };
        nrf24_send(tx_payload, sizeof(tx_payload));

        static int loop_cnt = 0;
        if (++loop_cnt % 10 == 0) { /* ~500ms */
            printk("estado=%-12s dist=%dcm outL=%d outR=%d percorrida=%ucm (sem_radio=%d)\n",
                   estado, dist_cm, out_l, out_r, dist_percorrida_cm, sem_radio);
        }

        k_msleep(50);
    }
}
