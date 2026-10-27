//servidor4.cpp - Raspberry - Licao de casa #3/3 da aula 4 (fase 4)
//Manda os quadros 240x320 da camera (JPEG) ao cliente4, que localiza a
//placa, e aplica nas rodas a velocidade que ele devolve. Toda a parte pesada
//(template matching) fica no computador.
//
//Protocolo trava-trava, como na aula 3: o servidor so' captura e manda um
//quadro depois de receber um comando de 3 bytes do cliente:
//  'm' esq dir   move: esq e dir em -100..100 (int8, % da potencia calibrada)
//  's'  -   -    termina (desliga os motores)
//raspberry$ compila servidor4 -ocv -w
//raspberry$ servidor4 [frenteEsq frenteDir reEsq reDir]
#include "../motores.hpp"

int main(int argc, char *argv[]) {
  iniciaMotores(argc, argv, "servidor4");

  VideoCapture w(0);
  if (!w.isOpened()) erro("Erro: Abertura de webcam 0.");
  w.set(CAP_PROP_FRAME_WIDTH,320);
  w.set(CAP_PROP_FRAME_HEIGHT,240);

  SERVER server;
  server.waitConnection();

  Mat_<COR> a;
  BYTE cmd[3];
  server.receiveBytes(3,cmd);        //'m' 0 0 inicial: cliente pronto
  while (cmd[0]!='s') {
    if (cmd[0]!='m') erro("Erro: comando desconhecido "+to_string(cmd[0]));
    double esq=int8_t(cmd[1])/100.0, dir=int8_t(cmd[2])/100.0;
    motores(max(-1.0,min(esq,1.0)), max(-1.0,min(dir,1.0)));
    w >> a;
    if (a.empty()) erro("Erro: quadro vazio da camera");
    if (a.rows!=240 || a.cols!=320) resize(a,a,Size(320,240));
    server.sendImgComp(a);
    server.receiveBytes(3,cmd);
  }
  motores(0,0);
}
