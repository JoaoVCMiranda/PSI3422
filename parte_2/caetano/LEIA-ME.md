# Caetano — onde foi parar o que você escreveu

Seus arquivos originais continuam **intactos nesta pasta**, como referência.
O que roda de verdade está um nível acima, em `parte_2/`. Este documento é o
mapa entre as duas coisas.

## Mapa rápido

| Seu arquivo | Equivalente que roda | O que aconteceu |
|---|---|---|
| `projeto.cpp` | `../projeto.hpp` | reescrito com classe-base `DEVICE`; **seu retry em `EINTR` foi adotado** |
| `server6b.cpp` | `../aula2/server6b.cpp` | praticamente igual ao seu (é o código da apostila) |
| `client6b.cpp` | `../aula2/client6b.cpp` | idem |
| `raspvid.cpp` | `../aula2/raspvid.cpp` | reescrito: nome do vídeo por argumento + fps medido |
| `pwmroda4.cpp` | `../aula2/pwmroda4.cpp` | igual ao seu, mas repetindo a sequência 2× como na apostila |

Para rodar qualquer coisa: `cd ../aula2 && ./aula2.sh` (mostra a ajuda).

---

## 1. O que do seu código foi adotado

**O retry em `EINTR`.** Seus laços `enviaTudo`/`recebeTudo` tratam o caso de
`send`/`recv` voltarem com `-1` e `errno == EINTR` — um sinal interrompeu a
chamada, e o certo é repetir, não abortar. O meu código abortava. Está
corrigido em `../projeto.hpp`:

```cpp
ssize_t n = send(fd(), buf+enviados, nBytesToSend-enviados, 0);
if (n == -1) {
  if (errno == EINTR) continue;  // <- veio do seu codigo
  erro("Erro: send: " + string(strerror(errno)));
}
```

A ideia central dos dois laços (`while` até completar todos os bytes, porque
`send`/`recv` podem transferir menos que o pedido) é a mesma nos dois códigos —
e é exatamente o ponto da lição #3/3.

---

## 2. Dois erros que impedem o `projeto.cpp` de compilar

Verificado com `g++ -fsyntax-only`:

```
projeto.hpp:1: error: unterminated #ifndef
projeto.hpp:177:30: error: expected class-name before '{' token
```

**(a) Falta o `#endif`.** O arquivo abre com `#ifndef PROJETO_HPP` / `#define
PROJETO_HPP` na linha 1-2, mas termina na linha 242 sem fechar com `#endif`.

**(b) `class CLIENT : public DEVICE` na linha 177, mas `DEVICE` não existe.**
Não está definido no seu arquivo, nem no `raspberry.hpp` do professor
(conferi). Provavelmente é resquício de uma tentativa de fazer a parte opcional
da apostila (a classe abstrata da figura 11b). Note que seu `SERVER` **não**
deriva de `DEVICE` — só o `CLIENT` — então nem a intenção ficou consistente.

**(c) Menor:** o arquivo se chama `projeto.cpp`, mas seus `server6b.cpp` e
`client6b.cpp` fazem `#include "projeto.hpp"`. Como é um header (só
declarações, com include guard), o nome certo é `.hpp`.

Corrigindo esses três, seu código compila.

---

## 3. Por que o `projeto.hpp` final ficou diferente

Fiz a **parte opcional** da apostila: uma classe abstrata `DEVICE` da qual
`SERVER` e `CLIENT` derivam — era o que seu `class CLIENT : public DEVICE`
tentava alcançar.

E fui um passo além. Na figura 11b da apostila, `sendBytes`/`receiveBytes`
aparecem duplicados nas duas classes derivadas — que é o que o seu código faz
também, chamando as funções livres `enviaTudo`/`recebeTudo` com o socket certo:

```cpp
// seu SERVER                          // seu CLIENT
void sendBytes(int n, BYTE *buf) {     void sendBytes(int n, BYTE *buf) {
  enviaTudo(new_fd, n, buf);             enviaTudo(sockfd, n, buf);
}                                      }
```

A observação é que a **única** diferença entre os dois é qual socket usar.
Então `DEVICE` expõe isso como um virtual puro e implementa os dois métodos
uma vez só:

```cpp
class DEVICE {
protected:
  virtual int fd() const = 0;          // new_fd no SERVER, sockfd no CLIENT
public:
  void sendBytes(int n, const BYTE *buf);     // escrito UMA vez
  void receiveBytes(int n, BYTE *buf);        // escrito UMA vez
};
```

Isso responde à pergunta que o professor deixa em aberto na p. 30 da apostila
("É possível escrever sendBytes e receiveBytes também uma única vez?").

Também já estão lá `sendUint`/`receiveUint` (com `htonl`/`ntohl`) e
`sendVb`/`receiveVb` — que são as primeiras lições da **aula 3**.

---

## 4. Por que o `raspvid.cpp` final ficou diferente

Seu código funciona e compila limpo. Dois pontos que a apostila cobra:

**(a) O nome do vídeo é argumento, não fixo.** A apostila (p. 7) especifica a
chamada `raspvid saida.avi`. O seu grava sempre em `video.avi`.

**(b) O fps.** Você fixou 30:

```cpp
VideoWriter vo("video.avi", VideoWriter::fourcc('X','V','I','D'), 30, Size(640,480));
```

A apostila avisa (p. 7) que, com o Raspberry no wifi, capturamos **bem menos
que 30 fps** — e gravar com 30 faz o vídeo sair acelerado. A versão final mede
o fps real nos primeiros 30 quadros e só então abre o `VideoWriter` com o valor
medido.

Detalhe que você acertou e vale registrar: `Size(640,480)` é `(largura,altura)`,
enquanto `Mat(rows,cols)` é `(altura,largura)`. Inverter isso gera um vídeo nulo
sem nenhuma mensagem de erro — a apostila alerta sobre isso na p. 7.

Seu `resize()` defensivo virou desnecessário porque a versão final usa
`Size(a.cols, a.rows)`, ou seja, o tamanho real do quadro que a câmera entregou.

---

## 5. Por que o `pwmroda4.cpp` final ficou diferente

Quase nada: o seu está correto, inclusive o `softPwmStop` no fim (que é a
correção de 23/10/2024 da apostila, para os motores não ficarem energizados).

A única diferença é que a apostila pede a sequência repetida **duas vezes**
(`for (int i=0; i<2; i++)`) — o seu executa uma vez só.

---

## 6. Onde está o resto

| Arquivo | O que é |
|---|---|
| `../PINOUT.md` | GPIO → ponte-H, conferido com `gpio readall` no Pi 3B real |
| `../README.md` | montagem, compilação e execução |
| `../aula1/README.md` | lições da aula 1 (`ssh -X` e `sshfs`) |
| `../aula2/aula2.sh` | script que roda tudo (`status`, `build`, `blink`, `pwm`, `parar`, `cam`, `vid`, `server`, `client`) |

**Armadilha de compilação:** a apostila (p. 19) manda `compila server6b` sem
`-ocv`. Isso valia para a versão mínima do `raspberry.hpp` da p. 13. O arquivo
oficial publicado pelo professor já inclui `<opencv2/opencv.hpp>`, então
**tudo que inclui `projeto.hpp` precisa de `-ocv`**.
