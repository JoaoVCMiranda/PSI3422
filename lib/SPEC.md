Aqui ficam as libs compartilhadas entre experiências, na última versão estável, com anotações e referências de uso.

Não saia modificando estas libs a não ser que seja explícita a necessidade para a experiência em questão — elas foram desenvolvidas e testadas para funcionar como estão. Se uma experiência precisar de um comportamento diferente, prefira estender/parametrizar em vez de alterar o contrato existente, e documente o motivo.

Cada projeto PlatformIO que usa uma lib daqui aponta para cá via `lib_extra_dirs = ../../../lib` no seu `platformio.ini` (ajuste o número de `../` conforme a profundidade do projeto).

## nrf24/

Wrapper Zephyr-idiomático (`nrf24_init`/`nrf24_send`/`nrf24_receive`, via `gpio_dt_spec`/`k_timeout_t`) para o transceiver nRF24L01+. Não usa a API `spi_dt_spec` do Zephyr porque o port `frdm_kl25z` deste framework (Zephyr 2.7.1 empacotado pelo PlatformIO) não tem nó de devicetree para SPI — o I/O é feito sobre `spi/` (abaixo). Ver o comentário no topo de `nrf24.c` para o racional completo.

Validado na Exp2_PSI3422 (Carrinho e Controle, comunicação bidirecional com handshake e auto-reconexão).

## spi/

Driver SPI0 bare-metal (acesso direto a registrador via CMSIS) para KL25Z, do Prof. Gustavo Rehder — necessário pelo mesmo motivo do `nrf24/`: sem nó SPI na devicetree deste port, `spi_dt_spec` não compila. Incorporado verbatim a partir do material de referência da disciplina (pasta `update/` entregue pela professora).

## pwm_z42/

Driver TPM bare-metal (acesso direto a registrador via CMSIS/MKL25Z4.h) do Prof. Gustavo Rehder, de 2017, anterior a este curso ter adotado Zephyr. Necessário porque a API `pwm_dt_spec` do Zephyr só escreve o duty (`TPM_CnV`) e não tem como reconfigurar o período (`TPM_MOD`) em runtime — e, neste port `frdm_kl25z` especificamente, não há nó de devicetree para TPM de forma alguma. Já reaproveitado em outra disciplina (PSI3441, Ativ.5, radar) além da Exp2_PSI3422 (motores do Carrinho) — motivo de estar aqui e não dentro de um projeto só. Ver o comentário no topo de `pwm_z42.h` para o racional completo.

## motor/

Motor DC via ponte H (L298N ou similar): 2 GPIOs de direção (Zephyr nativo) + 1 canal PWM de velocidade (via `pwm_z42/`, acima). Totalmente parametrizado por quem inicializa (`gpio_dt_spec` dos pinos de direção, `TPM_MemMapPtr`/canal/período do TPM já configurado) — não assume nenhum pino fixo, então serve para qualquer robô com a mesma topologia de ponte H, não só o Carrinho da Exp2.

Validado na Exp2_PSI3422 (Carrinho, dois motores independentes para curvas).

## ultrassom/

Leitura do HC-SR04 (trigger + echo) por interrupção de borda no pino de echo (não por polling, para não bloquear a CPU durante o eco). Também parametrizado via `gpio_dt_spec` de trigger/echo, sem pino fixo.

Validado na Exp2_PSI3422 (Carrinho, detecção de obstáculos).

## encoder/

Contador de pulsos assinado para o encoder IR HW-201 (par emissor/receptor IR, canal único — não é quadratura). Nesta bancada o "disco" é um marco de papel fixado na roda, não o disco ranhurado de fábrica — 1 pulso por volta completa (a confirmar, ver `odometria/` abaixo). Por interrupção de borda de subida no OUT do sensor, igual ao `ultrassom/` acima. Como um canal só não diz sozinho o sentido de giro, `encoder_init()` recebe um ponteiro `const motor_t *` (ver `motor/`, acima) da mesma roda, e a ISR lê `motor->speed` a cada pulso para decidir se soma (frente) ou subtrai (ré) — comando em ponto morto/freio (`speed == 0`) ignora o pulso, para não contar vibração residual como rotação. Depende de `motor/` por isso (inclui `motor.h`); não depende de `pwm_z42/` diretamente.

Pinos reservados nesta subfamília KL25Z precisam estar em PORTA ou PORTC/PORTD — PORTB/PORTE não geram interrupção por mudança de pino (confirmado em bancada, ver `debug/EncoderCheck` na Exp2_PSI3422).

Contagem de pulsos por segundo validada em bancada por `debug/EncoderCheck` (Exp2_PSI3422), antes do sinal de sentido existir. Lib com sentido introduzida na Exp4_PSI3422 (Aula 4).

## odometria/

Odometria diferencial: converte a variação de pulsos de cada roda (de `encoder/`, acima) em ângulo girado e distância total percorrida do carrinho — a "questão de programação competitiva" do roteiro da Aula 4. Pura aritmética (sem `<math.h>`/libm, sem seno/cosseno) e sem nenhum `#include` de Zephyr — não depende de board nem de framework, então é a lib mais portável do repositório, candidata natural pra um port futuro em ESP32 (ver `experiências/Exp4_PSI3422/debug/IRHW201_ESP32`, ainda não iniciado). Não rastreia posição (x, y) no plano — não pedido pelo roteiro, e exigiria seno/cosseno.

Constantes de calibração (`odometria_calibracao_t`, preenchidas por quem chama, ver `CarrinhoBase/src/main.c`): distância entre rodas medida em bancada (0,20 m); circunferência da roda a partir do raio medido em bancada (5 cm → 2*pi*0,05 ≈ 0,31416 m); pulsos por volta (1, marco de papel fixado na roda, ainda a confirmar girando a roda manualmente N voltas).

Introduzida na Exp4_PSI3422 (Aula 4), junto com `encoder/`. Validada apenas por leitura de código nesta sessão — mesma ressalva de build do `encoder/`, ver `control/relatorio-aula-4.md`.
