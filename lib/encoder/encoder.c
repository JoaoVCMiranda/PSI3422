#include "encoder.h"

#include <errno.h>

static void encoder_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(pins);

    encoder_t *encoder = CONTAINER_OF(cb, encoder_t, cb);
    int16_t speed = encoder->motor->speed;

    if (speed > 0) {
        encoder->pulsos++;
    } else if (speed < 0) {
        encoder->pulsos--;
    }
    /* speed == 0 (ponto morto/freio): pulso ignorado, ver encoder.h */
}

int encoder_init(encoder_t *encoder, const struct gpio_dt_spec *pino, const motor_t *motor)
{
    int ret;

    encoder->pino = *pino;
    encoder->motor = motor;
    encoder->pulsos = 0;

    if (!device_is_ready(encoder->pino.port)) {
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&encoder->pino, GPIO_INPUT);
    if (ret < 0) {
        return ret;
    }

    gpio_init_callback(&encoder->cb, encoder_isr, BIT(encoder->pino.pin));

    ret = gpio_add_callback(encoder->pino.port, &encoder->cb);
    if (ret < 0) {
        return ret;
    }

    return gpio_pin_interrupt_configure_dt(&encoder->pino, GPIO_INT_EDGE_RISING);
}

int32_t encoder_get(const encoder_t *encoder)
{
    return encoder->pulsos;
}

int32_t encoder_reset(encoder_t *encoder)
{
    /* leitura+zero não é atômico entre si (não crítico: no pior caso
     * perde-se ou soma-se 1 pulso de uma interrupção concorrente,
     * mesmo trade-off já aceito no encoder_l_count/encoder_r_count do
     * EncoderCheck) */
    int32_t valor = encoder->pulsos;
    encoder->pulsos = 0;
    return valor;
}
