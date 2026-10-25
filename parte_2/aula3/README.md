# Aula 3 parte 2 — comunicação, câmera via rede e controle remoto

| Pasta | Raspberry | Computador | O que demonstra |
|---|---|---|---|
| [`tarefa1`](tarefa1/) | `server6c` | `client6c` | `sendUint`/`receiveUint` com `htonl`/`ntohl` |
| [`tarefa2`](tarefa2/) | `server8` | `client8` | `sendVb`/`receiveVb` (tamanho → confirmação → dados) |
| [`tarefa3`](tarefa3/) | `camserver1` | `camclient1` | quadros 480×640 **sem** compressão |
| [`tarefa4`](tarefa4/) | `camserver2` | `camclient2` | quadros 480×640 com **JPEG** (`sendImgComp`) |
| [`tarefa5`](tarefa5/) | `servidor1` | `cliente1` | teclado virtual, 240×320, comando escrito na imagem, gravação t/c |
| [`tarefa6`](tarefa6/) | `servidor2` | `cliente2` | igual à 5, mas move os motores (sem texto na imagem) |

`teclado.hpp`, na raiz de `aula3/`, é compartilhado por `cliente1` e `cliente2` (teclado virtual,
gravador de vídeo e leitura dos argumentos).

## Onde está cada método pedido

Tudo em [`../projeto.hpp`](../projeto.hpp), na classe-base `DEVICE` (escrito
uma vez só, herdado por `SERVER` e `CLIENT`):

- `sendUint` / `receiveUint`: **`htonl`** antes de enviar, **`ntohl`** depois
  de receber. A rede é big endian; ARM e Intel são little endian.
- `sendVb` / `receiveVb`: os 3 passos da apostila — envia o tamanho `t`, o
  receptor aloca `vb.resize(t)` e **devolve `t` como confirmação**, e só então
  vêm os `t` bytes.
- `sendImg` / `receiveImg`: imagem crua (linhas, colunas, pixels) — tarefa 3.
- `sendImgComp` / `receiveImgComp`: `imencode(".jpg")` num `vector<BYTE>` +
  `sendVb`; do outro lado `receiveVb` + `imdecode` — tarefa 4 em diante.
- `testaVb`: a função da apostila.
- `~SERVER` imprime `server: fechei conexao.`.

`sendBytes` agora usa `MSG_NOSIGNAL`: se o computador cair, o `send` retorna
erro em vez de matar o processo com `SIGPIPE`. Isso garante que o `servidor2`
desliga os motores (ver abaixo).

## Compilar

    ./aula3.sh build          # compila o que faz sentido para este local

ou, na mão (de dentro de `aula3/`; cada programa compila na pasta da sua tarefa):

    raspberry$  (cd tarefa1 && compila server6c -ocv)
    raspberry$  (cd tarefa2 && compila server8 -ocv)
    raspberry$  (cd tarefa3 && compila camserver1 -ocv)
    raspberry$  (cd tarefa4 && compila camserver2 -ocv)
    raspberry$  (cd tarefa5 && compila servidor1 -ocv)
    raspberry$  (cd tarefa6 && compila servidor2 -ocv -w)

    computador$ (cd tarefa1 && compila client6c -ocv -v3)
    computador$ (cd tarefa2 && compila client8 -ocv -v3)
    computador$ (cd tarefa3 && compila camclient1 -ocv -v3)
    computador$ (cd tarefa4 && compila camclient2 -ocv -v3)
    computador$ (cd tarefa5 && compila cliente1 -ocv -v3)
    computador$ (cd tarefa6 && compila cliente2 -ocv -v3)

## Rodar

Sempre o servidor primeiro. Troque o IP pelo do Raspberry.

    raspberry$ tarefa1/server6c             computador$ tarefa1/client6c 192.168.15.44
    raspberry$ tarefa2/server8              computador$ tarefa2/client8 192.168.15.44
    raspberry$ tarefa3/camserver1           computador$ tarefa3/camclient1 192.168.15.44
    raspberry$ tarefa4/camserver2           computador$ tarefa4/camclient2 192.168.15.44
    raspberry$ tarefa5/servidor1            computador$ tarefa5/cliente1 192.168.15.44 [video.avi] [t/c]
    raspberry$ tarefa6/servidor2            computador$ tarefa6/cliente2 192.168.15.44 [video.avi] [t/c]

