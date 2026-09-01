#ifndef ENCODER_H_
#define ENCODER_H_

#include <zephyr.h>
#include <drivers/gpio.h>
#include <stdint.h>

#include "motor.h"

/*
 * IR HW-201: encoder de canal único (disco ranhurado + par
 * emissor/receptor IR) — o OUT digital pulsa uma vez por furo, mas
 * sozinho não diz se a roda girou pra frente ou pra trás (não é
 * quadratura, não há segundo canal defasado). Por isso o sentido de
 * cada pulso é inferido do último comando aplicado ao motor da mesma
 * roda (motor_t.speed, ver motor.h) no instante da interrupção:
 * >0 soma, <0 subtrai, ==0 (ponto morto ou freio) ignora — assim
 * vibração residual com o motor parado não é contada como rotação.
 * Essa inferência assume que a roda não patina/derrapa fora do
 * sentido comandado; suficiente para o objetivo da Aula 4 (contagem
 * de voltas com sinal), não uma medida independente de sentido.
 *
 * Validado em bancada por debug/EncoderCheck (Exp2_PSI3422): pulsos
 * por segundo por roda no mesmo duty, ainda sem o sinal de sentido
 * (ver Pinmap.md, seção Carrinho, sobre a reserva de PTD1/PTD3 —
 * único par de pinos livre em PORTD, escolhido porque nesta
 * subfamília KL25Z só PORTA e PORTC/PORTD geram interrupção por
 * mudança de pino; PORTB/PORTE não).
 */
typedef struct {
    struct gpio_dt_spec pino;
    struct gpio_callback cb;

    const motor_t *motor; /* só leitura: usado para inferir o sentido do pulso */

    volatile int32_t pulsos; /* contador assinado: +pra frente, -pra trás */
} encoder_t;

/*
 * pino: port/pin do OUT do HW-201 (GPIO_INPUT + GPIO_INT_EDGE_RISING
 *   são configurados aqui dentro, quem chama só preenche port/pin).
 * motor: motor da mesma roda — só precisa existir (não precisa já
 *   estar inicializado), o sentido é lido a cada pulso, não aqui.
 */
int encoder_init(encoder_t *encoder, const struct gpio_dt_spec *pino, const motor_t *motor);

/* Contador assinado atual (pulsos), sem zerar. */
int32_t encoder_get(const encoder_t *encoder);

/* Zera o contador e devolve o valor que tinha antes de zerar. */
int32_t encoder_reset(encoder_t *encoder);

#endif /* ENCODER_H_ */
