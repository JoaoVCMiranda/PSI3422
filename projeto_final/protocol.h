#ifndef PROJETO_FINAL_PROTOCOL_H_
#define PROJETO_FINAL_PROTOCOL_H_

#include <stdint.h>

/*
 * Payload trocado por rádio (nRF24L01+) entre Controle e Carrinho.
 * Base: experiências/Exp2_PSI3422/protocol.h — mesmo racional de
 * `packed` (contrato em bytes entre dois binários Zephyr separados),
 * mesmo motivo pra `auto_mode`/`freio` serem uint8_t e não bool.
 *
 * Dois campos novos pro roteiro da Aula 5/6 (RUN/STOP/mostrar
 * distância/apagar distância) — payload de rádio usa payload fixo de
 * NRF24_MAX_PAYLOAD_SIZE (32) bytes, então sobra espaço de sobra sem
 * mudar o tamanho do buffer nos dois lados.
 *
 * RUN/STOP não ganharam campo novo: `auto_mode`/`freio` já cobrem a
 * semântica (RUN = desvio autônomo já existente, STOP = freio,
 * motores ignorados) — só as teclas em Controle/src/main.c precisam
 * deixar isso explícito no texto (ver key_to_cmd()).
 */

/* Controle -> Carrinho */
typedef struct __attribute__((packed)) {
    uint8_t auto_mode;  /* RUN: desvia sozinho, ignora freio/motor_l/motor_r */
    uint8_t freio;      /* STOP: freia os dois motores, ignora motor_l/motor_r */
    int16_t motor_l;
    int16_t motor_r;
    /*
     * Contador (não flag booleana) incrementado 1x por tecla "apagar
     * distância" em Controle. radio_cmd_t é reenviado a cada ~20ms —
     * uma flag "apagar=1" zeraria a distância continuamente enquanto
     * a tecla ficasse no último comando; um contador que só sobe por
     * tecla permite ao Carrinho detectar a BORDA (valor mudou desde o
     * último comando recebido) e zerar uma vez só. Overflow de
     * uint8_t (255->0) também conta como mudança, então não trava.
     */
    uint8_t apagar_seq;
} radio_cmd_t;

/* Carrinho -> Controle */
typedef struct __attribute__((packed)) {
    uint16_t dist_cm;  /* 0xFFFF = sem eco (ultrassom) */
    int16_t duty_l;    /* milésimos, -1000..1000 */
    int16_t duty_r;
    /*
     * odometria_pose_t.distancia_percorrida_m * 100 (lib/odometria) —
     * nunca decresce, mesmo em ré/meia-volta (ver odometria.c). Vai
     * em toda telemetria (~20-100ms), não só sob pedido: "mostrar
     * distância" em Controle é só imprimir o último valor já
     * recebido, sem round-trip de rádio novo.
     */
    uint32_t dist_percorrida_cm;
} radio_telemetry_t;

#endif /* PROJETO_FINAL_PROTOCOL_H_ */
