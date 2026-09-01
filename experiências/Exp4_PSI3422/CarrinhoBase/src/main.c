/*
 * PSI3422 — Exp4_PSI3422 / CarrinhoBase
 *
 * Aula 4, escopo desta entrega: só ponte H (motor_l/motor_r, lib
 * compartilhada `motor/`) + dois contadores de volta com sinal
 * (encoder_l/encoder_r, lib nova `encoder/` — ver lib/SPEC.md).
 * Calibração de distância e curvas de 90° ficam para depois; aqui só
 * se demonstra que o sinal do contador acompanha o sentido comandado
 * ao motor da mesma roda (frente soma, ré subtrai), conforme o
 * racional em lib/encoder/encoder.h.
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

static motor_t motor_l;
static motor_t motor_r;
static encoder_t encoder_l;
static encoder_t encoder_r;

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

    printk("\n=== Exp4_PSI3422 CarrinhoBase -- ponte H + contadores com sinal ===\n");
    printk("Levante o carrinho do chao. Frente 3s, re 3s, repete.\n");
    printk("Contador esperado: sobe em frente, desce em re.\n\n");

    bool indo_frente = true;

    for (;;) {
        int16_t duty = indo_frente ? DEMO_DUTY : -DEMO_DUTY;

        motor_set(&motor_l, duty);
        motor_set(&motor_r, duty);
        printk("-- %s (duty=%d) --\n", indo_frente ? "FRENTE" : "RE", duty);

        for (int t = 0; t < DEMO_LEG_MS; t += 1000) {
            k_msleep(1000);
            printk("pulsos: L=%d  R=%d\n", encoder_get(&encoder_l), encoder_get(&encoder_r));
        }

        indo_frente = !indo_frente;
    }
}
