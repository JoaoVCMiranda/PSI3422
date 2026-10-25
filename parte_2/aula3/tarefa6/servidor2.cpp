//servidor2.cpp - Raspberry - Tarefa 6 da aula 3 (fase 2: controla os motores)
//Mesmo protocolo do servidor1, mas agora o comando recebido MOVE os motores
//e NAO e' escrito na imagem (o video servira' para treinar uma CNN).
//  's'       termina (desliga os motores)
//  '0' / '5' para os motores
//  '1'..'9'  movimento do teclado virtual (ver tabela em comando())
//raspberry$ compila servidor2 -ocv -w
//raspberry$ servidor2 [frenteEsq frenteDir reEsq reDir]
//
//CALIBRACAO: os 4 argumentos opcionais sao as potencias (0..100) de cada
//roda indo para frente e para tras. Ajuste ate' o carrinho andar em linha
//reta com "8" e com "2", e depois copie os valores para as constantes
//abaixo. Ex.: se puxa para a direita indo para frente, a roda esquerda
//esta' mais forte: diminua frenteEsq (ou aumente frenteDir).
#include "../../projeto.hpp"
#include <wiringPi.h>
#include <softPwm.h>
#include <csignal>

//---------------------------------------------------------------------------
// Mapeamento GPIO (wiringPi) -> rodas. Ver ../PINOUT.md: motor A = pinos
// 0,1 e motor B = pinos 2,3. Qual pino de cada par e' "frente" depende de
// como os fios do motor foram ligados: se "8" fizer uma roda girar para
// tras, troque os dois pinos daquela roda aqui.
//---------------------------------------------------------------------------
const int ESQ_FRENTE=0, ESQ_TRAS=1;   //motor A = roda esquerda
const int DIR_FRENTE=2, DIR_TRAS=3;   //motor B = roda direita

//Potencias calibradas (%). Valores iniciais - calibre no carrinho real.
int frenteEsq=80, frenteDir=80;
int reEsq=80, reDir=80;

//Fracao da potencia da roda interna nas curvas suaves (7, 9, 1, 3)
const double CURVA=0.35;
//Potencia de giro no proprio eixo (4 e 6), uma roda para cada lado
const double GIRO=0.7;

//v em -1..+1: positivo = frente, negativo = tras, 0 = parada
void roda(double v, int pinoFrente, int pinoTras, int potFrente, int potTras) {
  if (v>0)      { softPwmWrite(pinoFrente, int(v*potFrente+0.5)); softPwmWrite(pinoTras, 0); }
  else if (v<0) { softPwmWrite(pinoFrente, 0); softPwmWrite(pinoTras, int(-v*potTras+0.5)); }
  else          { softPwmWrite(pinoFrente, 0); softPwmWrite(pinoTras, 0); }
}

void motores(double esq, double dir) {
  roda(esq, ESQ_FRENTE, ESQ_TRAS, frenteEsq, reEsq);
  roda(dir, DIR_FRENTE, DIR_TRAS, frenteDir, reDir);
}

void comando(BYTE cmd) {
  switch (cmd) {
    case '7': motores(CURVA, 1.0);     break; //esquerda
    case '8': motores(1.0, 1.0);       break; //frente
    case '9': motores(1.0, CURVA);     break; //direita
    case '4': motores(-GIRO, GIRO);    break; //esquerda acentuada (gira no eixo)
    case '6': motores(GIRO, -GIRO);    break; //direita acentuada (gira no eixo)
    case '1': motores(-CURVA, -1.0);   break; //re a esquerda
    case '2': motores(-1.0, -1.0);     break; //re
    case '3': motores(-1.0, -CURVA);   break; //re a direita
    default:  motores(0, 0);           break; //'0', '5' ou desconhecido: para
  }
}

//---------------------------------------------------------------------------
// Os motores NUNCA podem ficar ligados quando o programa termina - seja
// pelo 's', por queda da conexao (erro() -> exit) ou por Ctrl+C.
//---------------------------------------------------------------------------
void desligaMotores() {
  softPwmStop(ESQ_FRENTE); softPwmStop(ESQ_TRAS);
  softPwmStop(DIR_FRENTE); softPwmStop(DIR_TRAS);
}

void trataSinal(int) {
  motores(0,0);   //zera ja'; o softPwmStop roda no atexit
  exit(1);
}

int main(int argc, char *argv[]) {
  if (argc==5) {
    frenteEsq=atoi(argv[1]); frenteDir=atoi(argv[2]);
    reEsq=atoi(argv[3]);     reDir=atoi(argv[4]);
  } else if (argc!=1) erro("servidor2 [frenteEsq frenteDir reEsq reDir]");
  printf("Potencias: frente %d/%d  re %d/%d (esq/dir)\n",frenteEsq,frenteDir,reEsq,reDir);

  wiringPiSetup();
  if (softPwmCreate(ESQ_FRENTE,0,100)) erro("Erro: softPwmCreate");
  if (softPwmCreate(ESQ_TRAS,0,100))   erro("Erro: softPwmCreate");
  if (softPwmCreate(DIR_FRENTE,0,100)) erro("Erro: softPwmCreate");
  if (softPwmCreate(DIR_TRAS,0,100))   erro("Erro: softPwmCreate");
  atexit(desligaMotores);
  signal(SIGINT, trataSinal);
  signal(SIGTERM, trataSinal);

  VideoCapture w(0);
  if (!w.isOpened()) erro("Erro: Abertura de webcam 0.");
  w.set(CAP_PROP_FRAME_WIDTH,320);
  w.set(CAP_PROP_FRAME_HEIGHT,240);

  SERVER server;
  server.waitConnection();

  Mat_<COR> a;
  BYTE cmd;
  server.receiveBytes(1,&cmd);       //"0" inicial: cliente pronto
  while (cmd!='s') {
    comando(cmd);
    w >> a;
    if (a.empty()) erro("Erro: quadro vazio da camera");
    if (a.rows!=240 || a.cols!=320) resize(a,a,Size(320,240));
    server.sendImgComp(a);           //sem o comando escrito na imagem
    server.receiveBytes(1,&cmd);
  }
  motores(0,0);
}
