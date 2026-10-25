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
#ifndef TECLADO_HPP
#define TECLADO_HPP
#include "../projeto.hpp"

class TECLADO {
  int lado;               //lado de cada botao, em pixels
  int botao=0;            //0 = nenhum, 1..9 = botao apertado

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

  //'0' se nenhum botao, '1'..'9' caso contrario
  BYTE codigo() const { return BYTE('0'+botao); }

  //o teclado fica no canto esquerdo da janela, a partir de (0,0)
  void instala(const string& janela) { setMouseCallback(janela, callback, this); }

  Mat_<COR> desenha() const {
    Mat_<COR> a(altura(), largura(), COR(64,64,64));
    for (int k=1; k<=9; k++) {
      int col=(k-1)%3, lin=2-(k-1)/3;
      int li=lin*lado, ci=col*lado;
      COR fundo = (k==botao) ? COR(0,160,255) : COR(200,200,200);
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
