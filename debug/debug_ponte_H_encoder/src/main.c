#include <zephyr.h>
#include <drivers/gpio.h>

#include "motor.h"
#include "pwm_z42.h"
#include "encoder.h"

/* TPM0 compartilhado pelos 2 canais de PWM dos motores: MCGIRCLK (4MHz,
 * independente do PLL) / PS_1 -> f_tpm = 4MHz; MOD=3999 -> f_pwm = 1kHz
 * (audível mas comum e seguro para motor DC via L298N). */
#define TPM_MOTOR_MOD 3999U

/* MOTOR_L_ENA — TPM0_CH2, velocidade do motor esquerdo, via lib/pwm_z42 [carrinho]
 * (canal físico confirmado em hal_nxp/dts/nxp/kinetis/MKL25Z128VLK4-pinctrl.dtsi:
 * "TPM0_CH2_PTD2" — o roteamento pino->canal é fixo no silício, não escolhido
 * pelo código; MOTOR_L_ENA_CH/MOTOR_R_ENB_CH estavam trocados, cruzando o PWM
 * de velocidade entre os dois motores). */
#define MOTOR_L_ENA_GPIO GPIOD
#define MOTOR_L_ENA_PIN  2   /* PTD2 = TPM0_CH2 */
#define MOTOR_L_ENA_CH   2

/* MOTOR_R_ENB — TPM0_CH1, velocidade do motor direito, via lib/pwm_z42 [carrinho]
 * ("TPM0_CH1_PTA4" no mesmo pinctrl.dtsi). */
#define MOTOR_R_ENB_GPIO GPIOA
#define MOTOR_R_ENB_PIN  4   /* PTA4 = TPM0_CH1 */
#define MOTOR_R_ENB_CH   1


#define ENCODER_R_PORT DEVICE_DT_GET(DT_NODELABEL(gpioa))
#define ENCODER_R_PIN  16   /* PTA16 */


#define DISTANCIA_ENTRE_RODAS_M 0.17f
#define PULSOS_POR_VOLTA 7
#define RODA_RAIO_M 0.035f
#define RODA_CIRCUNFERENCIA_M (2.0f * 3.14159265f * RODA_RAIO_M)

static encoder_t encoder_r;

/* DIAGNÓSTICO — remover depois de resolver o "zero pulsos".
 * Callback extra no mesmo pino do encoder_r, sem depender de
 * motor_r->speed: isola se a interrupção do GPIO dispara mesmo
 * (isr_raw_count) do bug de "conta zero porque motor->speed==0 no
 * instante do pulso" (ver encoder_isr em lib/encoder/encoder.c). */
static volatile uint32_t isr_raw_count;
static struct gpio_callback cb_raw;

static void isr_raw(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    isr_raw_count++;
}


static motor_t motor_l;
static motor_t motor_r;

void main(){
    struct gpio_dt_spec l_in1 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpiod)), .pin = 0, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec l_in2 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpioc)), .pin = 9, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in1 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpioc)), .pin = 8, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in2 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpioa)), .pin = 5, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec enc_r = { .port = ENCODER_R_PORT, .pin = ENCODER_R_PIN, .dt_flags = GPIO_ACTIVE_HIGH };

    /* TPM0 uma vez só — MOD (período) é compartilhado por todos os canais */
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

    int ret;
    ret = motor_init(&motor_l, &l_in1, &l_in2, TPM0, MOTOR_L_ENA_CH, TPM_MOTOR_MOD);
    if (ret < 0) { printk("ERRO: motor_init(L) = %d\n", ret); return; }

    ret = motor_init(&motor_r, &r_in1, &r_in2, TPM0, MOTOR_R_ENB_CH, TPM_MOTOR_MOD);
    if (ret < 0) { printk("ERRO: motor_init(R) = %d\n", ret); return; }

    // Não será calculado por motor->speed, a referência ao motor só serve para identificar se é frente ou ré
    // vamos fazer o cálculo de distância percorrida apenas pelo motor direito e sua geometria(quantidade de voltas)
    ret = encoder_init(&encoder_r, &enc_r, &motor_r);
    if (ret < 0) { printk("ERRO: encoder_init(R) = %d\n", ret); return; }

    /* DIAGNÓSTICO — mesmo pino, mesma borda (RISING) já configurada por
     * encoder_init acima; só soma um callback extra que não depende de
     * motor_r->speed. */
    gpio_init_callback(&cb_raw, isr_raw, BIT(enc_r.pin));
    ret = gpio_add_callback(enc_r.port, &cb_raw);
    if (ret < 0) { printk("ERRO: gpio_add_callback(raw) = %d\n", ret); return; }


    /* Velocidade de bancada: ~50% (16000/32767), suficiente pra girar sem
     * susto/corrente alta enquanto valida sentido de giro e fiação. Antes
     * de ligar, deixe o carrinho apoiado com as rodas fora do chão.
     *
     * v_teste_l > v_teste_r: motor esquerdo é mecanicamente mais fraco
     * (confirmado trocando os motores de canal — a fraqueza seguiu o
     * motor, não a fiação), precisa de mais duty pra girar na mesma
     * velocidade do direito. Ajuste TRIM_L empiricamente: aumente até as
     * rodas parecerem girar no mesmo ritmo com ESTADO_AMBOS_FRENTE. */
    const int16_t v_teste_r = 16000;
    const int16_t TRIM_L = 3000;
    const int16_t v_teste_l = v_teste_r + TRIM_L;

    /* Estado ESTÁTICO pra sondagem com apenas frente e ré no terminal gostaria de ver a distância percorrida pelo motor direito */
#define ESTADO_L_FRENTE      1
#define ESTADO_R_FRENTE      2
#define ESTADO_AMBOS_FRENTE  3
#define ESTADO_TESTE  ESTADO_AMBOS_FRENTE  /* <-- troque aqui e reflasheie */

    switch (ESTADO_TESTE) {
    case ESTADO_L_FRENTE:
        printk("Estado: L frente\n");
        motor_set(&motor_l, v_teste_l);
        break;
    case ESTADO_R_FRENTE:
        printk("Estado: R frente\n");
        motor_set(&motor_r, v_teste_r);
        break;
    case ESTADO_AMBOS_FRENTE:
        printk("Estado: ambos frente\n");
        motor_set(&motor_l, v_teste_l);
        motor_set(&motor_r, v_teste_r);
        break;
    default:
        printk("Estado: parado\n");
        break;
    }

    /* Distância = (pulsos / pulsos_por_volta) * circunferência da roda.
     * pulsos já vem com sinal (motor_r.speed no instante do pulso, ver
     * encoder.c) — frente soma, ré subtrai, então distancia_m também é
     * assinada (percurso líquido, não caminho percorrido). */
    for (;;) {
        k_msleep(500);

        int32_t pulsos = encoder_get(&encoder_r);
        float distancia_m = ((float)pulsos / PULSOS_POR_VOLTA) * RODA_CIRCUNFERENCIA_M;
        int nivel = gpio_pin_get_dt(&enc_r);

        printk("encoder_r: pulsos=%d distancia=%d mm | isr_raw=%u nivel_pino=%d\n",
               pulsos, (int)(distancia_m * 1000.0f), isr_raw_count, nivel);
    }
}
