/*
 * PSI3422 — Exp4_PSI3422 / CarrinhoBase
 *
 * Aula 4: ponte H (motor_l/motor_r, lib `motor/`) + dois contadores
 * de volta com sinal (encoder_l/encoder_r, lib nova `encoder/`) +
 * odometria diferencial (lib nova `odometria/`) — ângulo girado e
 * distância percorrida a partir dos dois contadores, a "questão de
 * programação competitiva" do roteiro. Ver lib/SPEC.md pras três.
 * Curvas de 90° calibradas ficam para depois (já dá pra saber o
 * ângulo girado via odometria_pose_t.theta_rad, falta só o controle
 * de motor que para ao atingir o ângulo alvo).
 *
 * Calibração medida em bancada (ver control/relatorio-aula-4.md,
 * "Calibragem Inicial e Estudos Pendentes"): raio da roda = 5 cm
 * (circunferência = 2*pi*0,05 ≈ 0,31416 m) e distância entre rodas =
 * 20 cm. Só falta confirmar PULSOS_POR_VOLTA (assumido 1, marco de
 * papel na roda) girando a roda manualmente N voltas em bancada —
 * com isso, tanto theta_rad quanto distancia_percorrida_m já usam
 * constantes medidas, não mais placeholder.
 *
 * Pinagem: ver ../pinmap.md. Mesmo pinmap de motores da
 * Exp2_PSI3422/Carrinho; encoders em PTD1 (L) / PTD3 (R), já
 * reservados e validados em debug/EncoderCheck daquele experimento.
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/printk.h>
#include <stdbool.h>

#include "pwm_z42.h"
#include "motor.h"
#include "encoder.h"
#include "odometria.h"

/* ── Pinmap dos motores — ver ../pinmap.md ── */
#define MOTOR_L_IN1_PORT DEVICE_DT_GET(DT_NODELABEL(gpioc))
#define MOTOR_L_IN1_PIN  8   /* PTC8 */
#define MOTOR_L_IN2_PIN  9   /* PTC9 */
#define MOTOR_L_ENA_GPIO GPIOA
#define MOTOR_L_ENA_PIN  4   /* PTA4 = TPM0_CH1 */
#define MOTOR_L_ENA_CH   1

#define MOTOR_R_IN1_PORT DEVICE_DT_GET(DT_NODELABEL(gpioa))
#define MOTOR_R_IN1_PIN  12  /* PTA12 */
#define MOTOR_R_IN2_PORT DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define MOTOR_R_IN2_PIN  5   /* PTD5 */
#define MOTOR_R_ENB_GPIO GPIOA
#define MOTOR_R_ENB_PIN  5   /* PTA5 = TPM0_CH2 */
#define MOTOR_R_ENB_CH   2

#define TPM_MOTOR_MOD 3999U

/* ── Encoders IR HW-201 — ver ../pinmap.md ── */
#define ENCODER_L_PORT DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define ENCODER_L_PIN  1   /* PTD1 */
#define ENCODER_R_PORT DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define ENCODER_R_PIN  3   /* PTD3 */

#define DEMO_DUTY (INT16_MAX / 2) /* ~50%, mesmo valor usado no debug/EncoderCheck da Exp2 */
#define DEMO_LEG_MS 3000          /* duração de cada trecho frente/ré */

/* ── Calibração da odometria — ver lib/odometria/odometria.h ── */
#define DISTANCIA_ENTRE_RODAS_M 0.20f /* medida em bancada, ver control/relatorio-aula-4.md */
#define PULSOS_POR_VOLTA 1            /* marco de papel fixado na roda, a confirmar em bancada */
#define RODA_RAIO_M 0.05f /* medido em bancada: 5 cm de raio */
#define RODA_CIRCUNFERENCIA_M (2.0f * 3.14159265f * RODA_RAIO_M) /* 2*pi*raio, calculada a partir da medida acima — só constante literal, sem libm/<math.h> */

static motor_t motor_l;
static motor_t motor_r;
static encoder_t encoder_l;
static encoder_t encoder_r;
static odometria_pose_t pose;