- **ESC** com a janela do computador em foco termina os dois programas
  (tarefas 3 a 6).
- Teclado virtual: **segure** o botão esquerdo do mouse sobre um botão; ao
  soltar, volta para "0" (nenhum botão). Dá para arrastar de um botão a outro.
- Vídeo: `t` (default) grava a janela inteira, com o teclado (560×240);
  `c` grava só a câmera (320×240). Sem nome de vídeo, não grava. O fps do
  vídeo é **medido** nos primeiros 20 quadros (depende da rede), para o vídeo
  não sair acelerado.

### Protocolo das tarefas 3 a 6

Trava-trava (*lock-step*): o servidor só captura e manda um quadro depois de
receber 1 byte do cliente.

| Byte | Significado |
|---|---|
| `0` | nenhum botão — no `servidor2`, **para os motores** |
| `1`..`9` | botão virtual (no `servidor1` é escrito no quadro) |
| `s` | ESC: termina |

Nas tarefas 3 e 4 o cliente só manda `0` ou `s`.

## Tarefa 6 — motores e calibração

Mapeamento e potências estão no topo de [`tarefa6/servidor2.cpp`](tarefa6/servidor2.cpp):

| Botão | Esquerda | Direita |
|---|---|---|
| 8 frente | +100% | +100% |
| 2 ré | −100% | −100% |
| 7 / 9 curva | 35% / 100% | 100% / 35% |
| 4 / 6 giro no eixo | −70% / +70% | +70% / −70% |
| 1 / 3 ré em curva | −35% / −100% | −100% / −35% |
| 0, 5 | para | para |

(100% = potência calibrada daquela roda naquele sentido.)

**Calibração, nesta ordem:**

1. `tarefa6/servidor2` e segure **8**. Se alguma roda girar para trás, troque os
   pinos daquela roda (`ESQ_FRENTE`/`ESQ_TRAS` ou `DIR_FRENTE`/`DIR_TRAS`).
   Se o carrinho girar para o lado errado no **4**, troque esquerda ↔ direita.
2. Linha reta: passe as potências como argumento, sem recompilar:

       raspberry$ tarefa6/servidor2 frenteEsq frenteDir reEsq reDir
       raspberry$ tarefa6/servidor2 80 74 80 76

   Se puxa para a direita indo para frente, a roda esquerda está mais forte:
   diminua `frenteEsq` (ou aumente `frenteDir`). Repita com o **2** para a ré.
3. Copie os valores finais para as constantes do `tarefa6/servidor2.cpp`.

**Segurança:** os motores são desligados quando o programa termina por
qualquer motivo — `s`, queda da conexão (`erro()` → `atexit`), Ctrl+C ou
`kill` (SIGTERM). `kill -9` não dá chance de reagir: nesse caso, ou se
ficarem ligados por qualquer outro motivo, `../aula2/paramotor`.

## Como medir a latência (tarefas 3 e 4)

1. No computador, abra um cronômetro com milissegundos (ex.: busque
   "stopwatch" no navegador) **ao lado** da janela do `camclient`.
2. Aponte a câmera do Raspberry para o cronômetro.
3. Tire uma foto (celular) da tela inteira: ela mostra o cronômetro de verdade
   e, na janela, o cronômetro "atrasado" visto pela câmera.
4. Latência = diferença entre os dois valores. Faça umas 3 fotos e tire a média.

O `camclient` imprime o fps a cada 2 s, o que ajuda a comparar as duas versões.
Referência da apostila: ~2 s sem compressão, ~0,5 s e ~15 fps com JPEG.

| | Latência medida | fps |
|---|---|---|
| camserver1/camclient1 (cru) | _preencher_ | _preencher_ |
| camserver2/camclient2 (JPEG) | _preencher_ | _preencher_ |

## Testes já feitos (loopback, em container Linux com OpenCV 4)

- `server6c`/`client6c` e `server8`/`client8`: saída idêntica à esperada da
  apostila, inclusive o "Nao recebi 100000 bytes 1" proposital.
- Tarefas 3 a 6 com câmera falsa: ESC encerra os dois lados; vídeos `t`
  (560×240) e `c` (320×240) abrem e têm os quadros certos.
- `servidor2` com wiringPi simulado: cada comando gera o PWM da tabela acima,
  e os motores são parados quando o cliente cai no meio de um movimento.

Falta validar no hardware: câmera real, latência, rede wifi e calibração.
