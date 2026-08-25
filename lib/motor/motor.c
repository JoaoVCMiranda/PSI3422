/*
 * Por que enA/enB usam pwm_z42 (bare-metal) em vez da API padrão do
 * Zephyr (`<zephyr/drivers/pwm.h>`, `pwm_dt_spec` + `pwm_set_pulse_dt`):
 *
 * A alternativa Zephyr nativa ficaria assim:
 *
 *     #include <zephyr/drivers/pwm.h>
 *     struct pwm_dt_spec ena; // de PWM_DT_SPEC_GET(...)
 *     ...
 *     pwm_set_pulse_dt(&motor->ena, pulse_ns); // só duty, período fixo no DT
 *
 * Motor só precisa variar duty (não período), então em teoria a API
 * nativa bastaria — ao contrário do radar da Ativ.5 (PSI3441), que
 * precisa mudar o período (TPM_MOD) em runtime e por isso *precisa*
 * de pwm_z42. Mas neste board/framework especificamente
 * (framework-zephyr@2.20701.220422, board frdm_kl25z) não existe
 * NENHUM nó de TPM na devicetree (conferido em
 * dts/arm/nxp/nxp_kl25z.dtsi) — não há como obter um pwm_dt_spec para
 * TPM aqui, ponto. A API Zephyr de PWM não é uma opção neste
 * ambiente, não é só uma preferência de estilo. pwm_z42 funciona
 * porque não depende de devicetree: configura o TPM direto via CMSIS
 * (MKL25Z4.h), independente do que a devicetree do Zephyr conhece.
 */
#include "motor.h"

#include <errno.h>

int motor_init(motor_t *motor,
                const struct gpio_dt_spec *in1,
                const struct gpio_dt_spec *in2,
                TPM_MemMapPtr tpm, uint16_t tpm_channel, uint16_t tpm_mod)
{
    int ret;

    motor->in1 = *in1;
    motor->in2 = *in2;
    motor->tpm = tpm;
    motor->tpm_channel = tpm_channel;
    motor->tpm_mod = tpm_mod;
    motor->speed = 0;

    if (!device_is_ready(motor->in1.port) || !device_is_ready(motor->in2.port)) {
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&motor->in1, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        return ret;
    }

    return gpio_pin_configure_dt(&motor->in2, GPIO_OUTPUT_INACTIVE);
}

static void motor_apply_pwm(motor_t *motor, uint16_t magnitude)
{
    /* magnitude: 0..32768 -> CnV proporcional ao MOD (período) do TPM */
    uint32_t cnv = (uint32_t)motor->tpm_mod * magnitude / 32768U;

    pwm_tpm_CnV(motor->tpm, motor->tpm_channel, (uint16_t)cnv);
}

void motor_set(motor_t *motor, int16_t speed)
{
    motor->speed = speed;

    if (speed > 0) {
        gpio_pin_set_dt(&motor->in1, 1);
        gpio_pin_set_dt(&motor->in2, 0);
        motor_apply_pwm(motor, (uint16_t)speed);
    } else if (speed < 0) {
        gpio_pin_set_dt(&motor->in1, 0);
        gpio_pin_set_dt(&motor->in2, 1);
        /* -32768 não tem +32768 correspondente; nega em 32 bits antes de truncar */
        motor_apply_pwm(motor, (uint16_t)(-(int32_t)speed));
    } else {
        gpio_pin_set_dt(&motor->in1, 0);
        gpio_pin_set_dt(&motor->in2, 0);
        motor_apply_pwm(motor, 0);
    }
}

void motor_freia(motor_t *motor)
{
    motor->speed = 0;
    gpio_pin_set_dt(&motor->in1, 1);
    gpio_pin_set_dt(&motor->in2, 1);
    motor_apply_pwm(motor, 0);
}

float motor_get_duty(const motor_t *motor)
{
    return (float)motor->speed / 32768.0f;
}
