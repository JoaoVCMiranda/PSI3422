# kicad/ — ponto de partida da PCB do shield

`PSI3422_shield.kicad_sym` é **gerado** por `../tools/gen_pinmap.py` a
partir de `../pinmap.yaml` — mesma fonte que gera `../pinmap.h` (o
código do `Carrinho`/`Controle` usa). Não editar o `.kicad_sym` à mão:
editar o YAML e rodar o gerador de novo (`python3 tools/gen_pinmap.py`
de dentro de `projeto_final/`).

## Ressalva importante

Esta máquina não tem KiCad instalado (`kicad-cli` ausente, nenhum
pacote instalado) — o `.kicad_sym` foi gerado por inspeção do formato
de arquivo (S-expression) do KiCad 6/7, **sem abrir/validar num KiCad
de verdade**. Antes de confiar nele:

1. Abra o KiCad, vá em **Preferences → Manage Symbol Libraries**,
   adicione `PSI3422_shield.kicad_sym` como biblioteca de projeto ou
   global.
2. Confirme que o símbolo `PSI3422_shield` aparece e abre sem erro no
   editor de símbolos (Symbol Editor). Se der erro de parsing, é o
   primeiro bug a reportar — o gerador precisa de ajuste.
3. Confira visualmente se os ~22 pinos batem com `../Pinmap.md`
   (nome = sinal, número = `PT<porta><pino>` físico da KL25Z).

## O que o gerador NÃO faz (trabalho manual que continua no KiCad)

- Criar um projeto/esquemático (`.kicad_pro`/`.kicad_sch`) de verdade
  — o símbolo gerado é uma peça pra colocar numa folha, não a folha.
- Adicionar os footprints dos outros componentes do shield: ponte H
  (L298N ou similar), HC-SR04, módulo nRF24L01+, os dois HW-201 —
  esses não têm pino fixo na KL25Z (são módulos externos conectados
  pelos sinais do símbolo gerado), então entram como símbolos/
  footprints padrão de biblioteca KiCad, não gerados daqui.
- Rotear a placa (layout, `.kicad_pcb`).

## Próximo passo sugerido

1. `File → New Project` no KiCad, novo esquemático.
2. Importar `PSI3422_shield.kicad_sym` (passo acima) e colocar uma
   instância na folha.
3. Colocar os símbolos dos módulos externos (ponte H, HC-SR04, nRF24,
   2x HW-201) e fiar cada um nos pinos correspondentes do símbolo do
   shield, seguindo `../Pinmap.md`.
4. Rodar ERC, gerar netlist, ir pro editor de PCB e rotear — nessa
   ordem, tudo manual a partir daqui.

Isso cobre a pendência da Aula 3 (`README.md` raiz) até o ponto onde
dá pra automatizar a partir do portmap; o desenho elétrico dos módulos
e o roteamento físico exigem julgamento humano (posicionamento,
trilhas de potência dos motores separadas das de sinal, etc.) que não
faz sentido gerar.
