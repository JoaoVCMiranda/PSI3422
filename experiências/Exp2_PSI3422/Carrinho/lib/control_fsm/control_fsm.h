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
 */

#define DISTANCIA_MINIMA_M 0.3f
#define CONTROL_FSM_TIMEOUT_MS 500

void control_fsm_apply(const radio_cmd_t *cmd,
                        motor_t *motor_l, motor_t *motor_r,
                        ultrassom_t *sensor);

void control_fsm_heartbeat();

/* Se não chega heartbeat há mais de CONTROL_FSM_TIMEOUT_MS, força auto_mode. */
bool control_fsm_watchdog(radio_cmd_t *cmd);

#endif /* CONTROL_FSM_H_ */
