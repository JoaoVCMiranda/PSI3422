//cliente4.cpp - computador - Licao de casa #3/3 da aula 4 (fase 4)
//Recebe os quadros do servidor4, localiza a placa (mesmo detector paralelo
//do fase3p, em ../placa.hpp) e devolve a velocidade das rodas para o
//carrinho seguir a placa. So' a posicao x da placa e' usada.
//
//Controle (como o do professor): a DIFERENCA entre as rodas e' proporcional
//ao desvio da placa em relacao ao centro da imagem:
//   erro = (x - 160)/160            -1 (toda a esquerda) .. +1 (toda a direita)
//   esq  = VEL + GANHO*erro          placa a direita -> roda esquerda mais
//   dir  = VEL - GANHO*erro          rapida -> carrinho vira a direita
//O carrinho PARA quando (a) nao acha a placa ou (b) a placa esta' perto
//demais (lado aparente >= LADO_PERTO pixels, ~30 cm).
//
//computador$ compila cliente4 -ocv -v3 -omp
//computador$ cliente4 192.168.0.110 [videosaida.avi] [t/c]
//  t = grava a tela (com os quadrados e numeros de debug, default)
//  c = grava so' a imagem da camera (video pedido para treinar a CNN)
//ESC (com a janela em foco) termina os dois programas.
#include "../placa.hpp"
#include "../../aula3/teclado.hpp"   //GRAVADOR e ARGCLIENTE

const double VEL=0.70;     //velocidade para frente com a placa centrada
const double GANHO=0.50;   //quanto o desvio da placa muda cada roda
const int LADO_PERTO=60;   //placa com lado >= isto (escalas 0 e 1): para

//procura quadrado.png rodando de aula4/ ou de aula4/tarefa3/
string achaModelo() {
  for (string s: {"quadrado.png", "../quadrado.png"})
    if (!imread(s,1).empty()) return s;
  erro("Erro: nao achei quadrado.png (coloque-o em parte_2/aula4/)");
  return "";
}

//barra vertical da velocidade de uma roda, a partir do centro
void barra(Mat_<COR>& a, int c, double v, const Scalar& cor) {
  int meio=a.rows/2, h=int(v*(a.rows/2-10));
  rectangle(a, Point(c,meio), Point(c+8,meio-h), cor, FILLED);
}

int main(int argc, char *argv[]) {
  ARGCLIENTE arg(argc,argv,"cliente4");
  PLACA placa(achaModelo(), true);
  printf("cliente4: localizando com %d thread(s)\n", placa.threads());
  CLIENT client(arg.ip);

  namedWindow("cliente4");
  GRAVADOR* gravador = arg.video.empty() ? NULL : new GRAVADOR(arg.video);

  BYTE cmd[3]={'m',0,0};
  client.sendBytes(3,cmd);           //"estou pronto para receber imagem"
  Mat_<COR> img, tela;
  Mat_<FLT> f;
  int quadros=0; double t0=timeSinceEpoch();
  while (true) {
    client.receiveImgComp(img);
    if (img.rows!=240 || img.cols!=320) resize(img,img,Size(320,240));
    converte(img,f);
    placa.procura(f);

    double esq=0, dir=0;
    string estado;
    if (!placa.achou) estado="sem placa: parado";
    else if (placa.melhor.lado>=LADO_PERTO) estado="perto: parado";
    else {
      double e=(placa.melhor.c-img.cols/2.0)/(img.cols/2.0);
      esq=max(-1.0,min(VEL+GANHO*e,1.0));
      dir=max(-1.0,min(VEL-GANHO*e,1.0));
      estado="seguindo";
    }

    tela=img.clone();
    placa.desenha(tela);
    barra(tela, 2, esq, Scalar(0,255,0));
    barra(tela, tela.cols-10, dir, Scalar(0,255,0));
    putText(tela, estado, Point(14,tela.rows-6), FONT_HERSHEY_PLAIN, 1.0, Scalar(0,0,255), 1);
    imshow("cliente4",tela);
    if (gravador) gravador->grava(arg.modo=='c' ? img : tela);

    bool esc=false;
    for (int i=0; i<100; i++) {      //esvazia a fila de teclas
      int k=waitKeyEx(1);
      if (k<0) break;
      if ((k & 0xffff)==27) esc=true;
    }
    if (esc) { cmd[0]='s'; cmd[1]=cmd[2]=0; }
    else { cmd[0]='m'; cmd[1]=BYTE(int8_t(round(100*esq))); cmd[2]=BYTE(int8_t(round(100*dir))); }
    client.sendBytes(3,cmd);
    if (esc) break;

    if (++quadros%40==0) {
      double t=timeSinceEpoch();
      printf("%.1f fps\n", 40/(t-t0)); t0=t;
    }
  }
  delete gravador;                   //fecha o video
}
