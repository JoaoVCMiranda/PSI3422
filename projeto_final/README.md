# projeto_final — consolidação de produção (Aulas 3-6)

Reúne os módulos "de produção" do carrinho — firmware, portmap único
e o ponto de partida da PCB — cobrindo o roteiro das Aulas 3 a 6 do
`README.md` raiz. Arquitetura: par **Carrinho + Controle** (like
`experiências/Exp2_PSI3422/`), não um board único — ver módulos abaixo.

## Fonte única do portmap

`pinmap.yaml` é a fonte de verdade dos pinos da FRDM-KL25Z (Carrinho
e Controle). `python3 tools/gen_pinmap.py` (de dentro desta pasta)
gera, de forma determinística:

- `pinmap.h` — incluído por `Carrinho/src/main.c` e
  `Controle/src/main.c`.
- `Pinmap.md` — tabela por board (substitui a manutenção manual
  duplicada em `experiências/Exp2_PSI3422/Pinmap.md` e
  `experiências/Exp4_PSI3422/pinmap.md`).
- `kicad/PSI3422_shield.kicad_sym` — símbolo KiCad do shield, ainda
  não reconciliado com o projeto KiCad de verdade (`kicad/Aula3_PCB.*`,
  movido de `experiências/Exp3_PSI3422/Kicad/`) — ver `kicad/README.md`
  pro estado de cada um e o próximo passo (validar no KiCad, que já
  está instalado nesta máquina agora, e unificar os dois símbolos).

Editar `pinmap.yaml`, nunca os três arquivos gerados.

## Protocolo — `protocol.h`

Extensão mínima de `experiências/Exp2_PSI3422/protocol.h`: RUN/STOP
reaproveitam `auto_mode`/`freio` (já existiam); dois campos novos —
`apagar_seq` (comando) e `dist_percorrida_cm` (telemetria) — cobrem
"mostrar distância"/"apagar distância" do roteiro. Ver comentários em
`protocol.h` pro racional de cada um.

## Carrinho/

Zephyr, FRDM-KL25Z. Junta `experiências/Exp2_PSI3422/Carrinho`
(motores, ultrassom, rádio, `lib/control_fsm`) com
`experiências/Exp4_PSI3422/CarrinhoBase` (encoder + `lib/odometria`)
por cima, sem mudar a lógica de nenhum dos dois — só a fiação entre
eles em `src/main.c`. `lib/control_fsm/` é cópia verbatim do Exp2 (não
precisou de mudança: "apagar distância" é tratado direto em
`main.c`, não dentro do FSM).

Só `ENCODER_R` está no pinmap atual — o carrinho não tem mais o
encoder esquerdo fisicamente montado. `lib/odometria` (contrato de
odometria diferencial, dois deltas) não foi alterada: `src/main.c`
passa o delta de `ENCODER_R` pros dois parâmetros, o que zera
`theta_rad` (campo não usado hoje) e mantém `distancia_percorrida_m`
correta. Ver comentário no topo de `Carrinho/src/main.c`.

**RUN** = `auto_mode` (desvio reativo por ultrassom frontal, 4
faixas) — é a "travessia de labirinto" cabível com um único sensor de
distância (frontal, sem mapeamento). **STOP** = `freio`.

### Parâmetros do carrinho

Distância entre as rodas: 17cm
Raio da roda: 3.5cm

## Controle/

Cópia de `experiências/Exp2_PSI3422/Controle` com duas teclas novas
em `key_to_cmd()`/`main()`: `i` (mostrar distância percorrida,
imprime a última telemetria já em cache — sem round-trip de rádio
novo) e `c` (apagar distância). `w/a/s/d/x/espaço/q/o` seguem iguais;
`o`/`x` agora nomeados explicitamente RUN/STOP no banner de boot.

Joystick continua desabilitado no código (defeito de hardware
confirmado, ver `experiências/Exp2_PSI3422/PENDENCIAS.md`) — UART
(`script/controle_serial.py`) é o caminho de comando.

## Monitoramento/

