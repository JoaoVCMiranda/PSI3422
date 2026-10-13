//projeto.hpp - PSI3422 2o sem/2026 - aula 2
//Classes de comunicacao TCP/IP do projeto.
//Derivado de server5.c/client5.c (Beej's Guide, adaptado pelo Hae).
//
//Compile sempre com -ocv, pois raspberry.hpp inclui OpenCV:
//  raspberry$   compila prog -ocv
//  computador$  compila prog -ocv -v3
#ifndef PROJETO_HPP
#define PROJETO_HPP

#include "raspberry.hpp"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <vector>

//---------------------------------------------------------------------------
// testaBytes: testa se os n bytes apontados por buf possuem o valor b
//---------------------------------------------------------------------------
bool testaBytes(BYTE* buf, BYTE b, int n) {
  bool igual=true;
  for (int i=0; i<n; i++)
    if (buf[i]!=b) { igual=false; break; }
  return igual;
}

//---------------------------------------------------------------------------
// DEVICE: classe abstrata com tudo o que SERVER e CLIENT tem em comum.
//
// sendBytes/receiveBytes aparecem uma unica vez (e nao duplicados nas duas
// classes derivadas, como na figura 11b da apostila): o que muda entre
// SERVER e CLIENT e' apenas QUAL socket usar, exposto pelo metodo virtual
// fd(). Isso responde a "Nota para mim" da apostila (p. 30).
//---------------------------------------------------------------------------
class DEVICE {
protected:
  static const int PORTA = 3490;

  // socket por onde trafegam os dados: new_fd no SERVER, sockfd no CLIENT
  virtual int fd() const = 0;

  static void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET)
      return &(((struct sockaddr_in*)sa)->sin_addr);
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
  }

public:
  virtual ~DEVICE() {}

  // Envia exatamente nBytesToSend bytes.
  // send() pode enviar MENOS bytes do que o pedido - por isso o laco.
  void sendBytes(int nBytesToSend, const BYTE *buf) {
    int enviados=0;
    while (enviados<nBytesToSend) {
      ssize_t n=send(fd(), buf+enviados, nBytesToSend-enviados, 0);
      if (n==-1) erro("Erro: send: "+string(strerror(errno)));
      enviados += int(n);
    }
  }

  // Recebe exatamente nBytesToReceive bytes.
  // recv() pode devolver MENOS bytes do que o pedido - por isso o laco.
  void receiveBytes(int nBytesToReceive, BYTE *buf) {
    int recebidos=0;
    while (recebidos<nBytesToReceive) {
      ssize_t n=recv(fd(), buf+recebidos, nBytesToReceive-recebidos, 0);
      if (n==-1) erro("Erro: recv: "+string(strerror(errno)));
      if (n==0)  erro("Erro: conexao fechada pelo outro lado");
      recebidos += int(n);
    }
  }

  // Inteiro de 4 bytes. A convencao da internet e' big endian; Raspberry e
  // Intel sao little endian, dai htonl/ntohl.
  // m e' local (nao static): sendBytes so' retorna depois de enviar tudo,
  // entao a variavel continua viva durante todo o envio.
  void sendUint(uint32_t m) {
    uint32_t rede=htonl(m);
    sendBytes(4, (const BYTE*)&rede);
  }

  void receiveUint(uint32_t& m) {
    uint32_t rede;
    receiveBytes(4, (BYTE*)&rede);
    m=ntohl(rede);
  }

  // vector<BYTE> = cabecalho (tamanho) + dados, enviados em dois passos.
  void sendVb(const vector<BYTE>& vb) {
    sendUint(uint32_t(vb.size()));
    sendBytes(int(vb.size()), vb.data());
  }

  void receiveVb(vector<BYTE>& vb) {
    uint32_t t;
    receiveUint(t);
    vb.resize(t);
    receiveBytes(int(t), vb.data());
  }
};

//---------------------------------------------------------------------------
// SERVER - roda no Raspberry
//---------------------------------------------------------------------------
class SERVER: public DEVICE {
  static const int BACKLOG = 1;   // atende um unico cliente
  int sockfd;                     // escuta conexoes
  int new_fd;                     // conexao estabelecida

protected:
  int fd() const override { return new_fd; }

public:
  SERVER(): sockfd(-1), new_fd(-1) {
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;   // usa o IP desta maquina

    string porta=to_string(PORTA);
    int rv=getaddrinfo(NULL, porta.c_str(), &hints, &servinfo);
    if (rv!=0) erro("Erro: getaddrinfo: "+string(gai_strerror(rv)));

    int yes=1;
    for (p=servinfo; p!=NULL; p=p->ai_next) {
      sockfd=socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (sockfd==-1) continue;
      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1)
        erro("Erro: setsockopt: "+string(strerror(errno)));
      if (bind(sockfd, p->ai_addr, p->ai_addrlen)==-1) { close(sockfd); sockfd=-1; continue; }
      break;
    }
    freeaddrinfo(servinfo);
    if (p==NULL) erro("Erro: server: nao consegui fazer bind na porta "+porta);

    if (listen(sockfd, BACKLOG)==-1)
      erro("Erro: listen: "+string(strerror(errno)));
  }

  ~SERVER() {
    if (new_fd!=-1) close(new_fd);
    if (sockfd!=-1) close(sockfd);
  }

  void waitConnection() {
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];

    printf("server: Esperando conexao...\n");
    while (1) {
      sin_size=sizeof their_addr;
      new_fd=accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
      if (new_fd==-1) { perror("accept"); continue; }
      break;
    }
    inet_ntop(their_addr.ss_family,
              get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
    printf("server: recebi conexao de %s\n", s);

    close(sockfd); sockfd=-1;   // nao aceita mais nenhum outro cliente
  }
};

//---------------------------------------------------------------------------
// CLIENT - roda no computador
//---------------------------------------------------------------------------
class CLIENT: public DEVICE {
  int sockfd;

protected:
  int fd() const override { return sockfd; }

public:
  CLIENT(string endereco): sockfd(-1) {
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    string porta=to_string(PORTA);
    int rv=getaddrinfo(endereco.c_str(), porta.c_str(), &hints, &servinfo);
    if (rv!=0) erro("Erro: getaddrinfo: "+string(gai_strerror(rv)));

    char s[INET6_ADDRSTRLEN];
    for (p=servinfo; p!=NULL; p=p->ai_next) {
      sockfd=socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (sockfd==-1) continue;
      if (connect(sockfd, p->ai_addr, p->ai_addrlen)==-1) { close(sockfd); sockfd=-1; continue; }
      break;
    }
    if (p==NULL) erro("Erro: client: nao consegui conectar em "+endereco);

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: conectando a %s\n", s);
    freeaddrinfo(servinfo);
  }

  ~CLIENT() {
    if (sockfd!=-1) close(sockfd);
  }
};

#endif
