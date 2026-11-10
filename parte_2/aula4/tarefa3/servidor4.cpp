
#include "../../projeto.hpp"
#include <wiringPi.h>
#include <softPwm.h>
#include <csignal>

//#########################################################################
// RODAS (motores)
//#########################################################################

//---------------------------------------------------------------------------
// Mapeamento: qual pino (numeracao wiringPi) faz o que.
// Conferido roda a roda em 27/10/2026 (rodas no ar, frente = lado da camera):
//   motor A (pinos 0,1) = roda ESQUERDA, frente no pino 1;
//   motor B (pinos 2,3) = roda DIREITA,  frente no pino 2.
//---------------------------------------------------------------------------
const int ESQ_FRENTE=1, ESQ_TRAS=0;
const int DIR_FRENTE=2, DIR_TRAS=3;

//---------------------------------------------------------------------------
// Calibracao: PWM (%) que cada roda recebe quando o comando e' 100%.
// Os motores nunca sao identicos: se o carrinho puxa para um lado, abaixe a
// potencia da roda mais forte. Podem ser trocadas pela linha de comando.
//---------------------------------------------------------------------------
int frenteEsq=80, frenteDir=80;
int reEsq=80, reDir=80;

//---------------------------------------------------------------------------
// motores: velocidade das duas rodas de uma vez (cada uma em -1..+1).
//   motores(1,1) = frente   motores(-1,-1) = re   motores(0,0) = para
//   motores(1,0.3) = curva para a DIREITA (a roda esquerda corre mais)
// Para cada roda (r=0 esquerda, r=1 direita):
//   v em -1..+1: +1 = frente a toda, -1 = re a toda, 0 = parada.
//   O PWM final e' |v| x potencia calibrada daquele sentido, arredondado.
//   Ex.: v=0.5 com potencia de frente 80  ->  PWM 40% no pino da frente.
//---------------------------------------------------------------------------
void motores(double esq, double dir) {
  double v[2]       = { esq, dir };
  int pinoFrente[2] = { ESQ_FRENTE, DIR_FRENTE };
  int pinoTras[2]   = { ESQ_TRAS,   DIR_TRAS   };
  int potFrente[2]  = { frenteEsq,  frenteDir  };
  int potTras[2]    = { reEsq,      reDir      };
  for (int r=0; r<2; r++) {
    if (v[r] > 0) {                              //para frente
      int pwm = int(v[r]*potFrente[r] + 0.5);    //+0.5 = arredonda
      softPwmWrite(pinoFrente[r], pwm);          //[wiringPi]
      softPwmWrite(pinoTras[r], 0);              //[wiringPi]
    } else if (v[r] < 0) {                       //para tras
      int pwm = int(-v[r]*potTras[r] + 0.5);     //-v e' positivo
      softPwmWrite(pinoFrente[r], 0);            //[wiringPi]
      softPwmWrite(pinoTras[r], pwm);            //[wiringPi]
    } else {                                     //parada
      softPwmWrite(pinoFrente[r], 0);            //[wiringPi]
      softPwmWrite(pinoTras[r], 0);              //[wiringPi]
    }
  }
}

//---------------------------------------------------------------------------
// SEGURANCA: os motores NUNCA podem ficar ligados quando o programa
// termina - seja por comando, queda da conexao (erro() -> exit) ou
// Ctrl+C/kill. Dois mecanismos (ligados no main):
//   atexit(desligaMotores)  roda em TODO exit() (inclusive o do erro())
//   signal(..., trataSinal) transforma Ctrl+C (SIGINT) e kill (SIGTERM)
//                           num exit(), que por sua vez chama o atexit
// (As duas tem que ser funcoes: e' o que atexit e signal recebem.)
//---------------------------------------------------------------------------
void desligaMotores() {
  //softPwmStop [wiringPi] para o thread da softPwm e deixa o pino em 0
  softPwmStop(ESQ_FRENTE); softPwmStop(ESQ_TRAS);
  softPwmStop(DIR_FRENTE); softPwmStop(DIR_TRAS);
}

void trataSinalMotores(int) {
  motores(0,0);   //zera ja'; o softPwmStop roda no atexit
  exit(1);
}

//#########################################################################
// PROGRAMA
//#########################################################################

