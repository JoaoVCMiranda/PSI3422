#ifndef ODOMETRIA_H_
#define ODOMETRIA_H_

#include <stdint.h>

/*
 * Odometria diferencial: converte a variação de pulsos de cada roda
 * (lib/encoder, já com sinal) em ângulo girado e distância percorrida
 * pelo carrinho — a "questão de programação competitiva" citada no
 * roteiro da Aula 4 (dados os dois contadores, calcular a distância
 * percorrida), mais o ângulo necessário pra calibrar as curvas de
 * 90°. Pura aritmética — sem <math.h>/libm, sem seno/cosseno — não
 * depende de Zephyr nem de board específico, então serve tanto pra
 * FRDM-KL25Z quanto pra um port futuro em ESP32.
 *
 * Não rastreia posição (x, y) no plano: não é pedido pelo roteiro
 * desta aula (que pede distância percorrida + curvas calibradas, não
 * localização), e exigiria seno/cosseno — custo/risco de libm em
 * embarcado que não vale a pena sem essa necessidade concreta.
 *
 * Fórmulas (odometria diferencial clássica):
 *   d_esq  = delta_pulsos_esq * circunferencia_roda_m / pulsos_por_volta
 *   d_dir  = delta_pulsos_dir * circunferencia_roda_m / pulsos_por_volta
 *   dtheta = (d_dir - d_esq) / distancia_entre_rodas_m   (rad; positivo = gira à esquerda/ccw)
 *   distância percorrida += (|d_esq| + |d_dir|) / 2       (nunca decresce, mesmo em ré ou meia-volta)
 */
typedef struct {
    float circunferencia_roda_m;   /* 0.31416f (2*pi*0,05m) — raio da roda medido em bancada: 5 cm, ver relatório da Aula 4 */
    float distancia_entre_rodas_m; /* 0.20f — medida em bancada, ver relatório da Aula 4 */
    int32_t pulsos_por_volta;      /* 1 — marco de papel fixado na roda (não disco ranhurado de fábrica), a confirmar em bancada */
} odometria_calibracao_t;

typedef struct {
    float theta_rad;              /* orientação acumulada desde odometria_init(); positivo = giro à esquerda (ccw) */
    float distancia_percorrida_m; /* soma de |deslocamento| — nunca decresce, mesmo dando ré ou meia-volta (ver roteiro da Aula 5/6) */
} odometria_pose_t;

void odometria_init(odometria_pose_t *pose);

/*
 * delta_pulsos_esq/dir: variação de pulsos desde a última chamada,
 * não o valor acumulado — quem chama zera os contadores do encoder a
 * cada ciclo (ex. encoder_reset(), lib/encoder) e passa o delta aqui.
 */
void odometria_atualiza(odometria_pose_t *pose, const odometria_calibracao_t *calib,
                         int32_t delta_pulsos_esq, int32_t delta_pulsos_dir);

#endif /* ODOMETRIA_H_ */
