/*
 * debug_ponte_H_encoder_radio_carrinho
 *
 * Passo 1 do debug gradual (retomando debug_ponte_H_encoder): mesma
 * bancada de motor+encoder (ponte H dupla, TRIM_L, IR HW-201 direita)
 * + o módulo nRF24 por cima, pra validar contagem de distância e link
 * de rádio JUNTOS antes de somar o ultrassom/control_fsm de volta.
 *
 * O que este firmware faz:
 *   - Gira os dois motores pra frente continuamente (mesmo teste de
 *     debug_ponte_H_encoder, agora via lib/volante — primeira vez que
 *     volante_t roda de verdade, ver lib/volante/volante.h).
 *   - Recebe radio_cmd_t do Controle e só IMPRIME (não aciona motor a
 *     partir do comando — o objetivo aqui é confirmar recepção, não
 *     testar controle manual).
 *   - Manda radio_telemetry_t (distância percorrida via odometria,
 *     duty dos motores) a cada ciclo, ultrassom MOCKADO (dist_cm =
 *     0xFFFF, "sem eco" — ver protocol.h; o HC-SR04 já funciona bem,
 *     não é isso que este firmware está testando).
 *   - LED_RED/LED_GREEN mostram o estado da conexão (mesma lógica de
 *     handshake/timeout/reconnect de projeto_final/Carrinho) — é
 *     exatamente esse sinal que ficou difícil de interpretar no
 *     projeto_final completo (muita coisa rodando junto); aqui isolado
 *     deve dar pra ver com clareza se/quando conecta.
 *   - isr_raw_count (callback bruto, independente do gate por
 *     motor->speed em encoder_isr) continua igual ao
 *     debug_ponte_H_encoder — a pergunta "zero pulsos" ainda está em
 *     aberto, e este é o primeiro teste com o rádio ativo ao mesmo
 *     tempo: serve também pra checar se SPI/IRQ do nRF24 rouba tempo
 *     de interrupção do encoder.
 *
 * Pinos hardcoded aqui (não inclui ../../projeto_final/pinmap.h) —
 * mesma convenção de debug_ponte_H/debug_ponte_H_encoder, canais TPM
 * já com o fix de MOTOR_L_ENA_CH/MOTOR_R_ENB_CH (ver comentário
 * original em debug_ponte_H/src/main.c). protocol.h É importado de
 * projeto_final/ (não duplicado) pra não divergir do struct real.
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/printk.h>

#include "motor.h"
#include "pwm_z42.h"
#include "encoder.h"
#include "odometria.h"
#include "volante.h"
#include "nrf24.h"
#include "../../../projeto_final/protocol.h"

#define TPM_MOTOR_MOD 3999U

/* MOTOR_L_ENA — TPM0_CH2, PTD2 (canal corrigido, ver debug_ponte_H) */
#define MOTOR_L_ENA_GPIO GPIOD
#define MOTOR_L_ENA_PIN  2
#define MOTOR_L_ENA_CH   2

/* MOTOR_R_ENB — TPM0_CH1, PTA4 (canal corrigido, ver debug_ponte_H) */
#define MOTOR_R_ENB_GPIO GPIOA
#define MOTOR_R_ENB_PIN  4
#define MOTOR_R_ENB_CH   1

#define ENCODER_R_PORT DEVICE_DT_GET(DT_NODELABEL(gpioa))
#define ENCODER_R_PIN  16   /* PTA16 */

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

/* Mesma calibração de projeto_final/Carrinho/src/main.c — ver
 * "Parâmetros do carrinho" em projeto_final/README.md. */
#define DISTANCIA_ENTRE_RODAS_M 0.17f
#define PULSOS_POR_VOLTA 1
#define RODA_RAIO_M 0.035f
#define RODA_CIRCUNFERENCIA_M (2.0f * 3.14159265f * RODA_RAIO_M)

/* TRIM_L: motor esquerdo mecanicamente mais fraco, validado em
 * debug_ponte_H/debug_ponte_H_encoder — ver lib/volante/volante.h. */
#define TRIM_L 3000
#define VELOCIDADE_TESTE 16000

static motor_t motor_l;
static motor_t motor_r;
static volante_t volante;
static encoder_t encoder_r;
static odometria_pose_t pose;

/* DIAGNÓSTICO — mesma dúvida em aberto de debug_ponte_H_encoder:
 * callback extra no pino do encoder, independente de motor->speed,
 * pra isolar se a interrupção dispara mesmo (ver encoder_isr em
 * lib/encoder/encoder.c). */
