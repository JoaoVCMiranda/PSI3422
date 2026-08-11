#ifndef NRF24_H
#define NRF24_H

#include <stdint.h>
#include <stddef.h>

/*
 * Tamanho máximo de payload suportado pelo nRF24L01+.
 */
#define NRF24_MAX_PAYLOAD_SIZE 32

/**
 * Inicializa o nRF24L01+.
 *
 * Configura:
 *  - SPI
 *  - CE
 *  - IRQ
 *  - rádio
 *  - CRC
 *  - auto-ACK
 *  - retransmissão
 *  - endereço
 *  - canal RF
 *
 * Retorno:
 *   0  = sucesso
 *  <0  = erro
 */
int nrf24_init(void);

/**
 * Envia uma mensagem.
 *
 * data   = ponteiro para os dados
 * length = quantidade de bytes
 *
 * A função só retorna quando:
 *
 *   - a transmissão foi confirmada, ou
 *   - ocorreu uma falha, ou
 *   - ocorreu timeout.
 *
 * Retorno:
 *   0  = transmissão realizada com sucesso
 *  <0  = erro
 */
int nrf24_send(const uint8_t *data, size_t length);

/**
 * Aguarda uma mensagem recebida.
 *
 * data       = buffer onde a mensagem será armazenada
 * max_length = tamanho máximo do buffer
 *
 * Retorno:
 *   >= 0 = quantidade de bytes recebidos
 *   < 0  = erro
 */
int nrf24_receive(uint8_t *data, size_t max_length);

#endif /* NRF24_H */