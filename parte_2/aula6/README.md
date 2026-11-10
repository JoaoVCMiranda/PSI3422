# Aula 6 parte 2 — controle manual + automático (fase 7)

| Pasta | Raspberry | Computador | Lição |
|---|---|---|---|
| [`tarefa1`](tarefa1/) | `servidor7` | `cliente7` | fase 7: manual (fase 2) e automático (fase 6), trocando pelo botão M/A |

Como nas aulas 4 e 5, **cada programa é um arquivo único**, comentado passo a
passo, com etiquetas de origem (`[OpenCV]`, `[teclado.hpp]` …).

## Arquivos necessários

O `cliente7` usa o `quadrado.png` e os 4 arquivos do MNIST de
**`parte_2/aula5/`**, para não duplicar 55 MB. Ele procura em `.`, `..`,
`../aula5` e `../../aula5`.

## Compilar e rodar

    ./aula6.sh build      # no computador: cliente7; no Raspberry: servidor7

Ou na mão (de dentro de `aula6/`):

    computador$ (cd tarefa1 && compila cliente7 -ocv -v3 -omp)
    raspberry$  (cd tarefa1 && compila servidor7 -ocv -w)

    raspberry$  tarefa1/servidor7 [frenteEsq frenteDir reEsq reDir]
    computador$ tarefa1/cliente7 100.70.100.51 [videosaida.avi] [t/c]

## Como usar

O sistema **começa no modo manual** (figura 13a).

| Ação | Mouse | Teclado do PC |
|---|---|---|
| andar (segure) | botões 7 8 9 / 4 6 / 1 2 3 | números, setas, `q w e / a d / z x c` |
| **M/A**: troca manual ↔ automático | botão do meio (5) | `5`, espaço ou `s` |
| terminar os dois programas | — | ESC |

Para usar:
1. Leve o carrinho **no manual** até o começo da trajetória.
2. Aperte **M/A** para passar ao **automático**: ele segue as placas e obedece aos dígitos, como na fase 6.
3. Ao ler **0 ou 1** (fim da trajetória), ele **volta sozinho ao manual**, parado.
4. A **qualquer momento**, M/A devolve o controle ao manual, mesmo no meio de um giro.

## Como o código está organizado

O `cliente7.cpp` tem seis partes:

1. **DETECTOR DA PLACA**, **LEITOR DO DÍGITO** e **MÁQUINA DE ESTADOS**: cópias
   exatas do `../aula5/tarefa2/cliente6.cpp` (conferidas com `diff`). A
   máquina da figura 6 é o modo automático inteiro, incluindo os tempos
   calibrados na aula 5.
2. **MODO MANUAL** (novo): `velocidadeManual(botao)`, a tabela do `servidor2`
   da aula 3 (frente, ré, curvas, giro no eixo). Na fase 2, a tradução
   botão → rodas era feita no Raspberry. Aqui ela vem para o computador, e o
   servidor recebe sempre `'m' esq dir`, nos dois modos.
3. **MODOS** (novo): a classe `CONTROLE`, que implementa a figura 14.
   - O modo automático é uma máquina **dentro** de um estado da outra. Entrar
     no automático cria uma `MAQUINA` nova, que começa no estado 0 com o
     cronômetro zerado.
   - Quando a `MAQUINA` chega ao estado 4 FIM (dígito 0/1), o `CONTROLE` volta
     ao manual. Por isso o estado 4 do `cliente6`, que esperaria a placa
     sumir, nunca roda aqui.
   - O M/A troca o modo a cada **aperto** (de "não 5" para "5"), e não
     enquanto está apertado. Sem isso, segurar o botão trocaria o modo a cada quadro.
4. **TELA E PROGRAMA**: teclado (240) + câmera (320) + painéis do dígito (120),
   como a figura 13. O botão do meio mostra **M** ou **A**.
   - No manual, o detector e o leitor **não rodam**, e o controle fica mais
     rápido. A câmera aparece crua e os painéis ficam cinzas.

O `servidor7` é o `servidor6` com os nomes trocados. O protocolo e o descarte
de quadros velhos são os mesmos.

Cada troca de modo é impressa no terminal:

    modo MANUAL -> AUTOMATICO (apertou M/A)
    estado 2 LE      -> 4 FIM      (li o digito 0: fim do caminho, parado)
    modo AUTOMATICO -> MANUAL (terminou a trajetoria (digito 0))

## Teste sem o carrinho

Sem janela: um programa de teste usa o próprio `cliente7.cpp`, entrega ao
`CONTROLE` uma sequência de botões no lugar do teclado e usa o
`capturado.avi` como câmera (o quadro congela com as rodas paradas, como na
aula 5).

- **manual:** botão 8 → rodas +1,00/+1,00; nenhum botão → paradas.
- **M/A segurado por 7 quadros:** troca o modo **uma** vez.
- **automático:** segue a placa, lê 3 (180° à direita) e 9 (90° à direita);
  ao ler 0, **volta sozinho ao manual**.
- **M/A no meio do automático:** volta ao manual no mesmo quadro, com as rodas paradas.

O `servidor7` compila com wiringPi simulado. Falta testar no carrinho.