static volatile uint32_t isr_raw_count;
static struct gpio_callback cb_raw;

static void isr_raw(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    isr_raw_count++;
}

void main(void)
{
    struct gpio_dt_spec l_in1 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpiod)), .pin = 0, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec l_in2 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpioc)), .pin = 9, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in1 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpioc)), .pin = 8, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in2 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpioa)), .pin = 5, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec enc_r = { .port = ENCODER_R_PORT, .pin = ENCODER_R_PIN, .dt_flags = GPIO_ACTIVE_HIGH };

    struct gpio_dt_spec ce  = { .port = RADIO_CE_PORT, .pin = RADIO_CE_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec csn = { .port = RADIO_CSN_PORT, .pin = RADIO_CSN_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    struct gpio_dt_spec irq = { .port = RADIO_IRQ_PORT, .pin = RADIO_IRQ_PIN, .dt_flags = GPIO_ACTIVE_LOW };

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
    ret = motor_init(&motor_r, &r_in1, &r_in2, TPM0, MOTOR_R_ENB_CH, TPM_MOTOR_MOD);
    if (ret < 0) { printk("ERRO: motor_init(R) = %d\n", ret); return; }

    volante_init(&volante, &motor_l, &motor_r, TRIM_L);

    ret = encoder_init(&encoder_r, &enc_r, &motor_r);
    if (ret < 0) { printk("ERRO: encoder_init(R) = %d\n", ret); return; }

    gpio_init_callback(&cb_raw, isr_raw, BIT(enc_r.pin));
    ret = gpio_add_callback(enc_r.port, &cb_raw);
    if (ret < 0) { printk("ERRO: gpio_add_callback(raw) = %d\n", ret); return; }

    odometria_calibracao_t odo_calib = {
        .circunferencia_roda_m = RODA_CIRCUNFERENCIA_M,
        .distancia_entre_rodas_m = DISTANCIA_ENTRE_RODAS_M,
        .pulsos_por_volta = PULSOS_POR_VOLTA,
    };
    odometria_init(&pose);

    printk("\n==================================\n");
    printk("=== debug_ponte_H_encoder_radio_carrinho ===\n");
    printk("==================================\n");

    ret = nrf24_init(&ce, &csn, &irq);
    if (ret < 0) {
        printk("ERRO: nrf24_init = %d\n", ret);
    }

    printk("Motores pra frente (volante_frente, trim aplicado) — rodas fora do chao!\n");
    volante_frente(&volante, VELOCIDADE_TESTE);

    bool handshake = false;
    int64_t radio_lost_time = 0;

    for (;;) {
        uint8_t rx_payload[NRF24_MAX_PAYLOAD_SIZE];
        uint8_t tx_payload[NRF24_MAX_PAYLOAD_SIZE] = {0};

        ret = nrf24_receive(rx_payload, sizeof(rx_payload), K_MSEC(100));
        if (ret >= 0) {
            radio_cmd_t cmd = *(const radio_cmd_t *)rx_payload;
            printk("-> recebido (nao executado): auto=%d freio=%d motL=%d motR=%d apagar=%d\n",
                   cmd.auto_mode, cmd.freio, cmd.motor_l, cmd.motor_r, cmd.apagar_seq);

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

        int32_t delta_r = encoder_reset(&encoder_r);
        odometria_atualiza(&pose, &odo_calib, delta_r, delta_r);

        uint32_t dist_percorrida_cm = (uint32_t)(pose.distancia_percorrida_m * 100.0f);
        int16_t out_l = (int16_t)(motor_get_duty(&motor_l) * 1000.0f);
        int16_t out_r = (int16_t)(motor_get_duty(&motor_r) * 1000.0f);

        *(radio_telemetry_t *)tx_payload = (radio_telemetry_t){
            .dist_cm = 0xFFFF, /* ultrassom mockado — HC-SR04 ja validado, nao e o alvo deste teste */
            .duty_l = out_l, .duty_r = out_r,
            .dist_percorrida_cm = dist_percorrida_cm,
        };
        nrf24_send(tx_payload, sizeof(tx_payload));

        static int loop_cnt = 0;
        if (++loop_cnt % 10 == 0) { /* ~500ms */
            printk("pulsos_delta=%d isr_raw=%u percorrida=%ucm outL=%d outR=%d conectado=%d\n",
                   delta_r, isr_raw_count, dist_percorrida_cm, out_l, out_r, handshake && radio_lost_time == 0);
        }

        k_msleep(50);
    }
}
