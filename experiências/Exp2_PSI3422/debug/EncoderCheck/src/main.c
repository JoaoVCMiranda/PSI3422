/*
 * PSI3422 — Exp2_PSI3422 / debug / EncoderCheck
 *
 * Conta pulsos dos dois encoders IR HW-201 (um por roda) enquanto os
 * motores giram no mesmo duty fixo — dá o dado que falta pra separar
 * "parece mais fraca" / "anda torto" (AlinhamentoCheck,
 * DutySweepCheck) de uma medida real de rotação: pulsos por segundo
 * de cada roda no mesmo comando. Base também pra Aula 4 (calibrar
 * distância percorrida a partir da contagem de pulsos).
 *
 * HW-201: 3 pinos (VCC, GND, OUT digital — pulsa a cada vez que o
 * disco ranhurado da roda interrompe o feixe IR).
 *
 * AVISO IMPORTANTE sobre a escolha dos pinos: OUT_L em PTD1, OUT_R em
 * PTD3 — de propósito NÃO em PORTB/PORTE. Nesta subfamília KL25Z só
 * PORTA e PORTC/PORTD têm vetor de interrupção de pino; PORTB/PORTE
 * não geram IRQ por mudança de pino (limitação de silício, não bug de
 * configuração — foi exatamente o problema relatado ao tentar usar
 * gpioe como IRQ antes). PTD1/PTD3 ficam nos "buracos" entre os pinos
 * de PORTD já usados no Carrinho real (TRIG=PTD0, IRQ do rádio=PTD2,
 * ECHO=PTD4, motor R IN2=PTD5) — mesmo port, já comprovado como fonte
 * de interrupção por ECHO/IRQ, e já habilitado por padrão (gpiod é um
 * dos 3 ports default do board, sem overlay novo).
 *
 * Ainda não instalados fisicamente — pinos reservados em
 * ../../Pinmap.md antes de decidir a fiação de verdade, conforme pede
 * a Aula 3 do README ("defina os pinos antes de criar a placa").
 *
 * Como usar: levante o carrinho do chão (rodas livres). Os dois
 * motores ligam no mesmo duty fixo; a cada 1s imprime pulsos/s (PPS)
 * de cada roda. Se os PPS forem bem diferentes no mesmo duty, é
 * confirmação numérica de que uma roda gira mais devagar — motor,
 * ponte H, fiação ou atrito desigual (ver AlinhamentoCheck e
 * DutySweepCheck pra isolar qual).
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/printk.h>

#include "pwm_z42.h"
#include "motor.h"

/* ── Pinmap dos motores — mesmo do Carrinho, ver ../../Pinmap.md ── */
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
#define TEST_DUTY (INT16_MAX / 2) /* ~50%, mesmo valor usado no AlinhamentoCheck */

/* ── Encoders IR HW-201 (reservado, ver aviso no topo do arquivo) ── */
#define ENCODER_L_PORT DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define ENCODER_L_PIN  1   /* PTD1 */
#define ENCODER_R_PORT DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define ENCODER_R_PIN  3   /* PTD3 */

static motor_t motor_l;
static motor_t motor_r;

static struct gpio_dt_spec encoder_l;
static struct gpio_dt_spec encoder_r;
static struct gpio_callback encoder_l_cb;
static struct gpio_callback encoder_r_cb;
static volatile uint32_t encoder_l_count;
static volatile uint32_t encoder_r_count;

static void encoder_l_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    encoder_l_count++;
}

static void encoder_r_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    encoder_r_count++;
}

static int encoder_init(struct gpio_dt_spec *enc, struct gpio_callback *cb,
                         const struct device *port, uint8_t pin,
                         gpio_callback_handler_t handler)
{
    int ret;

    *enc = (struct gpio_dt_spec){ .port = port, .pin = pin, .dt_flags = GPIO_ACTIVE_HIGH };

    if (!device_is_ready(enc->port)) {
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(enc, GPIO_INPUT);
    if (ret < 0) {
        return ret;
    }

    gpio_init_callback(cb, handler, BIT(enc->pin));
    ret = gpio_add_callback(enc->port, cb);
    if (ret < 0) {
        return ret;
    }

    return gpio_pin_interrupt_configure_dt(enc, GPIO_INT_EDGE_RISING);
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

    ret = encoder_init(&encoder_l, &encoder_l_cb, ENCODER_L_PORT, ENCODER_L_PIN, encoder_l_isr);
    if (ret < 0) { printk("ERRO: encoder_init(L) = %d\n", ret); return; }

    ret = encoder_init(&encoder_r, &encoder_r_cb, ENCODER_R_PORT, ENCODER_R_PIN, encoder_r_isr);
    if (ret < 0) { printk("ERRO: encoder_init(R) = %d\n", ret); return; }

    printk("\n=== EncoderCheck -- pulsos por segundo (PPS) por roda ===\n");
    printk("Levante o carrinho do chao. Motores no duty fixo=%d.\n", TEST_DUTY);
    printk("Compare PPS de L vs R -- medida real de rotacao, nao so visual.\n\n");

    motor_set(&motor_l, TEST_DUTY);
    motor_set(&motor_r, TEST_DUTY);

    for (;;) {
        k_msleep(1000);

        uint32_t l = encoder_l_count;
        uint32_t r = encoder_r_count;
        encoder_l_count = 0;
        encoder_r_count = 0;

        printk("PPS: L=%4u  R=%4u\n", l, r);
    }
}
