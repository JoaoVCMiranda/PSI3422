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
- `kicad/PSI3422_shield.kicad_sym` — símbolo KiCad do shield, ver
  `kicad/README.md` pro próximo passo (KiCad não está instalado nesta
  máquina, símbolo não verificado abrindo de verdade).

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
`experiências/Exp4_PSI3422/CarrinhoBase` (encoders + `lib/odometria`)
por cima, sem mudar a lógica de nenhum dos dois — só a fiação entre
eles em `src/main.c`. `lib/control_fsm/` é cópia verbatim do Exp2 (não
precisou de mudança: "apagar distância" é tratado direto em
`main.c`, não dentro do FSM).

**RUN** = `auto_mode` (desvio reativo por ultrassom frontal, 4
faixas) — é a "travessia de labirinto" cabível com um único sensor de
distância (frontal, sem mapeamento). **STOP** = `freio`.

## Controle/

Cópia de `experiências/Exp2_PSI3422/Controle` com duas teclas novas
em `key_to_cmd()`/`main()`: `i` (mostrar distância percorrida,
imprime a última telemetria já em cache — sem round-trip de rádio
novo) e `c` (apagar distância). `w/a/s/d/x/espaço/q/o` seguem iguais;
`o`/`x` agora nomeados explicitamente RUN/STOP no banner de boot.

Joystick continua desabilitado no código (defeito de hardware
confirmado, ver `experiências/Exp2_PSI3422/PENDENCIAS.md`) — UART
(`script/controle_serial.py`) é o caminho de comando.

## Pendente (fora desta rodada)

- **Build verificado**: esta máquina só tem `framework-zephyr`
  3.40402.0 (Zephyr 4.4.2) instalado, mas todo o código do repositório
  (aqui e em Exp2/Exp4) usa a API antiga do Zephyr 2.7.1
  (`#include <zephyr.h>` sem prefixo `zephyr/`) — nenhum projeto
  Zephyr do repo builda do zero nesta máquina hoje. Não é regressão
  deste merge; ambiente a resolver separadamente antes de `pio run`
  valer como verificação real.
- **Validação de bancada**: RUN (desvio reativo) nunca rodou em
  carrinho de verdade (herdado do Exp2, ver PENDENCIAS.md); comandos
  `i`/`c` novos, sem teste com hardware.
- **Chassi 3D**: decisão adiada (`experiências/Exp3_PSI3422/`,
  `modelo_1`/`modelo_2`) — fora do escopo desta rodada.
- **Esquemático/layout de PCB completo**: `kicad/PSI3422_shield.kicad_sym`
  cobre só o que dá pra derivar do portmap; colocar footprints dos
  módulos e rotear é manual, ver `kicad/README.md`.
- **Distância "salva"**: acumulador em RAM (`odometria_pose_t`), não
  sobrevive a reboot — suficiente pro roteiro por decisão explícita
  (sem NVS/flash).
