#ifndef MOTOR_H_
#define MOTOR_H_

#include <zephyr.h>
#include <drivers/gpio.h>
#include <stdint.h>

#include "pwm_z42.h"

/*
 * Motor DC controlado por ponte H (L298N ou similar): 2 pinos de
 * direção (in1/in2, GPIO Zephyr nativo) + 1 canal PWM de velocidade
 * (enA/enB, via pwm_z42/TPM — ver comentário longo em motor.c sobre
 * por que não é a API de PWM do Zephyr).
 *
 * Tabela de estados do L298N (Frente, Reverso, Morto, Freio):
 *   Frente = in1=1, in2=0, enA=duty
 *   Reverso = in1=0, in2=1, enA=duty
 *   Morto (coast)       = in1=0, in2=0
 *   Freio (short brake) = in1=1, in2=1
 */
typedef struct {
    struct gpio_dt_spec in1;
    struct gpio_dt_spec in2;

    TPM_MemMapPtr tpm;     /* TPM0/TPM1/TPM2 — ver pwm_z42.h */
    uint16_t tpm_channel;  /* canal dentro do TPM acima */
    uint16_t tpm_mod;      /* período (TPM_MOD) já configurado por quem inicializou o TPM */

    int16_t speed; /* último valor aplicado, para motor_get_duty() */
} motor_t;

/*
 * gpio_in1/gpio_in2: pinos de direção (GPIO comum).
 * tpm/tpm_channel: canal TPM já inicializado com pwm_tpm_Init() +
 *   pwm_tpm_Ch_Init() por quem chama (o período é compartilhado entre
 *   os dois canais de um mesmo TPM, então faz mais sentido inicializar
 *   o TPM uma vez em main.c do que dentro de cada motor_init()).
 * tpm_mod: o mesmo valor de MOD passado para pwm_tpm_Init() — motor_set()
 *   precisa dele para converter -32768..32767 em CnV.
 */
int motor_init(motor_t *motor,
                const struct gpio_dt_spec *in1,
                const struct gpio_dt_spec *in2,
                TPM_MemMapPtr tpm, uint16_t tpm_channel, uint16_t tpm_mod);

/*
 * Define velocidade e sentido: -32768 (ré máxima) .. 0 (ponto morto) ..
 * 32767 (frente máxima).
 */
void motor_set(motor_t *motor, int16_t speed);

/* Freio curto: in1=in2=1. Não é o mesmo que motor_set(motor, 0) (ponto morto). */
void motor_freia(motor_t *motor);

/* Duty aplicado no último motor_set()/motor_freia(), normalizado em -1.0..1.0. */
float motor_get_duty(const motor_t *motor);

#endif /* MOTOR_H_ */
