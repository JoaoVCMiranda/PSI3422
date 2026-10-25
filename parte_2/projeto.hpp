//projeto.hpp - PSI3422 2o sem/2026 - aulas 2 e 3
//Classes de comunicacao TCP/IP do projeto.
//Aula 3: confirmacao no sendVb/receiveVb, testaVb, sendImg/receiveImg
//(sem compressao) e sendImgComp/receiveImgComp (JPEG).
//Derivado de server5.c/client5.c (Beej's Guide, adaptado pelo Hae).
//O retry em EINTR nos lacos de send/recv veio do codigo do Caetano
//(ver caetano/LEIA-ME.md).
//
//Compile sempre com -ocv, pois raspberry.hpp inclui OpenCV:
//  raspberry$   compila prog -ocv
//  computador$  compila prog -ocv -v3
#ifndef PROJETO_HPP
#define PROJETO_HPP

//---------------------------------------------------------------------------
// Compatibilidade OpenCV 3 <-> OpenCV 4.
//
// O raspberry.hpp do professor usa CV_BGR2GRAY e CV_GRAY2BGR, que existiam no
// OpenCV 2/3 e o OpenCV 4 removeu (viraram cv::COLOR_*). Sem isto, compilar
// contra um OpenCV 4 da' "CV_BGR2GRAY was not declared in this scope".
//
// Fica AQUI, e nao nas flags de compilacao, para valer tambem quando alguem
// chama o g++ na mao. Precisa vir ANTES do include do raspberry.hpp.
// No Raspberry e no Cekeikon (OpenCV 3) este bloco nao faz nada.
//
// Obs.: CV_32F e CV_8U, tambem usados pelo raspberry.hpp, continuam existindo
// no OpenCV 4 - nao precisam de tratamento.
//---------------------------------------------------------------------------
#include <opencv2/core/version.hpp>
#if CV_MAJOR_VERSION >= 4
  #define CV_BGR2GRAY cv::COLOR_BGR2GRAY
  #define CV_GRAY2BGR cv::COLOR_GRAY2BGR
  //o projeto usa cv::IMWRITE_JPEG_QUALITY (existe no 3 e no 4); isto e'
  //so' para quem copiar codigo antigo que use o nome em C
  #define CV_IMWRITE_JPEG_QUALITY cv::IMWRITE_JPEG_QUALITY
#endif

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
// testaVb: testa se todos os bytes de vb possuem valor b (aula 3)
//---------------------------------------------------------------------------
bool testaVb(const vector<BYTE> vb, BYTE b) {
  bool igual=true;
  for (unsigned i=0; i<vb.size(); i++)
    if (vb[i]!=b) { igual=false; break; }
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
  // MSG_NOSIGNAL: se o outro lado caiu, send() devolve EPIPE em vez de matar
  // o processo com SIGPIPE. Assim erro() roda e os handlers de atexit (ex.:
  // desligar os motores no servidor2) tambem.
  void sendBytes(int nBytesToSend, const BYTE *buf) {
    int enviados=0;
    while (enviados<nBytesToSend) {
      ssize_t n=send(fd(), buf+enviados, nBytesToSend-enviados, MSG_NOSIGNAL);
      if (n==-1) {
        if (errno==EINTR) continue;  //um sinal interrompeu: tenta de novo
        erro("Erro: send: "+string(strerror(errno)));
      }
      enviados += int(n);
    }
  }

  // Recebe exatamente nBytesToReceive bytes.
  // recv() pode devolver MENOS bytes do que o pedido - por isso o laco.
  void receiveBytes(int nBytesToReceive, BYTE *buf) {
    int recebidos=0;
    while (recebidos<nBytesToReceive) {
      ssize_t n=recv(fd(), buf+recebidos, nBytesToReceive-recebidos, 0);
      if (n==-1) {
        if (errno==EINTR) continue;  //um sinal interrompeu: tenta de novo
        erro("Erro: recv: "+string(strerror(errno)));
      }
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

  // vector<BYTE> = cabecalho (tamanho) + dados, enviados em tres passos:
  //  1) sender envia o tamanho t; receiver recebe t e aloca t bytes
  //  2) receiver confirma, devolvendo t; sender confere a confirmacao
  //  3) sender envia os t bytes; receiver os recebe em vb.data()
  void sendVb(const vector<BYTE>& vb) {
    uint32_t t=uint32_t(vb.size());
    sendUint(t);
    uint32_t conf;
    receiveUint(conf);
    if (conf!=t) erro("Erro: sendVb: confirmacao "+to_string(conf)+
                      " diferente do tamanho "+to_string(t));
    sendBytes(int(t), vb.data());
  }

  void receiveVb(vector<BYTE>& vb) {
    uint32_t t;
    receiveUint(t);
    vb.resize(t);
    sendUint(t);                 //confirmacao: "recebi t, pode mandar"
    receiveBytes(int(t), vb.data());
  }

  // Imagem colorida SEM compressao: linhas, colunas e depois os pixels.
  // Mat_ vindo da camera e' continuo; clone() garante isso para um ROI.
  void sendImg(const Mat_<COR>& img) {
    Mat_<COR> c = img.isContinuous() ? img : Mat_<COR>(img.clone());
    sendUint(uint32_t(c.rows));
    sendUint(uint32_t(c.cols));
    sendBytes(int(c.total()*c.elemSize()), c.data);
  }

  void receiveImg(Mat_<COR>& img) {
    uint32_t l, c;
    receiveUint(l);
    receiveUint(c);
    img.create(int(l), int(c));  //so' realoca se o tamanho mudou
    receiveBytes(int(img.total()*img.elemSize()), img.data);
  }

  // Imagem colorida COM compressao JPEG. imencode grava "o arquivo .jpg"
  // num vector<BYTE> em vez do disco; imdecode faz o caminho inverso.
  void sendImgComp(const Mat_<COR>& img, int qualidade=80) {
    vector<BYTE> vb;
    vector<int> param{IMWRITE_JPEG_QUALITY, qualidade};
    if (!imencode(".jpg", img, vb, param)) erro("Erro: imencode");
    sendVb(vb);
  }

  void receiveImgComp(Mat_<COR>& img) {
    vector<BYTE> vb;
    receiveVb(vb);
    img = imdecode(vb, IMREAD_COLOR);
    if (img.empty()) erro("Erro: imdecode: JPEG invalido");
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
    if (new_fd!=-1) { close(new_fd); printf("server: fechei conexao.\n"); }
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
