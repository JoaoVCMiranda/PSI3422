/*
 * PSI3422 — projeto_final / Carrinho
 *
 * Consolidação de produção: 2 motores DC (ponte H dupla) + ultrassom
 * HC-SR04 + rádio nRF24L01+ (base: experiências/Exp2_PSI3422/Carrinho)
 * com encoders IR HW-201 + odometria diferencial somados por cima
 * (base: experiências/Exp4_PSI3422/CarrinhoBase) — cobre o roteiro das
 * Aulas 5/6 (README.md raiz): RUN/STOP remotos, travessia de
 * labirinto por desvio reativo, distância percorrida que nunca
 * decresce, comando de apagar. Board: FRDM-KL25Z.
 *
 * Pinos: ver ../../pinmap.h (gerado por ../../tools/gen_pinmap.py a
 * partir de ../../pinmap.yaml — fonte única também do símbolo KiCad
 * em ../../kicad/). Protocolo de rádio: ../../protocol.h.
 *
 * ── Arquitetura híbrida (GPIO Zephyr nativo, PWM/SPI bare-metal) e
 * por que um laço síncrono só (sem thread de rádio dedicada) ──
 * ver o comentário equivalente, mais longo, em
 * experiências/Exp2_PSI3422/Carrinho/src/main.c — inalterado aqui,
 * não repetido pra não desincronizar duas cópias do mesmo racional.
 *
 * RUN = auto_mode (desvio reativo por ultrassom frontal, 4 faixas —
 * ver lib/control_fsm/control_fsm.c). É a "travessia de labirinto"
 * cabível aqui: o carrinho só tem um sensor de distância (frontal),
 * não há como mapear o labirinto, só reagir ao obstáculo mais
 * próximo. Herdado de Exp2_PSI3422 sem mudança de lógica — por
 * PENDENCIAS.md daquele experimento, essa lógica nunca rodou em
 * carrinho de verdade, só buildou; validação de bancada fica pra
 * quando o hardware estiver montado.
 * STOP = freio (motores ignorados, carrinho parado).
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/printk.h>

#include "pwm_z42.h"
#include "motor.h"
#include "ultrassom.h"
#include "encoder.h"
#include "odometria.h"
#include "nrf24.h"
#include "control_fsm.h"
#include "../../protocol.h"
#include "../../pinmap.h"

/* TPM0 compartilhado pelos 2 canais de PWM dos motores: MCGIRCLK (4MHz,
 * independente do PLL) / PS_1 -> f_tpm = 4MHz; MOD=3999 -> f_pwm = 1kHz
 * (audível mas comum e seguro para motor DC via L298N). */
#define TPM_MOTOR_MOD 3999U

/* ── Calibração da odometria — medida em bancada, ver
 * experiências/Exp4_PSI3422/CarrinhoBase/src/main.c e
 * control/relatorio-aula-4.md (raio/dist. entre rodas medidos;
 * pulsos/volta=1, marco de papel na roda, a confirmar girando N
 * voltas em bancada). ── */
#define DISTANCIA_ENTRE_RODAS_M 0.20f
#define PULSOS_POR_VOLTA 1
#define RODA_RAIO_M 0.05f
#define RODA_CIRCUNFERENCIA_M (2.0f * 3.14159265f * RODA_RAIO_M)

static motor_t motor_l;
static motor_t motor_r;
static ultrassom_t sensor;
static encoder_t encoder_l;
static encoder_t encoder_r;
static odometria_pose_t pose;

