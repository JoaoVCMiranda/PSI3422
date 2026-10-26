//teclado.hpp - Tarefas 5 e 6 da aula 3
//Teclado numerico virtual (apertado com o mouse) e gravador de video,
//usados por cliente1.cpp e cliente2.cpp.
//
//Botoes, na disposicao do teclado numerico:
//   7 esquerda        8 frente   9 direita
//   4 esq. acentuada  5 nada     6 dir. acentuada
//   1 re a esquerda   2 re       3 re a direita
//O botao fica "apertado" enquanto o botao esquerdo do mouse estiver
//pressionado sobre ele; ao soltar, volta a 0 (nenhum botao).
//
//Teclado fisico (com a janela em foco), tambem "segure para andar":
//   numeros 1..9 (linha de cima ou teclado numerico, com ou sem NumLock)
//   setas: cima=8 baixo=2 esquerda=4 direita=6
//   q w e / a s d / z x c = 7 8 9 / 4 5 6 / 1 2 3 (para notebook)
//   espaco = 5 (para)        ESC = sai
#ifndef TECLADO_HPP
#define TECLADO_HPP
#include "../projeto.hpp"

class TECLADO {
  int lado;               //lado de cada botao, em pixels
  int botao=0;            //0 = nenhum, 1..9 = botao apertado com o mouse

  //Teclado fisico. O HighGUI so' avisa quando a tecla e' APERTADA, nunca
  //quando e' solta. Segurando a tecla, o sistema repete o evento (~30x/s,
  //depois de ~0,5 s); o comando vale ate' um prazo apos o ultimo evento.
  //Na 1a vez o prazo cobre o atraso inicial da repeticao; depois, curto,
  //para o carrinho parar logo que a tecla e' solta.
  int botaoTecla=0;
  double expira=0;
  static constexpr double PRAZO_INICIAL=0.7, PRAZO_REPETINDO=0.3;

  //codigo do waitKeyEx (keysym do X11, em GTK e Qt) -> botao 1..9, ou 0
  static int teclaParaBotao(int k) {
    k &= 0xffff;                         //o GTK poe os modificadores em cima
    if (k>='1' && k<='9') return k-'0';  //linha de cima e numerico c/ NumLock
    switch (k) {
      case 0xff52: case 0xff97: return 8;   //seta cima,     KP_Up
      case 0xff54: case 0xff99: return 2;   //seta baixo,    KP_Down
      case 0xff51: case 0xff96: return 4;   //seta esquerda, KP_Left
      case 0xff53: case 0xff98: return 6;   //seta direita,  KP_Right
      case 0xff95: return 7;  case 0xff9a: return 9;   //KP_Home, KP_PgUp
      case 0xff9c: return 1;  case 0xff9b: return 3;   //KP_End,  KP_PgDn
      case 0xff9d: case ' ': return 5;                 //KP_Begin, espaco
      case 'q': return 7; case 'w': return 8; case 'e': return 9;
      case 'a': return 4; case 's': return 5; case 'd': return 6;
      case 'z': return 1; case 'x': return 2; case 'c': return 3;
      case 'Q': return 7; case 'W': return 8; case 'E': return 9;
      case 'A': return 4; case 'S': return 5; case 'D': return 6;
      case 'Z': return 1; case 'X': return 2; case 'C': return 3;
    }
    return 0;
  }

  //botao sob o pixel (l,c), ou 0 se estiver fora do teclado
  int qualBotao(int l, int c) const {
    if (l<0 || c<0 || l>=3*lado || c>=3*lado) return 0;
    int lin=l/lado, col=c/lado;
    return (2-lin)*3 + col + 1;
  }

  static void callback(int event, int x, int y, int flags, void* userdata) {
    TECLADO* t=(TECLADO*)userdata;
    if (event==EVENT_LBUTTONDOWN) t->botao=t->qualBotao(y,x);
    else if (event==EVENT_MOUSEMOVE && (flags & EVENT_FLAG_LBUTTON))
      t->botao=t->qualBotao(y,x);     //arrastar de um botao para outro
    else if (event==EVENT_LBUTTONUP) t->botao=0;
  }

public:
  //altura = altura da imagem da camera (240), para gruda-los lado a lado
  TECLADO(int altura=240): lado(altura/3) {}

  int largura() const { return 3*lado; }
  int altura() const { return 3*lado; }

  //botao valendo agora: o mouse tem prioridade sobre o teclado fisico
  int atual() const {
    if (botao) return botao;
    return (timeSinceEpoch()<expira) ? botaoTecla : 0;
  }

  //'0' se nenhum botao, '1'..'9' caso contrario
  BYTE codigo() const { return BYTE('0'+atual()); }

