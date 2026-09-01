#ifndef PROJETO_FINAL_PINMAP_H_
#define PROJETO_FINAL_PINMAP_H_

/*
 * GERADO por tools/gen_pinmap.py a partir de pinmap.yaml — não
 * editar à mão. Editar pinmap.yaml e rodar o gerador de novo.
 *
 * Inclui #define de todo pino usado por Carrinho e/ou Controle;
 * cada projeto usa só o subconjunto que precisa (ver pinmap.yaml,
 * campo `boards`, e Pinmap.md pra tabela por board). Pinos
 * tipo=doc (SPI/UART fixos) não geram define, só documentação.
 */

#include <device.h>

/* MOTOR_L_IN1 — Ponte H, direção do motor esquerdo (lib/motor) [carrinho] */
#define MOTOR_L_IN1_PORT DEVICE_DT_GET(DT_NODELABEL(gpioc))
#define MOTOR_L_IN1_PIN  8   /* PTC8 */

/* MOTOR_L_IN2 — Ponte H, direção do motor esquerdo (lib/motor) [carrinho] */
#define MOTOR_L_IN2_PORT DEVICE_DT_GET(DT_NODELABEL(gpioc))
#define MOTOR_L_IN2_PIN  9   /* PTC9 */

/* MOTOR_L_ENA — TPM0_CH1, velocidade do motor esquerdo, via lib/pwm_z42 [carrinho] */
#define MOTOR_L_ENA_GPIO GPIOA
#define MOTOR_L_ENA_PIN  4   /* PTA4 = TPM0_CH1 */
#define MOTOR_L_ENA_CH   1

/* MOTOR_R_IN1 — Ponte H, direção do motor direito (lib/motor) [carrinho] */
#define MOTOR_R_IN1_PORT DEVICE_DT_GET(DT_NODELABEL(gpioa))
#define MOTOR_R_IN1_PIN  12   /* PTA12 */

/* MOTOR_R_IN2 — Ponte H, direção do motor direito (lib/motor) [carrinho] */
#define MOTOR_R_IN2_PORT DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define MOTOR_R_IN2_PIN  5   /* PTD5 */

/* MOTOR_R_ENB — TPM0_CH2, velocidade do motor direito, via lib/pwm_z42 [carrinho] */
#define MOTOR_R_ENB_GPIO GPIOA
#define MOTOR_R_ENB_PIN  5   /* PTA5 = TPM0_CH2 */
#define MOTOR_R_ENB_CH   2

/* ULTRASSOM_TRIG — HC-SR04 trigger (lib/ultrassom) [carrinho] */
#define ULTRASSOM_TRIG_PORT DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define ULTRASSOM_TRIG_PIN  0   /* PTD0 */

/* ULTRASSOM_ECHO — HC-SR04 echo, interrupção de borda (lib/ultrassom) [carrinho] */
#define ULTRASSOM_ECHO_PORT DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define ULTRASSOM_ECHO_PIN  4   /* PTD4 */

/* ENCODER_L — IR HW-201 esquerda (lib/encoder) [carrinho] */
#define ENCODER_L_PORT DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define ENCODER_L_PIN  1   /* PTD1 */

/* ENCODER_R — IR HW-201 direita (lib/encoder) [carrinho] */
#define ENCODER_R_PORT DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define ENCODER_R_PIN  3   /* PTD3 */

/* RADIO_CSN — chip-select manual do nRF24 (lib/nrf24) [carrinho+controle] */
#define RADIO_CSN_PORT DEVICE_DT_GET(DT_NODELABEL(gpioc))
#define RADIO_CSN_PIN  4   /* PTC4 */

/* RADIO_CE — lib/nrf24 [carrinho+controle] */
#define RADIO_CE_PORT DEVICE_DT_GET(DT_NODELABEL(gpioa))
#define RADIO_CE_PIN  13   /* PTA13 */

/* RADIO_IRQ — ativo em LOW (lib/nrf24) [carrinho+controle] */
#define RADIO_IRQ_PORT DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define RADIO_IRQ_PIN  2   /* PTD2 */

/* LED_RED — active low [carrinho+controle] */
#define LED_RED_PORT DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define LED_RED_PIN  18   /* PTB18 */

/* LED_GREEN — active low [carrinho+controle] */
#define LED_GREEN_PORT DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define LED_GREEN_PIN  19   /* PTB19 */

/* JOYSTICK_X — ADC0_SE8 — defeito de hardware conhecido, ver PENDENCIAS.md [controle] */
#define JOYSTICK_X_CHANNEL 8   /* PTB0 = ADC0_SE8 */

/* JOYSTICK_Y — ADC0_SE9 [controle] */
#define JOYSTICK_Y_CHANNEL 9   /* PTB1 = ADC0_SE9 */

/* JOYSTICK_BTN — botão de freio, pull-up interno, active low [controle] */
#define JOYSTICK_BTN_PORT DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define JOYSTICK_BTN_PIN  2   /* PTB2 */

#endif /* PROJETO_FINAL_PINMAP_H_ */
