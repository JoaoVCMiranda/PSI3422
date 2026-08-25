Aqui ficam as libs compartilhadas entre experiências, na última versão estável, com anotações e referências de uso.

Não saia modificando estas libs a não ser que seja explícita a necessidade para a experiência em questão — elas foram desenvolvidas e testadas para funcionar como estão. Se uma experiência precisar de um comportamento diferente, prefira estender/parametrizar em vez de alterar o contrato existente, e documente o motivo.

Cada projeto PlatformIO que usa uma lib daqui aponta para cá via `lib_extra_dirs = ../../../lib` no seu `platformio.ini` (ajuste o número de `../` conforme a profundidade do projeto).

## nrf24/

Wrapper Zephyr-idiomático (`nrf24_init`/`nrf24_send`/`nrf24_receive`, via `gpio_dt_spec`/`k_timeout_t`) para o transceiver nRF24L01+. Não usa a API `spi_dt_spec` do Zephyr porque o port `frdm_kl25z` deste framework (Zephyr 2.7.1 empacotado pelo PlatformIO) não tem nó de devicetree para SPI — o I/O é feito sobre `spi/` (abaixo). Ver o comentário no topo de `nrf24.c` para o racional completo.

Validado na Exp2_PSI3422 (Carrinho e Controle, comunicação bidirecional com handshake e auto-reconexão).

## spi/

Driver SPI0 bare-metal (acesso direto a registrador via CMSIS) para KL25Z, do Prof. Gustavo Rehder — necessário pelo mesmo motivo do `nrf24/`: sem nó SPI na devicetree deste port, `spi_dt_spec` não compila. Incorporado verbatim a partir do material de referência da disciplina (pasta `update/` entregue pela professora).
