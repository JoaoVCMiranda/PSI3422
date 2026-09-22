/*
 * debug_ponte_H_control_curvas_distancia
 *
 * Passo 2 do debug gradual (depois de debug_ponte_H_encoder_radio_*):
 * motor+encoder+ultrassom juntos, SEM rádio — só a decisão de desvio
 * reativo (mesmas 4 faixas de projeto_final/Carrinho/lib/control_fsm)
 * só que dirigindo via lib/volante em vez de motor_set() direto nos
 * dois motores. Primeira vez que volante_direita()/volante_re()/
 * volante_para() rodam de verdade (volante_frente() já validado em
 * debug_ponte_H_encoder_radio_carrinho).
 *
 * O HC-SR04 já funciona bem (validado antes) — este firmware não está
 * testando o sensor, está testando se virar (pivô) + frear + dar ré
 * funcionam com o TRIM_L aplicado, e se a distância percorrida
 * continua sendo contada certo enquanto o carrinho manobra sozinho
 * (não só andando reto, como em debug_ponte_H_encoder_radio_carrinho).
 *
 * Sem rádio aqui de propósito — isolar "o carrinho decide sozinho e
 * dirige certo" de "o link de rádio está confiável" (ver
 * debug_ponte_H_encoder_radio_{carrinho,controle}). Junta os dois só
 * no merge final (ver projeto_final/README.md).
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/printk.h>

#include "motor.h"
#include "pwm_z42.h"
#include "ultrassom.h"
#include "encoder.h"
#include "odometria.h"
#include "volante.h"

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

/* Mesma calibração de projeto_final/Carrinho/src/main.c. */
#define DISTANCIA_ENTRE_RODAS_M 0.17f
#define PULSOS_POR_VOLTA 1
#define RODA_RAIO_M 0.035f
#define RODA_CIRCUNFERENCIA_M (2.0f * 3.14159265f * RODA_RAIO_M)

/* TRIM_L: validado em debug_ponte_H/debug_ponte_H_encoder — ver
 * lib/volante/volante.h. */
#define TRIM_L 3000
#define VELOCIDADE_AUTO 16000

/* Mesmas 3 faixas de projeto_final/Carrinho/lib/control_fsm/control_fsm.h
 * (sem incluir o header pra não arrastar radio_cmd_t/protocol.h aqui —
 * este firmware não usa rádio). Mudou uma faixa, muda as duas cópias. */
#define DISTANCIA_FRENTE_M 0.30f
#define DISTANCIA_CURVA_M  0.20f
#define DISTANCIA_PARADA_M 0.04f

static motor_t motor_l;
static motor_t motor_r;
static volante_t volante;
static ultrassom_t sensor;
static encoder_t encoder_r;
static odometria_pose_t pose;

void main(void)
{
    struct gpio_dt_spec l_in1 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpiod)), .pin = 0, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec l_in2 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpioc)), .pin = 9, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in1 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpioc)), .pin = 8, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in2 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpioa)), .pin = 5, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec trig  = { .port = DEVICE_DT_GET(DT_NODELABEL(gpiod)), .pin = 3, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec echo  = { .port = DEVICE_DT_GET(DT_NODELABEL(gpiod)), .pin = 1, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec enc_r = { .port = ENCODER_R_PORT, .pin = ENCODER_R_PIN, .dt_flags = GPIO_ACTIVE_HIGH };

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

    ret = ultrassom_init(&sensor, &trig, &echo);
    if (ret < 0) { printk("ERRO: ultrassom_init = %d\n", ret); return; }

    ret = encoder_init(&encoder_r, &enc_r, &motor_r);
    if (ret < 0) { printk("ERRO: encoder_init(R) = %d\n", ret); return; }

    odometria_calibracao_t odo_calib = {
        .circunferencia_roda_m = RODA_CIRCUNFERENCIA_M,
        .distancia_entre_rodas_m = DISTANCIA_ENTRE_RODAS_M,
        .pulsos_por_volta = PULSOS_POR_VOLTA,
    };
    odometria_init(&pose);

    printk("\n==================================\n");
    printk("=== debug_ponte_H_control_curvas_distancia ===\n");
    printk("==================================\n");
    printk("Desvio reativo autonomo, sem radio. Carrinho no chao, longe de gente.\n\n");

    for (;;) {
        float distancia = ultrassom_read(&sensor);
        const char *estado;

        if (distancia <= DISTANCIA_PARADA_M) {
            volante_para(&volante);
            estado = "PARA";
        } else if (distancia <= DISTANCIA_CURVA_M) {
            volante_re(&volante, VELOCIDADE_AUTO);
            estado = "RE";
        } else if (distancia <= DISTANCIA_FRENTE_M) {
            volante_direita(&volante, VELOCIDADE_AUTO);
            estado = "CURVA_DIREITA";
        } else {
            volante_frente(&volante, VELOCIDADE_AUTO);
            estado = "FRENTE";
        }

        int32_t delta_r = encoder_reset(&encoder_r);
        odometria_atualiza(&pose, &odo_calib, delta_r, delta_r);
        uint32_t dist_percorrida_cm = (uint32_t)(pose.distancia_percorrida_m * 100.0f);

        static int loop_cnt = 0;
        if (++loop_cnt % 10 == 0) { /* ~500ms */
            printk("ultrassom=%dmm estado=%-14s percorrida=%ucm\n",
                   (int)(distancia * 1000.0f), estado, dist_percorrida_cm);
        }

        k_msleep(50);
    }
}