void main(void)
{
    struct gpio_dt_spec l_in1 = { .port = MOTOR_L_IN1_PORT, .pin = MOTOR_L_IN1_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec l_in2 = { .port = MOTOR_L_IN1_PORT, .pin = MOTOR_L_IN2_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in1 = { .port = MOTOR_R_IN1_PORT, .pin = MOTOR_R_IN1_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in2 = { .port = MOTOR_R_IN2_PORT, .pin = MOTOR_R_IN2_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec enc_l = { .port = ENCODER_L_PORT, .pin = ENCODER_L_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec enc_r = { .port = ENCODER_R_PORT, .pin = ENCODER_R_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    int ret;

    if (!pwm_tpm_Init(TPM0, TPM_MCGIRCLK, TPM_MOTOR_MOD, TPM_CLK, PS_1, EDGE_PWM)) {
        printk("ERRO: pwm_tpm_Init(TPM0) falhou\n");
        return;
    }
    if (!pwm_tpm_Ch_Init(TPM0, MOTOR_L_ENA_CH, TPM_PWM_H, MOTOR_L_ENA_GPIO, MOTOR_L_ENA_PIN)) {
        printk("ERRO: pwm_tpm_Ch_Init(motor L) falhou\n");
        return;
    }
    if (!pwm_tpm_Ch_Init(TPM0, MOTOR_R_ENB_CH, TPM_PWM_H, MOTOR_R_ENB_GPIO, MOTOR_R_ENB_PIN)) {
        printk("ERRO: pwm_tpm_Ch_Init(motor R) falhou\n");
        return;
    }

    ret = motor_init(&motor_l, &l_in1, &l_in2, TPM0, MOTOR_L_ENA_CH, TPM_MOTOR_MOD);
    if (ret < 0) { printk("ERRO: motor_init(L) = %d\n", ret); return; }

    ret = motor_init(&motor_r, &r_in1, &r_in2, TPM0, MOTOR_R_ENB_CH, TPM_MOTOR_MOD);
    if (ret < 0) { printk("ERRO: motor_init(R) = %d\n", ret); return; }

    /* encoder_init depois de motor_init: a ISR do encoder lê motor->speed,
     * então o motor associado precisa já existir (não precisa girar ainda). */
    ret = encoder_init(&encoder_l, &enc_l, &motor_l);
    if (ret < 0) { printk("ERRO: encoder_init(L) = %d\n", ret); return; }

    ret = encoder_init(&encoder_r, &enc_r, &motor_r);
    if (ret < 0) { printk("ERRO: encoder_init(R) = %d\n", ret); return; }

    odometria_calibracao_t calib = {
        .circunferencia_roda_m = RODA_CIRCUNFERENCIA_M,
        .distancia_entre_rodas_m = DISTANCIA_ENTRE_RODAS_M,
        .pulsos_por_volta = PULSOS_POR_VOLTA,
    };
    odometria_init(&pose);

    printk("\n=== Exp4_PSI3422 CarrinhoBase -- ponte H + contadores com sinal + odometria ===\n");
    printk("Levante o carrinho do chao. Frente 3s, re 3s, repete.\n");
    printk("Contador esperado: sobe em frente, desce em re.\n");
    printk("Calibracao: raio roda=5cm (circ=%dmm), dist.entre rodas=%dmm, pulsos/volta=%d (a confirmar em bancada).\n\n",
           (int)(RODA_CIRCUNFERENCIA_M * 1000.0f), (int)(DISTANCIA_ENTRE_RODAS_M * 1000.0f), PULSOS_POR_VOLTA);

    bool indo_frente = true;

    for (;;) {
        int16_t duty = indo_frente ? DEMO_DUTY : -DEMO_DUTY;

        motor_set(&motor_l, duty);
        motor_set(&motor_r, duty);
        printk("-- %s (duty=%d) --\n", indo_frente ? "FRENTE" : "RE", duty);

        for (int t = 0; t < DEMO_LEG_MS; t += 1000) {
            k_msleep(1000);

            int32_t delta_l = encoder_reset(&encoder_l);
            int32_t delta_r = encoder_reset(&encoder_r);
            odometria_atualiza(&pose, &calib, delta_l, delta_r);

            printk("pulsos: L=%d  R=%d | theta=%d mrad | dist.percorrida~=%d mm\n",
                   delta_l, delta_r,
                   (int)(pose.theta_rad * 1000.0f),
                   (int)(pose.distancia_percorrida_m * 1000.0f));
        }

        indo_frente = !indo_frente;
    }
}