int main(int argc, char *argv[]) {
  //=======================================================================
  // 1) motores: potencias da linha de comando, softPwm e seguranca
  //=======================================================================
  //   servidor4                                   usa as potencias padrao (80)
  //   servidor4 frenteEsq frenteDir reEsq reDir   ex.: servidor4 76 80 80 78
  if (argc==5) {
    frenteEsq=atoi(argv[1]); frenteDir=atoi(argv[2]);   //[C] texto -> int
    reEsq=atoi(argv[3]);     reDir=atoi(argv[4]);
  } else if (argc!=1) {
    erro(string("servidor4")+" [frenteEsq frenteDir reEsq reDir]");    //[raspberry.hpp]
  }
  printf("Potencias: frente %d/%d  re %d/%d (esq/dir)\n",frenteEsq,frenteDir,reEsq,reDir);

  //liga a softPwm: faixa 0..100, comecando em 0 (parado)
  wiringPiSetup();                      //[wiringPi]
  if (softPwmCreate(ESQ_FRENTE,0,100)) erro("Erro: softPwmCreate");   //[wiringPi]
  if (softPwmCreate(ESQ_TRAS,0,100))   erro("Erro: softPwmCreate");
  if (softPwmCreate(DIR_FRENTE,0,100)) erro("Erro: softPwmCreate");
  if (softPwmCreate(DIR_TRAS,0,100))   erro("Erro: softPwmCreate");

  //seguranca (ver a secao RODAS)
  atexit(desligaMotores);               //[C] chama desligaMotores em todo exit()
  signal(SIGINT, trataSinalMotores);    //[C] Ctrl+C
  signal(SIGTERM, trataSinalMotores);   //[C] kill

  //=======================================================================
  // 2) camera e conexao
  //=======================================================================
  //Abre a webcam pedindo 320x240 (a camera pode ignorar o pedido; por isso
  //cada quadro e' conferido e redimensionado no laco).
  VideoCapture w;
  w.open(0);                            //[OpenCV] camera 0
  if (!w.isOpened()) erro("Erro: Abertura de webcam 0.");
  w.set(CAP_PROP_FRAME_WIDTH,320);      //[OpenCV] pede 320 colunas
  w.set(CAP_PROP_FRAME_HEIGHT,240);     //[OpenCV] pede 240 linhas

  SERVER server;                     //[projeto.hpp] abre a porta 3490
  server.waitConnection();           //[projeto.hpp] espera o cliente4

  //=======================================================================
  // 3) laco trava-trava: um comando, um quadro
  //=======================================================================
  Mat_<COR> a;
  BYTE cmd[3];
  server.receiveBytes(3,cmd);        //[projeto.hpp] 'm' 0 0 inicial: cliente pronto
  while (cmd[0]!='s') {
    if (cmd[0]!='m') erro("Erro: comando desconhecido "+to_string(cmd[0]));

    //---- a) aplica nas rodas o comando que acabou de chegar ----
    //Os bytes 2 e 3 de um comando 'm' viram velocidades -1..+1. Cada byte
    //vem como int8 (-128..127) representando a porcentagem -100..100:
    //  ex.: o byte 0xCE = -50 como int8  ->  -0.50 (re a meia potencia)
    //O max/min garante a faixa -1..+1 mesmo se chegar algo como 127.
    double esq = (int8_t)cmd[1] / 100.0;     //byte sem sinal -> com sinal -> -1..+1
    double dir = (int8_t)cmd[2] / 100.0;
    esq = max(-1.0, min(esq, 1.0));
    dir = max(-1.0, min(dir, 1.0));
    motores(esq, dir);

    //---- b) captura um quadro novo, garante 240x320 e manda comprimido (JPEG) ----
    w.read(a);                            //[OpenCV] captura um quadro
    if (a.empty()) erro("Erro: quadro vazio da camera");
    if (a.rows!=240 || a.cols!=320) resize(a, a, Size(320,240));   //[OpenCV] Size(colunas,linhas)
    server.sendImgComp(a);           //[projeto.hpp] JPEG -> cliente4

    //---- c) espera o proximo comando (o cliente responde depois de localizar) ----
    server.receiveBytes(3,cmd);      //[projeto.hpp]
  }
  motores(0,0);                      //'s': para (o atexit ainda desliga a softPwm)
}
