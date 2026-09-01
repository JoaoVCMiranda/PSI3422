/*
 * PSI3422 — Exp2_PSI3422 / debug / DutySweepCheck
 *
 * Complementa AlinhamentoCheck: aquele manda o MESMO duty fixo pros
 * dois motores ao mesmo tempo; este testa CADA motor sozinho (o
 * outro fica freado) e sobe o duty aos poucos a partir de 0 — pra
 * achar, visual/auditivamente, o duty mínimo em que cada roda sai do
 * lugar (torque de arranque). Se os dois "arrancam" em valores bem
 * diferentes, é evidência objetiva — não só "parece mais fraca" — de
 * que motor/ponte H/fiação/atrito de um lado exige mais força que o
 * outro.
 *
 * Mesmo pinmap do Carrinho real (ver ../../Carrinho/src/main.c e
 * ../../Pinmap.md), só motor+PWM, sem ultrassom/rádio/control_fsm.
 *
 * Como usar: levante o carrinho do chão (rodas livres, sem carga).
 * Ciclo: freia os dois -> sobe duty do motor_l em degraus de
 * SWEEP_STEP a cada SWEEP_STEP_MS até SWEEP_MAX -> freia -> pausa ->
 * repete pro motor_r -> freia -> pausa -> repete tudo de novo.
 * Anote (serial monitor ou debug/monitor.py) o valor de duty impresso
 * no instante em que cada roda sai do lugar e compare L vs R.
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/printk.h>

#include "pwm_z42.h"
#include "motor.h"

/* ── Pinmap — mesmo do Carrinho, ver ../../Pinmap.md ── */
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

/* TPM0 compartilhado pelos 2 canais — mesma config do Carrinho real. */
#define TPM_MOTOR_MOD 3999U

#define SWEEP_STEP     1500
#define SWEEP_MAX      INT16_MAX
#define SWEEP_STEP_MS  1500
#define SWEEP_PAUSA_MS 2000

static motor_t motor_l;
static motor_t motor_r;

static void sweep_motor(const char *label, motor_t *motor, motor_t *outro)
{
    printk("\n-> sweep motor=%s (motor=%s freado)\n",
           label, label[0] == 'L' ? "R" : "L");
    motor_freia(outro);

    for (int32_t duty = 0; duty <= SWEEP_MAX; duty += SWEEP_STEP) {
        motor_set(motor, (int16_t)duty);
        printk("SWEEP motor=%s duty=%6d max=%d\n", label, (int)duty, SWEEP_MAX);
        k_msleep(SWEEP_STEP_MS);
    }

    motor_freia(motor);
    printk("-> fim do sweep motor=%s, freando\n", label);
}

void main(void)
{
    struct gpio_dt_spec l_in1 = { .port = MOTOR_L_IN1_PORT, .pin = MOTOR_L_IN1_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec l_in2 = { .port = MOTOR_L_IN1_PORT, .pin = MOTOR_L_IN2_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in1 = { .port = MOTOR_R_IN1_PORT, .pin = MOTOR_R_IN1_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in2 = { .port = MOTOR_R_IN2_PORT, .pin = MOTOR_R_IN2_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
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

    printk("\n=== DutySweepCheck -- torque de arranque por motor ===\n");
    printk("Levante o carrinho do chao (rodas livres) antes de comecar.\n");
    printk("Anote o duty onde CADA roda sai do lugar e compare L vs R.\n");
    printk("step=%d, %dms por degrau, max=%d\n\n", SWEEP_STEP, SWEEP_STEP_MS, SWEEP_MAX);

    for (;;) {
        sweep_motor("L", &motor_l, &motor_r);
        k_msleep(SWEEP_PAUSA_MS);

        sweep_motor("R", &motor_r, &motor_l);
        k_msleep(SWEEP_PAUSA_MS);
    }
}
