#include "control_fsm.h"

#include <zephyr.h>

/* velocidade usada nas 3 manobras de auto_mode (frente/curva/ré) —
 * mesma magnitude nos dois motores, só muda sinal/qual roda. */
#define VELOCIDADE_AUTO (INT16_MAX / 2)

static int64_t last_heartbeat_ms;

void control_fsm_heartbeat()
{
    last_heartbeat_ms = k_uptime_get();
}

bool control_fsm_watchdog(radio_cmd_t *cmd)
{
    if (k_uptime_get() - last_heartbeat_ms > CONTROL_FSM_TIMEOUT_MS) {
        cmd->auto_mode = 1;
        return true;
    }
    return false;
}

void control_fsm_apply(const radio_cmd_t *cmd,
                        motor_t *motor_l, motor_t *motor_r,
                        ultrassom_t *sensor)
{
    if (cmd->auto_mode) {
        float distance = ultrassom_read(sensor);

        if (distance <= DISTANCIA_PARADA_M) {
            /* obstáculo colado — para completamente, sem tentar manobra */
            motor_freia(motor_l);
            motor_freia(motor_r);
        } else if (distance <= DISTANCIA_CURVA_M) {
            /* perto demais pra virar com segurança — dá ré */
            motor_set(motor_l, -VELOCIDADE_AUTO);
            motor_set(motor_r, -VELOCIDADE_AUTO);
        } else if (distance <= DISTANCIA_FRENTE_M) {
            /* obstáculo à frente — vira à direita (pivô: L frente, R ré) */
            motor_set(motor_l, VELOCIDADE_AUTO);
            motor_set(motor_r, -VELOCIDADE_AUTO);
        } else {
            /* livre — segue em frente */
            motor_set(motor_l, VELOCIDADE_AUTO);
            motor_set(motor_r, VELOCIDADE_AUTO);
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
