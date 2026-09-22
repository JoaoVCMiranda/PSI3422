#ifndef VOLANTE_H_
#define VOLANTE_H_

#include <stdint.h>

#include "motor.h"

/*
 * Abstração de "dirigir com duas rodas": junta motor_l/motor_r (cada
 * um só sabe girar) com a compensação mecânica entre eles — sem essa
 * camada, todo chamador (control_fsm em auto_mode, comando manual via
 * rádio) precisava lembrar de aplicar o trim nos dois motores toda
 * vez que quisesse "ir reto".
 *
 * trim_esquerda: offset de duty aplicado só na roda esquerda (some
 * com o sinal do comando: soma indo pra frente, subtrai indo de ré) —
 * medido em bancada (debug/debug_ponte_H, debug/debug_ponte_H_encoder):
 * motor esquerdo é mecanicamente mais fraco que o direito (confirmado
 * trocando os motores de canal — a fraqueza seguiu o motor, não a
 * fiação/canal), precisa de mais duty pra girar no mesmo ritmo do
 * direito. TRIM_L=3000 (de 32767) foi o valor validado nessa bancada;
 * ajuste aqui se o chassi/motor mudar.
 *
 * Direção "direita"/"esquerda" segue a mesma convenção já testada em
 * projeto_final/Carrinho/lib/control_fsm/control_fsm.c: virar à
 * direita = esquerda frente + direita ré (pivô no próprio eixo).
 */
typedef struct {
    motor_t *esquerda;
    motor_t *direita;
    int16_t trim_esquerda;
} volante_t;

/*
 * esquerda/direita: motor_t já inicializados (motor_init), guardados
 * por ponteiro — o volante não é dono deles, só os pilota.
 * trim_esquerda: offset de duty da roda esquerda (ver comentário
 * acima); 0 se não houver assimetria conhecida.
 */
void volante_init(volante_t *volante, motor_t *esquerda, motor_t *direita, int16_t trim_esquerda);

/*
 * Comando direto por roda (ex.: WASD/joystick vindo do rádio, curva
 * assimétrica) — ainda aplica trim_esquerda, então mesmo o caminho
 * "manual" sai reto sem cada chamador ter que lembrar da compensação.
 */
void volante_set(volante_t *volante, int16_t esquerda, int16_t direita);

/* velocidade: 0..32767, mesma magnitude nas duas rodas (trim à parte). */
void volante_frente(volante_t *volante, int16_t velocidade);
void volante_re(volante_t *volante, int16_t velocidade);

/* Pivô no próprio eixo: uma roda frente, a outra ré, mesma magnitude. */
void volante_direita(volante_t *volante, int16_t velocidade);
void volante_esquerda(volante_t *volante, int16_t velocidade);

/* Freio curto nas duas rodas (ver motor_freia) — não é ponto morto. */
void volante_para(volante_t *volante);

#endif /* VOLANTE_H_ */
