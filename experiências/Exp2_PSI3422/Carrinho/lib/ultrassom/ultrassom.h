#ifndef ULTRASSOM_H_
#define ULTRASSOM_H_

#include <zephyr.h>
#include <drivers/gpio.h>

/*
 * HC-SR04: trigger (saída) + echo (entrada, interrupção de borda).
 * GPIO Zephyr nativo — gpioc/gpiod funcionam normalmente neste
 * framework (só SPI e TPM não têm nó de devicetree, ver nrf24.c e
 * motor.c). Mede o pulso por interrupção (não por polling) para não
 * bloquear a CPU durante o eco — ver
 * (notas de outra disciplina, não presentes neste repo) para o contraste
 * com a leitura direta de registrador (PDIR) usada na Ativ.5.
 */
typedef struct {
    struct gpio_dt_spec trigger;
    struct gpio_dt_spec echo;

    struct gpio_callback echo_cb;
    struct k_sem echo_sem;

    volatile uint32_t rise_cycles;
    volatile uint32_t pulse_cycles;

    float distance; /* metros, atualizado a cada ultrassom_read() */
} ultrassom_t;

int ultrassom_init(ultrassom_t *sensor,
                    const struct gpio_dt_spec *trigger,
                    const struct gpio_dt_spec *echo);

/* Dispara o trigger e bloqueia até capturar o eco (ou 30 ms de timeout). */
float ultrassom_read(ultrassom_t *sensor);

#endif /* ULTRASSOM_H_ */