void main()
{
    struct gpio_dt_spec l_in1 = { .port = MOTOR_L_IN1_PORT, .pin = MOTOR_L_IN1_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec l_in2 = { .port = MOTOR_L_IN1_PORT, .pin = MOTOR_L_IN2_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in1 = { .port = MOTOR_R_IN1_PORT, .pin = MOTOR_R_IN1_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in2 = { .port = MOTOR_R_IN2_PORT, .pin = MOTOR_R_IN2_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec trig  = { .port = ULTRASSOM_TRIG_PORT, .pin = ULTRASSOM_TRIG_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec echo  = { .port = ULTRASSOM_ECHO_PORT, .pin = ULTRASSOM_ECHO_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec enc_l = { .port = ENCODER_L_PORT, .pin = ENCODER_L_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec enc_r = { .port = ENCODER_R_PORT, .pin = ENCODER_R_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec ce    = { .port = RADIO_CE_PORT, .pin = RADIO_CE_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec csn   = { .port = RADIO_CSN_PORT, .pin = RADIO_CSN_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    struct gpio_dt_spec irq   = { .port = RADIO_IRQ_PORT, .pin = RADIO_IRQ_PIN, .dt_flags = GPIO_ACTIVE_LOW };

    struct gpio_dt_spec led_red = { .port = LED_RED_PORT, .pin = LED_RED_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    struct gpio_dt_spec led_green = { .port = LED_GREEN_PORT, .pin = LED_GREEN_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    if (device_is_ready(led_red.port)) {
        gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    }
    int ret;

    /* TPM0 uma vez só — MOD (período) é compartilhado por todos os canais */
    if (!pwm_tpm_Init(TPM0, TPM_MCGIRCLK, TPM_MOTOR_MOD, TPM_CLK, PS_1, EDGE_PWM)) {
        printk("ERRO: pwm_tpm_Init(TPM0) falhou\n");
        return;
    }
    if (!pwm_tpm_Ch_Init(TPM0, MOTOR_L_ENA_CH, TPM_PWM_H, MOTOR_L_ENA_GPIO, MOTOR_L_ENA_PIN)) {
        printk("ERRO: pwm_tpm_Ch_Init(motor L) falhou\n");
        return;
    }
    if (!pwm_tpm_Ch_Init(TPM0, MOTOR_R_ENB_CH, TPM_PWM_H, MOTOR_R_ENB_GPIO, MOTOR_R_ENB_PIN)) {
        printk("ERRO: pwm_tpm_Ch_Init(motor R) falhou\n");
        return;
    }

    ret = motor_init(&motor_l, &l_in1, &l_in2, TPM0, MOTOR_L_ENA_CH, TPM_MOTOR_MOD);
    if (ret < 0) { printk("ERRO: motor_init(L) = %d\n", ret); return; }

    ret = motor_init(&motor_r, &r_in1, &r_in2, TPM0, MOTOR_R_ENB_CH, TPM_MOTOR_MOD);
    if (ret < 0) { printk("ERRO: motor_init(R) = %d\n", ret); return; }

    ret = ultrassom_init(&sensor, &trig, &echo);
    if (ret < 0) { printk("ERRO: ultrassom_init = %d\n", ret); return; }

    /* encoder_init depois de motor_init: a ISR do encoder lê motor->speed,
     * então o motor associado precisa já existir (não precisa girar ainda). */
    ret = encoder_init(&encoder_l, &enc_l, &motor_l);
    if (ret < 0) { printk("ERRO: encoder_init(L) = %d\n", ret); return; }

    ret = encoder_init(&encoder_r, &enc_r, &motor_r);
    if (ret < 0) { printk("ERRO: encoder_init(R) = %d\n", ret); return; }

    odometria_calibracao_t odo_calib = {
        .circunferencia_roda_m = RODA_CIRCUNFERENCIA_M,
        .distancia_entre_rodas_m = DISTANCIA_ENTRE_RODAS_M,
        .pulsos_por_volta = PULSOS_POR_VOLTA,
    };
    odometria_init(&pose);

    printk("\n==================================\n");
    printk("=== BOOTING CARRINHO APP       ===\n");
    printk("==================================\n");

    ret = nrf24_init(&ce, &csn, &irq);
    if (ret < 0) {
        printk("ERRO: rádio falhou = %d. Carrinho continuará sem rádio.\n", ret);
    }

    printk("PSI3422 projeto_final — carrinho pronto\n");

    radio_cmd_t cmd = { .auto_mode = 1 }; /* começa em RUN (modo seguro: desvia sozinho) */
    uint8_t last_apagar_seq = 0;
    control_fsm_heartbeat();

    for (;;) {
        /*
         * O rádio é configurado (nrf24_init) com payload fixo de
         * NRF24_MAX_PAYLOAD_SIZE (32) nos dois lados — nrf24_receive()
         * exige um buffer de pelo menos esse tamanho e sempre lê os
         * 32 bytes inteiros, então o buffer de rádio continua sendo
         * uint8_t[32]; radio_cmd_t/radio_telemetry_t (protocol.h) só
         * descrevem os bytes que de fato importam no início dele.
         */
        uint8_t rx_payload[NRF24_MAX_PAYLOAD_SIZE];
        uint8_t tx_payload[NRF24_MAX_PAYLOAD_SIZE] = {0};

        ret = nrf24_receive(rx_payload, sizeof(rx_payload), K_MSEC(100));
        if (ret >= 0) {
            cmd = *(const radio_cmd_t *)rx_payload;
            control_fsm_heartbeat();

            static int rx_cnt = 0;
            if (++rx_cnt % 10 == 0) {
                printk("-> recebido: auto=%d freio=%d motL=%d motR=%d\n", cmd.auto_mode, cmd.freio, cmd.motor_l, cmd.motor_r);
            }
        }

        /* ── Controle dos LEDs de Status ── */
        /* O watchdog retorna true se não tiver controle (timeout) */
        bool sem_radio = control_fsm_watchdog(&cmd); /* sem comando há > 500ms -> força RUN (auto_mode) */
        gpio_pin_set_dt(&led_red, sem_radio ? 1 : 0);
        gpio_pin_set_dt(&led_green, sem_radio ? 0 : 1);

        // Se ficar 2 segundos sem rádio (após handshake), tenta reiniciar o módulo
        static int64_t radio_lost_time = 0;
        static bool handshake = false;

        if (!sem_radio) {
            handshake = true;
            radio_lost_time = 0;
        } else if (handshake) {
            if (radio_lost_time == 0) {
                radio_lost_time = k_uptime_get();
            } else if (k_uptime_get() - radio_lost_time > 2000) {
                printk("Reconectando modulo NRF24 do Carrinho...\n");
                nrf24_init(&ce, &csn, &irq);
                radio_lost_time = k_uptime_get(); // Tenta de novo em 2s
            }
        }

        /* ── Atualizar Sensor Ultrassom ── */
        ultrassom_read(&sensor); // Atualiza sensor->distance

        control_fsm_apply(&cmd, &motor_l, &motor_r, &sensor);

        /* ── Odometria: delta de pulsos desde o último ciclo (~50ms) ── */
        int32_t delta_l = encoder_reset(&encoder_l);
        int32_t delta_r = encoder_reset(&encoder_r);
        odometria_atualiza(&pose, &odo_calib, delta_l, delta_r);

        /* "Apagar distância": borda de apagar_seq (não nível) — ver
         * comentário em protocol.h sobre por que é contador, não flag. */
        if (cmd.apagar_seq != last_apagar_seq) {
            last_apagar_seq = cmd.apagar_seq;
            odometria_init(&pose);
            printk("Distancia percorrida apagada (comando remoto).\n");
        }

        uint16_t dist = (uint16_t)(sensor.distance * 100.0f);
        int16_t out_l = (int16_t)(motor_get_duty(&motor_l) * 1000.0f);
        int16_t out_r = (int16_t)(motor_get_duty(&motor_r) * 1000.0f);
        uint32_t dist_percorrida_cm = (uint32_t)(pose.distancia_percorrida_m * 100.0f);

        *(radio_telemetry_t *)tx_payload = (radio_telemetry_t){
            .dist_cm = dist, .duty_l = out_l, .duty_r = out_r,
            .dist_percorrida_cm = dist_percorrida_cm,
        };
        nrf24_send(tx_payload, sizeof(tx_payload));

        static int loop_cnt = 0;
        if (++loop_cnt % 10 == 0) { // roughly every 500ms
            printk("car heartbeat: dist=%dcm cmd.auto=%d outL=%d outR=%d percorrida=%ucm (sem_radio=%d)\n",
                   dist, cmd.auto_mode, out_l, out_r, dist_percorrida_cm, sem_radio);
        }

        k_msleep(50);
    }
}
