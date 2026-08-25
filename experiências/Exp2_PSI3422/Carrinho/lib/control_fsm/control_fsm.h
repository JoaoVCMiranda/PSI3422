#ifndef CONTROL_FSM_H_
#define CONTROL_FSM_H_

#include <stdbool.h>
#include <stdint.h>

#include "motor.h"
#include "ultrassom.h"

/*
 * auto_mode == true  -> ignora freio/motor_l/motor_r, desvia sozinho
 * freio == true      -> freia os dois motores, ignora motor_l/motor_r
 * caso contrário       -> aplica motor_l/motor_r diretamente
 */
typedef struct {
    bool auto_mode;
    bool freio;
    int16_t motor_l;
    int16_t motor_r;
} control_cmd_t;

#define DISTANCIA_MINIMA_M 0.3f
#define CONTROL_FSM_TIMEOUT_MS 500

void control_fsm_apply(const control_cmd_t *cmd,
                        motor_t *motor_l, motor_t *motor_r,
                        ultrassom_t *sensor);

void control_fsm_heartbeat();

/* Se não chega heartbeat há mais de CONTROL_FSM_TIMEOUT_MS, força auto_mode. */
bool control_fsm_watchdog(control_cmd_t *cmd);

#endif /* CONTROL_FSM_H_ */