Terceiro board: só rádio nRF24L01+ em modo recepção — ouve a
telemetria (`radio_telemetry_t`) que o Carrinho já transmite a cada
ciclo (~50ms) e imprime `dist`/`dutyL`/`dutyR`/`percorrida`, sem
transmitir nenhum `radio_cmd_t` (isso continua sendo papel exclusivo
do Controle). Primeiro passo do roteiro de debug em ciclos rápidos:
recepção de metadados -> conferir contagem de distância -> conferir
o modelo de desvio do Carrinho resolvendo o percurso -> percurso real
(geometria a definir).

Mesmo pinmap de rádio/LEDs de Carrinho/Controle (`../pinmap.h`).
CAVEAT não validado em bancada: endereço/canal do nRF24 são únicos e
fixos pro projeto inteiro (`lib/nrf24/nrf24.c`) e auto-ACK está
ligado — com três rádios (Carrinho, Controle, Monitoramento) no
mesmo endereço/canal, Monitoramento passa a mandar ACK pros pacotes
do Carrinho ao mesmo tempo que o Controle, o que pode colidir no ar
e aumentar reenvios/timeouts vistos pelo Carrinho. Não deveria
derrubar o link (retransmissão já existe), mas fica por confirmar
com os três rádios ligados juntos.

## Pendente (fora desta rodada)

- ~~**`nrf24_init()` sempre retornava -134 (-ENOTSUP)**~~ RESOLVIDO
  (sessão de 22/09/2026) — provável causa raiz de TODO o histórico de
  "rádio não conecta" do projeto. `nrf24_init()` chamava
  `gpio_pin_interrupt_configure_dt(&nrf24_irq, GPIO_INT_EDGE_TO_ACTIVE)`
  em RADIO_IRQ (PTB8, PORTB). Neste board/framework
  (`dts/arm/nxp/nxp_kl25z.dtsi`), só `gpioa` (PORTA) e `gpiod`
  (PORTD) têm a property `interrupts` — `gpiob`/`gpioc`/`gpioe` não;
  o driver (`drivers/gpio/gpio_mcux.c`) só liga `GPIO_INT_ENABLE`
  quando essa property existe, e sem ela
  `gpio_pin_interrupt_configure()` retorna `-ENOTSUP` (-134) pra
  qualquer pino do port. Como RADIO_IRQ está em PORTB, essa chamada
  **sempre falhava**, e o `if (ret < 0) return ret;` logo depois
  fazia `nrf24_init()` abortar ali — `nrf24_configure()` (endereço,
  canal, CRC) nunca rodava, CE nunca ia pra 1: o rádio nunca foi
  configurado de verdade, em nenhum board, em nenhuma sessão. A
  interrupção era código morto (o semáforo que ela dispararia nunca
  era esperado em lugar nenhum — `nrf24_send()`/`nrf24_receive()` só
  fazem polling direto de `gpio_pin_get_dt()`), removida de
  `lib/nrf24/nrf24.c` — RADIO_IRQ agora só é `GPIO_INPUT` comum, sem
  callback/interrupção. `pio run` verificado em todos os projetos que
  usam `lib/nrf24` depois do fix. Ainda sem validação de bancada —
  mas isso muda completamente a prioridade dos diagnósticos SPI
  (`nrf24_read_status()`) e de timing/protocolo levantados antes:
  aquilo tudo era irrelevante enquanto o rádio nem chegava a ser
  inicializado.
- **Dois pinmaps agora (`pinmap.yaml` vs `pinmap_carrinho.yaml`)**:
  a PCB do Carrinho já existia antes do fix de RADIO_SCK/MOSI/MISO
  (item abaixo) e foi roteada em cima do pinmap de antes dele (commit
  6e70945: PTD6/PTB9/PTD7); o Controle foi fiado à mão depois do fix,
  em cima do `pinmap.yaml` atual (PTC5/PTC6/PTC7). `pinmap_carrinho.yaml`
  é um fork só desses 3 pinos — motor/ultrassom/encoder/CE/CSN/IRQ/LED
  continuam idênticos nos dois (`pinmap_carrinho.h` e `pinmap.h` só
  diferem no include guard, ver diff). `tools/gen_pinmap.py` agora
  aceita um YAML alternativo como argumento (`gen_pinmap.py
  pinmap_carrinho.yaml`) e deriva os nomes de saída — comportamento
  padrão (sem argumento) inalterado. **Ainda em aberto**: como
  RADIO_SCK/MOSI/MISO são `tipo: doc` (sem define em pinmap.h),
  reescrever o YAML sozinho não muda nenhum binário — se a PCB do
  Carrinho really tem o nRF24 fiado em PTD6/PTB9/PTD7, nenhuma rota de
  hardware SPI0/SPI1 da KL25Z alcança esses 3 pinos (só PORTC/PORTA
  pro SPI0), então o Carrinho precisaria de SPI bit-banged nesses
  pinos especificamente — ainda não implementado, decisão pendente até
  confirmar em bancada (ver `nrf24_read_status()`, próximo item).
