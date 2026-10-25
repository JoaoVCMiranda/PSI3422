//servidor1.cpp - Raspberry - Tarefa 5 da aula 3 (fase 1: sem motores)
//Protocolo (1 byte do cliente por quadro):
//  's'       termina
//  '0'       nenhum botao virtual apertado
//  '1'..'9'  botao virtual apertado
//A cada mensagem que nao seja 's', captura um quadro 240x320, escreve nele
//a mensagem recebida (prova de que o comando chegou), compacta e envia.
//raspberry$ compila servidor1 -ocv
//raspberry$ servidor1
#include "../../projeto.hpp"

int main(void) {
  VideoCapture w(0);
  if (!w.isOpened()) erro("Erro: Abertura de webcam 0.");
  //resolucao menor => menos bytes por quadro => menos latencia
  w.set(CAP_PROP_FRAME_WIDTH,320);
  w.set(CAP_PROP_FRAME_HEIGHT,240);

  SERVER server;
  server.waitConnection();

  Mat_<COR> a;
  BYTE cmd;
  server.receiveBytes(1,&cmd);       //"0" inicial: cliente pronto
  while (cmd!='s') {
    w >> a;
    if (a.empty()) erro("Erro: quadro vazio da camera");
    if (a.rows!=240 || a.cols!=320) resize(a,a,Size(320,240));
    putText(a, string(1,char(cmd)), Point(10,60), FONT_HERSHEY_SIMPLEX,
            2.0, Scalar(0,0,255), 4);
    server.sendImgComp(a);
    server.receiveBytes(1,&cmd);
  }
}