  //Le TODAS as teclas pendentes (substitui o waitKey do laco principal).
  //Esvaziar a fila importa: a repeticao gera ~30 eventos/s e o laco roda a
  //~15 fps; lendo um por quadro, eles se acumulariam e o carrinho
  //continuaria andando depois de a tecla ser solta.
  //Devolve true se apertou ESC.
  bool leTeclas() {
    bool esc=false;
    for (int i=0; i<100; i++) {
      int k=waitKeyEx(1);
      if (k<0) break;
      if ((k & 0xffff)==27) { esc=true; continue; }
      int b=teclaParaBotao(k);
      if (b==0) continue;
      double t=timeSinceEpoch();
      bool repetindo = (b==botaoTecla && t<expira);
      botaoTecla=b;
      expira=t + (repetindo ? PRAZO_REPETINDO : PRAZO_INICIAL);
    }
    return esc;
  }

  //o teclado fica no canto esquerdo da janela, a partir de (0,0)
  void instala(const string& janela) { setMouseCallback(janela, callback, this); }

  Mat_<COR> desenha() const {
    Mat_<COR> a(altura(), largura(), COR(64,64,64));
    for (int k=1; k<=9; k++) {
      int col=(k-1)%3, lin=2-(k-1)/3;
      int li=lin*lado, ci=col*lado;
      COR fundo = (k==atual()) ? COR(0,160,255) : COR(200,200,200);
      rectangle(a, Rect(ci+2,li+2,lado-4,lado-4), Scalar(fundo[0],fundo[1],fundo[2]), FILLED);

      //a direcao da flecha sai da propria posicao do botao
      int lc=li+lado/2, cc=ci+lado/2, L=lado/4;
      int dl=lin-1, dc=col-1;
      if (k==5) circle(a, Point(cc,lc), L/2, Scalar(0,0,0), 3);
      else flecha(a, lc-dl*L, cc-dc*L, lc+dl*L, cc+dc*L, COR(0,0,0), 3);

      putText(a, to_string(k), Point(ci+6,li+18), FONT_HERSHEY_SIMPLEX,
              0.5, Scalar(0,0,0), 1);
    }
    return a;
  }
};

//---------------------------------------------------------------------------
// GRAVADOR: grava quadros num video com o fps REAL do sistema.
// O fps depende da rede, entao nao da' para fixar 30 (o video sairia
// acelerado). Guarda os primeiros N quadros, mede o fps com eles e so'
// entao abre o VideoWriter.
//---------------------------------------------------------------------------
class GRAVADOR {
  static const unsigned N=20;
  string nome;
  VideoWriter vo;
  vector<Mat_<COR>> buf;
  double t0=0;

  void abre() {
    double dt=timeSinceEpoch()-t0;
    double fps = (buf.size()>1 && dt>0) ? (buf.size()-1)/dt : 10.0;
    fps=max(1.0, min(fps, 60.0));
    //Size(largura,altura): invertido em relacao a Mat(linhas,colunas)
    vo.open(nome, VideoWriter::fourcc('X','V','I','D'), fps,
            Size(buf[0].cols, buf[0].rows));
    if (!vo.isOpened()) erro("Erro: nao consegui abrir o video de saida "+nome);
    printf("Gravando %s a %.1f fps (medido)\n", nome.c_str(), fps);
    for (auto& q: buf) vo << q;
    buf.clear();
  }

public:
  GRAVADOR(const string& n): nome(n) {}

  void grava(const Mat_<COR>& q) {
    if (vo.isOpened()) { vo << q; return; }
    if (buf.empty()) t0=timeSinceEpoch();
    buf.push_back(q.clone());
    if (buf.size()==N) abre();
  }

  ~GRAVADOR() {
    if (!vo.isOpened() && !buf.empty()) abre();  //programa curto: < N quadros
    vo.release();
  }
};

//---------------------------------------------------------------------------
// Argumentos de cliente1/cliente2:  ip [videosaida.avi] [t/c]
//---------------------------------------------------------------------------
struct ARGCLIENTE {
  string ip, video;     //video vazio = nao grava
  char modo='t';        //'t' = tela inteira (com teclado), 'c' = so' camera

  ARGCLIENTE(int argc, char* argv[], const string& prog) {
    string uso=prog+" servidorIpAddr [videosaida.avi] [t/c]";
    if (argc<2 || argc>4) erro(uso);
    ip=argv[1];
    for (int i=2; i<argc; i++) {
      string a=argv[i];
      if (a=="t" || a=="c") modo=a[0];
      else if (video.empty()) video=a;
      else erro(uso);
    }
  }
};

#endif
