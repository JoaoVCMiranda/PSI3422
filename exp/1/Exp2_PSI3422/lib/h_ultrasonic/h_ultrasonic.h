#ifndef H_ULTRASONIC_H_
#define H_ULTRASONIC_H_

#include <zephyr/kernel.h>
#include <soc.h>

/**
 * @brief Configura o pino de Trigger para gerar o pulso PWM.
 * * @param tpm Ponteiro para o periférico TPM (Ex: TPM0, TPM1)
 * @param channel Canal do TPM (Ex: 1, 2)
 * @param frequency_hz Frequência desejada em Hz (Ex: 40 para o ciclo de 25ms do sensor)
 * @param duty_ticks Tempo em ticks que o sinal fica em HIGH (Ex: 37)
 * @param gpio_bank Ponteiro para o banco GPIO (Ex: GPIOA, GPIOD)
 * @param pin_number Número do pino no banco (Ex: 4, 1)
 */
void create_trigger(TPM_Type* tpm, uint8_t channel, uint32_t frequency_hz, uint32_t duty_ticks, GPIO_Type* gpio_bank, uint8_t pin_number);

/**
 * @brief Inicializa o Input Capture em um canal específico do TPM0 e aguarda a medição.
 * * @param channel Canal do TPM0 para o Echo (Ex: 2 para o PTA5)
 * @param gpio_bank Ponteiro para o banco GPIO (Ex: GPIOA)
 * @param pin_number Número do pino no banco (Ex: 5)
 * @return uint16_t Tempo do pulso medido em ticks do hardware
 */
uint16_t create_input(uint8_t channel, GPIO_Type* gpio_bank, uint8_t pin_number);

#endif /* H_ULTRASONIC_H_ */