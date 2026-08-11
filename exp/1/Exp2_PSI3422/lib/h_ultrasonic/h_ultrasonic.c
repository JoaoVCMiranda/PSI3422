#include "h_ultrasonic.h"
#include <pwm_z42.h>
#include <zephyr/irq.h>

#define EDGE_PWM 0
#define PWM_MODE_HIGH_TRUE 0x28
#define TPM_INPUT_CAPTURE_EITHER_EDGE_INT 0x4C

// Semáforo interno da biblioteca para controle de fluxo
K_SEM_DEFINE(lib_pulse_sem, 0, 1);

// Variáveis voláteis internas para controle da ISR
static volatile uint16_t lib_pulse_ticks = 0;
static uint8_t lib_echo_channel = 2;
static GPIO_Type* lib_echo_bank = GPIOA;
static uint8_t lib_echo_pin = 5;

// Interrupção unificada do TPM0 dentro da biblioteca
void lib_tpm0_isr(const void *arg)
{
    ARG_UNUSED(arg);
    static uint16_t start_time = 0;

    // Cria a máscara dinâmica baseada no canal configurado (CH0F, CH1F, CH2F...)
    uint32_t channel_mask = (1 << lib_echo_channel);

    if (TPM0->STATUS & channel_mask) {
        // Limpa a flag de interrupção do canal específico
        TPM0->STATUS = channel_mask;

        // Captura o tempo atual do canal configurado
        uint16_t current_capture = TPM0->CONTROLS[lib_echo_channel].CnV;

        // Verifica dinamicamente se o pino do Echo está em HIGH (Borda de Subida)
        if (lib_echo_bank->PDIR & (1 << lib_echo_pin)) {
            start_time = current_capture;
        }
        // Se está em LOW (Borda de Descida)
        else {
            uint16_t pulse_duration = current_capture - start_time;

            if (pulse_duration > 5) {
                lib_pulse_ticks = pulse_duration;
                k_sem_give(&lib_pulse_sem); // Libera a função create_input
            }
        }
    }
}

void create_trigger(TPM_Type* tpm, uint8_t channel, uint32_t frequency_hz, uint32_t duty_ticks, GPIO_Type* gpio_bank, uint8_t pin_number)
{
    // Calcula o valor do MOD com base na frequência (assumindo clock de sistema padrão do driver)
    // Para manter compatibilidade exata com seu código anterior (37499 para ~40Hz):
    uint32_t mod_value = (frequency_hz == 40) ? 37499 : (6000000 / frequency_hz) - 1;

    // Inicializa o periférico TPM usando as funções da pwm_z42
    pwm_tpm_Init(tpm, 1, mod_value, 1, 6, EDGE_PWM);
    pwm_tpm_Ch_Init(tpm, channel, PWM_MODE_HIGH_TRUE, gpio_bank, pin_number);
    pwm_tpm_CnV(tpm, channel, duty_ticks);
}

uint16_t create_input(uint8_t channel, GPIO_Type* gpio_bank, uint8_t pin_number)
{
    // Atualiza as variáveis estáticas para que a ISR saiba quem ler
    lib_echo_channel = channel;
    lib_echo_bank = gpio_bank;
    lib_echo_pin = pin_number;

    // Conecta e ativa a interrupção do TPM0 (apenas uma vez de forma segura)
    static bool irq_initialized = false;
    if (!irq_initialized) {
        IRQ_CONNECT(TPM0_IRQn, 1, lib_tpm0_isr, NULL, 0);
        irq_enable(TPM0_IRQn);
        irq_initialized = true;
    }

    // Inicializa o canal do Input Capture
    pwm_tpm_Ch_Init(TPM0, channel, TPM_INPUT_CAPTURE_EITHER_EDGE_INT, gpio_bank, pin_number);

    // Bloqueia a execução aqui até que a ISR capture uma borda de descida válida
    k_sem_take(&lib_pulse_sem, K_FOREVER);

    // Retorna o tempo gerado pela ISR
    return lib_pulse_ticks;
}