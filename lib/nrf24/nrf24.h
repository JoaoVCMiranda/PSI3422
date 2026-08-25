#ifndef NRF24_H
#define NRF24_H

#include <stdint.h>
#include <stddef.h>
#include <drivers/gpio.h>
#include <zephyr.h>

/*
 * Tamanho máximo de payload suportado pelo nRF24L01+.
 */
#define NRF24_MAX_PAYLOAD_SIZE 32

/**
 * Inicializa o nRF24L01+: SPI0 (bare-metal, ver spi.h), CE/CSN/IRQ
 * (GPIO Zephyr nativo, specs passados pelo chamador), rádio, CRC,
 * auto-ACK, retransmissão, endereço, canal RF.
 *
 * ce/csn/irq: gpio_dt_spec já preenchidos por quem chama (ver
 * main.c e experiências/Exp2_PSI3422/Pinmap.md) — este módulo não
 * depende de nó de devicetree próprio (diferente de uma versão
 * anterior, que exigia um nó customizado `nrf24` que nunca chegou a
 * existir na devicetree real do board).
 *
 * Retorno:
 *   0  = sucesso
 *  <0  = erro
 */
int nrf24_init(const struct gpio_dt_spec *ce,
               const struct gpio_dt_spec *csn,
               const struct gpio_dt_spec *irq);

/**
 * Envia uma mensagem. Só retorna quando a transmissão foi confirmada,
 * falhou, ou deu timeout.
 *
 * Retorno:
 *   0  = transmissão realizada com sucesso
 *  <0  = erro
 */
int nrf24_send(const uint8_t *data, size_t length);

/**
 * Aguarda uma mensagem recebida, até `timeout` (ex.: K_MSEC(100) ou
 * K_FOREVER). Timeout explícito (diferente do K_FOREVER fixo da
 * versão original em Exp2_PSI3422) para permitir um loop de controle
 * que precisa continuar rodando — e detectar perda de conexão — mesmo
 * sem pacote chegando (ver control_fsm_watchdog() e main.c).
 *
 * Retorno:
 *   >= 0 = quantidade de bytes recebidos
 *   -ETIMEDOUT = nada chegou dentro do timeout
 *   < 0  = outro erro
 */
int nrf24_receive(uint8_t *data, size_t max_length, k_timeout_t timeout);

#endif /* NRF24_H */