- ~~**RADIO_SCK/MOSI/MISO documentados errado em pinmap.yaml**~~
  RESOLVIDO (sessão de 22/09/2026): pinmap.yaml dizia PTD6/PTB9/PTD7,
  mas `nrf24_init()` chama `spi_init(SPI_0, ALT_0, ...)`
  (`lib/spi/spi.c`), que hardcoda PTC5/PTC6/PTC7 via CMSIS direto —
  as únicas duas rotas de hardware do SPI0 na KL25Z são PORTC (ALT_0)
  ou PORTA (ALT_1), então PTD6/PTB9/PTD7 nunca foi uma opção válida
  (erro de digitação antigo, não uma rota alternativa real).
  `pinmap.yaml`/`Pinmap.md` corrigidos pra PTC5/PTC6/PTC7 — esses
  pinos são `tipo: doc` (spi_init() não passa por devicetree, sem
  define em pinmap.h), então a correção não muda nenhum binário, só a
  documentação. **Mas se o shield físico foi roteado pelos valores
  antigos (PTD6/PTB9/PTD7), o SPI do rádio está fisicamente errado na
  placa** — conferir continuidade PTC5→SCK/PTC6→MOSI/PTC7→MISO do
  nRF24L01+ é prioridade de bancada, antes de qualquer outro debug de
  rádio (se for isso, explicaria falha de conexão de forma muito mais
  direta que qualquer hipótese de timing).
