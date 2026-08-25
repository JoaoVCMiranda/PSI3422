#ifndef CONTROL_FSM_H_
#define CONTROL_FSM_H_

#include <stdbool.h>
#include <stdint.h>

#include "motor.h"
#include "ultrassom.h"
#include "../../../protocol.h"

/*
 * Usa radio_cmd_t (protocol.h) como estado de comando também aqui:
 * é o mesmo comando recebido do Controle, sem tradução — não faz
 * sentido ter dois structs iguais (um "de rádio" e um "de FSM").
 *
 * auto_mode != 0 -> ignora freio/motor_l/motor_r, desvia sozinho
 * freio != 0     -> freia os dois motores, ignora motor_l/motor_r
 * caso contrário -> aplica motor_l/motor_r diretamente
 *
 * Em auto_mode, a distância do ultrassom cai em uma de 4 faixas (sem
 * sobreposição, cada corte é o limite superior EXCLUSIVE da faixa
 * mais perto — ver control_fsm_apply()):
 *
 *   d > DISTANCIA_FRENTE_M                        -> segue em frente
 *   DISTANCIA_CURVA_M  < d <= DISTANCIA_FRENTE_M   -> vira à direita
 *   DISTANCIA_PARADA_M < d <= DISTANCIA_CURVA_M    -> dá ré
 *   d <= DISTANCIA_PARADA_M                        -> para completamente
 */

#define DISTANCIA_FRENTE_M 0.30f
#define DISTANCIA_CURVA_M  0.20f
#define DISTANCIA_PARADA_M 0.04f
#define CONTROL_FSM_TIMEOUT_MS 500

void control_fsm_apply(const radio_cmd_t *cmd,
                        motor_t *motor_l, motor_t *motor_r,
                        ultrassom_t *sensor);

void control_fsm_heartbeat();

/* Se não chega heartbeat há mais de CONTROL_FSM_TIMEOUT_MS, força auto_mode. */
bool control_fsm_watchdog(radio_cmd_t *cmd);

#endif /* CONTROL_FSM_H_ */
