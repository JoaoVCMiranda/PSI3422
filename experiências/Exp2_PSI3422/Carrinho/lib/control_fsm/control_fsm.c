#include "control_fsm.h"

#include <zephyr.h>

static int64_t last_heartbeat_ms;

void control_fsm_heartbeat()
{
    last_heartbeat_ms = k_uptime_get();
}

bool control_fsm_watchdog(control_cmd_t *cmd)
{
    if (k_uptime_get() - last_heartbeat_ms > CONTROL_FSM_TIMEOUT_MS) {
        cmd->auto_mode = true;
        return true;
    }
    return false;
}

void control_fsm_apply(const control_cmd_t *cmd,
                        motor_t *motor_l, motor_t *motor_r,
                        ultrassom_t *sensor)
{
    if (cmd->auto_mode) {
        float distance = ultrassom_read(sensor);

        if (distance <= DISTANCIA_MINIMA_M) {
            motor_freia(motor_l);
            motor_freia(motor_r);
        } else {
            motor_set(motor_l, INT16_MAX / 2);
            motor_set(motor_r, INT16_MAX / 2);
        }
        return;
    }

    if (cmd->freio) {
        motor_freia(motor_l);
        motor_freia(motor_r);
        return;
    }

    motor_set(motor_l, cmd->motor_l);
    motor_set(motor_r, cmd->motor_r);
}