- ~~**Canais PWM dos motores trocados**~~ RESOLVIDO (sessão de
  22/09/2026): `debug/debug_ponte_H_encoder/src/main.c` já tinha
  descoberto e corrigido isso em bancada ("MOTOR_L_ENA_CH/MOTOR_R_ENB_CH
  estavam trocados"), mas a correção nunca voltou pro `pinmap.yaml` de
  produção. Confirmado de forma independente contra o datasheet
  (`MKL25Z128VLK4-pinctrl.dtsi`): PTD2 (MOTOR_L_ENA) é fisicamente
  TPM0_CH2, não CH1; PTA4 (MOTOR_R_ENB) é TPM0_CH1, não CH2. Com os
  canais trocados, o duty de cada motor saía fisicamente no pino do
  *outro* motor (direção in1/in2 continuava certa — só a velocidade
  ficava cruzada) — candidato forte pra explicar "carrinho anda torto"
  (PENDENCIAS.md do Exp2). `pinmap.yaml` corrigido e
  `pinmap.h`/`Pinmap.md` regerados via `gen_pinmap.py`; `pio run`
  verificado nos três projetos depois do fix. Ainda sem confirmação em
  bancada com o carrinho de verdade.
- ~~**Leitura dupla do ultrassom em auto_mode**~~ RESOLVIDO (mesma
  sessão): `Carrinho/src/main.c` já chama `ultrassom_read()` a cada
  ciclo (telemetria); `control_fsm_apply()` chamava de novo
  internamente em auto_mode, cada chamada bloqueando até
  `ECHO_TIMEOUT_MS` (30ms) — quase dobrava a latência da malha de
  desvio reativo. `control_fsm.c` agora reusa `sensor->distance` (já
  atualizado por main.c) em vez de disparar o sensor de novo.
- **`nrf24_receive(timeout)` era ignorado**: a implementação sempre
  esperava 100ms fixos, não importa o valor passado — sem efeito
  observável hoje (todo chamador usa `K_MSEC(100)`), mas quebraria em
  silêncio um `K_FOREVER`/outro valor. Corrigido em `lib/nrf24/nrf24.c`
  pra usar o `timeout` de verdade (`K_TICKS_FOREVER` tratado à parte).
- **Colisão de ACK com 3 rádios (caveat do `Monitoramento/`)**:
  mitigado, não eliminado — `lib/nrf24` ganhou `nrf24_set_auto_ack()`
  (liga/desliga EN_AA sem reconfigurar o resto do rádio) e
  `Monitoramento/src/main.c` desliga o próprio auto-ACK logo após
  `nrf24_init()` (e de novo a cada reconexão). Como Monitoramento só
  recebe, nunca precisou responder ACK — agora só o Controle responde,
  eliminando a colisão por construção em vez de só confiar em
  retransmissão. Ainda sem teste de bancada com os três rádios ligados
  juntos.
- ~~**Build verificado**~~ RESOLVIDO: causa raiz não era a máquina nem
  a instalação (pip vs. `uv tool install platformio`) — era
  `platform = freescalekinetis` sem versão em cada `platformio.ini`,
  que sempre resolve pra última release do registry. Isso "funcionava
  por acidente" enquanto a última release continuava pinando
  `framework-zephyr ~2.20701.0` (Zephyr 2.7.1, API antiga
  `#include <zephyr.h>`, a que todo o código daqui e do Exp2/Exp4
  usa). Em 31/08/2026 o registry publicou
  `freescalekinetis@11.0.0`, que pina `framework-zephyr ~3.40402.0`
  (Zephyr 4.4.2, exige `#include <zephyr/...>`) — dali em diante
  qualquer `pio run` nesta família de projetos passou a falhar,
  inclusive includes relativos (`../../protocol.h` etc.), porque a
  falha no primeiro `#include <zephyr.h>` aborta a unit de compilação
  antes de chegar nos outros. `Carrinho/`, `Controle/` e
  `Monitoramento/` agora pinam `platform = freescalekinetis@10.0.0`
  (última versão que pina Zephyr 2.7.1) em `platformio.ini` — `pio
  run` verificado com sucesso nos três (`[SUCCESS]`, firmware.elf/.bin
  gerados) nesta máquina. `experiências/Exp1-4_PSI3422/` continuam
  sem o pin — mesmo problema vai aparecer lá se alguém rodar `pio run`
  neles agora.
- **Validação de bancada**: RUN (desvio reativo) nunca rodou em
  carrinho de verdade (herdado do Exp2, ver PENDENCIAS.md); comandos
  `i`/`c` novos, sem teste com hardware. `Monitoramento/` (recepção
  de telemetria) também sem teste com hardware — inclui o caveat de
  colisão de ACK com três rádios, ver seção acima.
- **Calibração da odometria**: `Carrinho/src/main.c` atualizado para
  as medidas reais (17cm entre rodas, roda raio 3,5cm — ver seção
  "Parâmetros do carrinho" acima), substituindo as estimativas
  antigas (20cm/5cm) herdadas do Exp4. Ainda sem validação de bancada
  do valor absoluto de `dist_percorrida_cm` contra uma distância
  conhecida percorrida — é o próximo passo do roteiro (ver
  `Monitoramento/`, acima).
- **Chassi 3D**: decisão adiada (`experiências/Exp3_PSI3422/`,
  `modelo_1`/`modelo_2`) — fora do escopo desta rodada.
- **PCB**: `kicad/Aula3_PCB.kicad_pcb` já tem uma placa roteada, mas o
  esquemático (`.kicad_sch`) está praticamente vazio e o símbolo
  gerado (`PSI3422_shield.kicad_sym`) ainda não foi conciliado com o
  símbolo real (`FRDM-KL25Z.kicad_sym`) usado nela — validar abrindo
  no KiCad (já instalado) é o próximo passo, ver `kicad/README.md`.
- **Distância "salva"**: acumulador em RAM (`odometria_pose_t`), não
  sobrevive a reboot — suficiente pro roteiro por decisão explícita
  (sem NVS/flash).
