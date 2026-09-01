# kicad/ — PCB do shield

Dois conjuntos de arquivos aqui, ainda não reconciliados entre si
(próximo passo, ver "Pendente" no fim):

- **`Aula3_PCB.*`, `FRDM-KL25Z.kicad_sym`/`.kicad_mod`,
  `design-block-lib-table`, `model.png`** — projeto KiCad de verdade
  (movido de `experiências/Exp3_PSI3422/Kicad/`), com PCB já roteada
  (`Aula3_PCB.kicad_pcb`, ~5400 linhas) e um símbolo/footprint
  dedicado da FRDM-KL25Z. `Aula3_PCB.kicad_sch` ainda é praticamente
  vazio (13 linhas) — o trabalho até aqui parece ter ido direto pro
  editor de PCB.
- **`PSI3422_shield.kicad_sym`** — **gerado** por
  `../tools/gen_pinmap.py` a partir de `../pinmap.yaml` (mesma fonte
  que gera `../pinmap.h`, usado pelo código de `Carrinho`/`Controle`).
  Não editar à mão: editar o YAML e rodar
  `python3 tools/gen_pinmap.py` de dentro de `projeto_final/`.

KiCad 9.0.2 já está instalado nesta máquina (`kicad-cli` disponível) —
ainda não usei pra abrir/validar nenhum dos dois conjuntos de verdade,
fica pro próximo passo.

## Pendente (próxima rodada, avisado pelo usuário)

- **Validar** `Aula3_PCB.*` abrindo no KiCad de verdade — inclui
  conferir `design-block-lib-table`, que aponta pra um caminho
  absoluto do Windows (`C:/Users/caean/Desktop/FRDM-KL25Z ver6`,
  provavelmente de onde o símbolo/footprint originais vieram) —
  provavelmente inofensivo (metadado de biblioteca de "design
  blocks", não usado pelo `.kicad_pcb` em si) mas não confirmado ainda.
- **Single source of truth**: hoje `pinmap.yaml`/`gen_pinmap.py`
  geram um símbolo próprio (`PSI3422_shield.kicad_sym`) só com os
  sinais do portmap, separado do símbolo `FRDM-KL25Z` de verdade usado
  no `Aula3_PCB.kicad_sch`/`.kicad_pcb`. Reconciliar os dois é o
  próximo passo — provavelmente gerar os pinos DENTRO do símbolo
  `FRDM-KL25Z.kicad_sym` já usado na PCB real, em vez de manter dois
  símbolos paralelos.

## O que o gerador NÃO faz (trabalho manual que continua no KiCad)

- Adicionar os footprints dos outros componentes do shield: ponte H
  (L298N ou similar), HC-SR04, módulo nRF24L01+, os dois HW-201 —
  esses não têm pino fixo na KL25Z (são módulos externos conectados
  pelos sinais do símbolo gerado), então entram como símbolos/
  footprints padrão de biblioteca KiCad, não gerados daqui.
- Desenho elétrico dos módulos e roteamento físico exigem julgamento
  humano (posicionamento, trilhas de potência dos motores separadas
  das de sinal, etc.) — o gerador só resolve a parte mecânica de
  derivar pinos do portmap, não substitui isso.

Isso cobre a pendência da Aula 3 (`README.md` raiz) até o ponto onde
dá pra automatizar a partir do portmap.
