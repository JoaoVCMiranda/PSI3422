#ifndef EXP2_PSI3422_PROTOCOL_H_
#define EXP2_PSI3422_PROTOCOL_H_

#include <stdint.h>

/*
 * Payload trocado por rádio (nRF24L01+) entre Controle e Carrinho.
 * Carrinho e Controle são dois binários Zephyr separados — só
 * concordam sobre o comando/telemetria se o layout em bytes for
 * idêntico dos dois lados. `packed` torna isso explícito em vez de
 * depender da regra de alinhamento do compilador (mesmo sendo o
 * mesmo toolchain nos dois).
 *
 * Cada struct vai direto em nrf24_send()/nrf24_receive() (cast pra
 * `uint8_t *`, `sizeof(struct)` como length) — substitui o
 * pack/unpack manual byte a byte que existia antes.
 *
 * auto_mode/freio são uint8_t (0/1), não bool: o tamanho de bool não
 * é garantido pela linguagem C, e aqui o tamanho em bytes É o
 * contrato entre os dois lados.
 */

/* Controle -> Carrinho */
typedef struct __attribute__((packed)) {
    uint8_t auto_mode;
    uint8_t freio;
    int16_t motor_l;
    int16_t motor_r;
} radio_cmd_t;

/* Carrinho -> Controle */
typedef struct __attribute__((packed)) {
    uint16_t dist_cm;  /* 0xFFFF = sem eco */
    int16_t duty_l;    /* milésimos, -1000..1000 */
    int16_t duty_r;
} radio_telemetry_t;

#endif /* EXP2_PSI3422_PROTOCOL_H_ */
