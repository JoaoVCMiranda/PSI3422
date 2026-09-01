/*
 * PSI3422 — Exp2_PSI3422 / debug / AlinhamentoCheck
 *
 * Firmware isolado só pra validar o alinhamento mecânico das rodas:
 * os dois motores recebem o MESMO duty (sem PID, sem correção de
 * curva, sem control_fsm, sem rádio, sem ultrassom) e o carrinho deve
 * andar em linha reta se motores/rodas estiverem alinhados. Qualquer
 * desvio observado aqui é mecânico (folga, roda torta, atrito
 * desigual), não pode ser explicado por diferença de código entre os
 * dois lados — os dois recebem literalmente o mesmo valor.
 *
 * Pinmap idêntico ao Carrinho real (ver ../../Carrinho/src/main.c e
 * ../../Pinmap.md).
 *
 * Ciclo: anda ALINHAMENTO_DUTY por ALINHAMENTO_ANDA_MS, freia por
 * ALINHAMENTO_PAUSA_MS, repete — pra observar várias vezes sem
 * precisar resetar a placa. Coloque o carrinho no chão antes do
 * primeiro ciclo (~sem contagem regressiva; o log no serial avisa
 * quando cada fase começa).
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

#define ALINHAMENTO_DUTY     (INT16_MAX / 2) /* ~50%, mesmo valor do modo automático do control_fsm */
#define ALINHAMENTO_ANDA_MS  3000
#define ALINHAMENTO_PAUSA_MS 2000

static motor_t motor_l;
static motor_t motor_r;

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

    printk("\n=== AlinhamentoCheck -- teste de linha reta ===\n");
    printk("duty=%d nos dois motores, anda %dms, freia %dms, repete\n",
           ALINHAMENTO_DUTY, ALINHAMENTO_ANDA_MS, ALINHAMENTO_PAUSA_MS);
    printk("Observe se o carrinho desvia pra um lado -> desalinhamento mecanico\n\n");

    for (;;) {
        printk("-> andando\n");
        motor_set(&motor_l, ALINHAMENTO_DUTY);
        motor_set(&motor_r, ALINHAMENTO_DUTY);
        k_msleep(ALINHAMENTO_ANDA_MS);

        printk("-> freando\n");
        motor_freia(&motor_l);
        motor_freia(&motor_r);
        k_msleep(ALINHAMENTO_PAUSA_MS);
    }
}
