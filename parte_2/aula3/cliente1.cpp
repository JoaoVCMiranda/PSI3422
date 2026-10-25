//cliente1.cpp - computador - Tarefa 5 da aula 3 (fase 1: sem motores)
//Recebe os quadros do servidor1, gruda o teclado virtual a esquerda,
//mostra na janela e (opcionalmente) grava em video.
//Envia ao servidor 1 byte por quadro: 's' (ESC), '0' (nenhum botao) ou
//'1'..'9' (botao virtual apertado com o mouse).
//computador$ compila cliente1 -ocv -v3
//computador$ cliente1 192.168.0.110 [videosaida.avi] [t/c]
//  t = grava a tela inteira, com o teclado (default)
//  c = grava so' a imagem da camera
#include "teclado.hpp"

int main(int argc, char *argv[]) {
  ARGCLIENTE arg(argc,argv,"cliente1");
  CLIENT client(arg.ip);

  TECLADO teclado(240);
  namedWindow("cliente1");
  teclado.instala("cliente1");
  GRAVADOR* gravador = arg.video.empty() ? NULL : new GRAVADOR(arg.video);

  BYTE cmd='0';
  client.sendBytes(1,&cmd);          //"estou pronto para receber imagem"
  Mat_<COR> img, tela;
  while (true) {
    client.receiveImgComp(img);
    if (img.rows!=teclado.altura()) resize(img,img,Size(320,240));
    hconcat(teclado.desenha(), img, tela);
    imshow("cliente1",tela);
    if (gravador) gravador->grava(arg.modo=='c' ? img : tela);

    int ch=(signed char)(waitKey(1));
    cmd = (ch==27) ? 's' : teclado.codigo();
    client.sendBytes(1,&cmd);
    if (cmd=='s') break;
  }
  delete gravador;                   //fecha o video
}
