
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
// roda: aplica a velocidade v a UMA roda.
//   v em -1..+1: +1 = frente a toda, -1 = re a toda, 0 = parada.
//   O PWM final e' |v| x potencia calibrada daquele sentido, arredondado.
//   Ex.: v=0.5 com potFrente=80  ->  PWM 40% no pino da frente.
//---------------------------------------------------------------------------
void roda(double v, int pinoFrente, int pinoTras, int potFrente, int potTras) {
  if (v > 0) {                               //para frente
    int pwm = int(v*potFrente + 0.5);        //+0.5 = arredonda
    softPwmWrite(pinoFrente, pwm);           //[wiringPi]
    softPwmWrite(pinoTras, 0);               //[wiringPi]
  } else if (v < 0) {                        //para tras
    int pwm = int(-v*potTras + 0.5);         //-v e' positivo
    softPwmWrite(pinoFrente, 0);             //[wiringPi]
    softPwmWrite(pinoTras, pwm);             //[wiringPi]
  } else {                                   //parada
    softPwmWrite(pinoFrente, 0);             //[wiringPi]
    softPwmWrite(pinoTras, 0);               //[wiringPi]
  }
}

//---------------------------------------------------------------------------
// motores: velocidade das duas rodas de uma vez (cada uma em -1..+1).
//   motores(1,1) = frente   motores(-1,-1) = re   motores(0,0) = para
//   motores(1,0.3) = curva para a DIREITA (a roda esquerda corre mais)
//---------------------------------------------------------------------------
void motores(double esq, double dir) {
  roda(esq, ESQ_FRENTE, ESQ_TRAS, frenteEsq, reEsq);
  roda(dir, DIR_FRENTE, DIR_TRAS, frenteDir, reDir);
}

//---------------------------------------------------------------------------
// SEGURANCA: os motores NUNCA podem ficar ligados quando o programa
// termina - seja por comando, queda da conexao (erro() -> exit) ou
// Ctrl+C/kill. Dois mecanismos:
//   atexit(desligaMotores)  roda em TODO exit() (inclusive o do erro())
//   signal(..., trataSinal) transforma Ctrl+C (SIGINT) e kill (SIGTERM)
//                           num exit(), que por sua vez chama o atexit
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

//---------------------------------------------------------------------------
// iniciaMotores: le as potencias opcionais da linha de comando e liga a
// softPwm nos 4 pinos.
//   prog                                   usa as potencias padrao (80)
//   prog frenteEsq frenteDir reEsq reDir   ex.: servidor4 76 80 80 78
//---------------------------------------------------------------------------
void iniciaMotores(int argc, char* argv[], const string& prog) {
  //---- 1) potencias da linha de comando (opcionais) ----
  if (argc==5) {
    frenteEsq=atoi(argv[1]); frenteDir=atoi(argv[2]);   //[C] texto -> int
    reEsq=atoi(argv[3]);     reDir=atoi(argv[4]);
  } else if (argc!=1) {
    erro(prog+" [frenteEsq frenteDir reEsq reDir]");    //[raspberry.hpp]
  }
  printf("Potencias: frente %d/%d  re %d/%d (esq/dir)\n",frenteEsq,frenteDir,reEsq,reDir);

  //---- 2) liga a softPwm: faixa 0..100, comecando em 0 (parado) ----
  wiringPiSetup();                      //[wiringPi]
  if (softPwmCreate(ESQ_FRENTE,0,100)) erro("Erro: softPwmCreate");   //[wiringPi]
  if (softPwmCreate(ESQ_TRAS,0,100))   erro("Erro: softPwmCreate");
  if (softPwmCreate(DIR_FRENTE,0,100)) erro("Erro: softPwmCreate");
  if (softPwmCreate(DIR_TRAS,0,100))   erro("Erro: softPwmCreate");

  //---- 3) seguranca (ver acima) ----
  atexit(desligaMotores);               //[C] chama desligaMotores em todo exit()
  signal(SIGINT, trataSinalMotores);    //[C] Ctrl+C
  signal(SIGTERM, trataSinalMotores);   //[C] kill
}

//#########################################################################
// CAMERA, PROTOCOLO E PROGRAMA
//#########################################################################

//Abre a webcam pedindo 320x240 (a camera pode ignorar o pedido; por isso
//cada quadro e' conferido e redimensionado no laco).
void abreCamera(VideoCapture& w) {
  w.open(0);                            //[OpenCV] camera 0
  if (!w.isOpened()) erro("Erro: Abertura de webcam 0.");
  w.set(CAP_PROP_FRAME_WIDTH,320);      //[OpenCV] pede 320 colunas
  w.set(CAP_PROP_FRAME_HEIGHT,240);     //[OpenCV] pede 240 linhas
}

//Traduz os bytes 2 e 3 de um comando 'm' em velocidades -1..+1.
//Cada byte vem como int8 (-128..127) representando a porcentagem -100..100:
//  ex.: o byte 0xCE = -50 como int8  ->  -0.50 (re a meia potencia)
//O max/min garante a faixa -1..+1 mesmo se chegar algo como 127.
void interpretaMovimento(const BYTE cmd[3], double& esq, double& dir) {
  esq = (int8_t)cmd[1] / 100.0;     //byte sem sinal -> com sinal -> -1..+1
  dir = (int8_t)cmd[2] / 100.0;
  esq = max(-1.0, min(esq, 1.0));
  dir = max(-1.0, min(dir, 1.0));
}

//Captura um quadro e garante o tamanho 240x320.
void capturaQuadro(VideoCapture& w, Mat_<COR>& a) {
  w.read(a);                            //[OpenCV] captura um quadro
  if (a.empty()) erro("Erro: quadro vazio da camera");
  if (a.rows!=240 || a.cols!=320) resize(a, a, Size(320,240));   //[OpenCV] Size(colunas,linhas)
}

int main(int argc, char *argv[]) {
  iniciaMotores(argc, argv, "servidor4");   //softPwm + seguranca (secao RODAS)
  VideoCapture w;
  abreCamera(w);

  SERVER server;                     //[projeto.hpp] abre a porta 3490
  server.waitConnection();           //[projeto.hpp] espera o cliente4

  Mat_<COR> a;
  BYTE cmd[3];
  server.receiveBytes(3,cmd);        //[projeto.hpp] 'm' 0 0 inicial: cliente pronto
  while (cmd[0]!='s') {
    if (cmd[0]!='m') erro("Erro: comando desconhecido "+to_string(cmd[0]));

    //1) aplica nas rodas o comando que acabou de chegar
    double esq, dir;
    interpretaMovimento(cmd, esq, dir);
    motores(esq, dir);

    //2) captura um quadro novo e manda comprimido (JPEG)
    capturaQuadro(w, a);
    server.sendImgComp(a);           //[projeto.hpp] JPEG -> cliente4

    //3) espera o proximo comando (o cliente responde depois de localizar)
    server.receiveBytes(3,cmd);      //[projeto.hpp]
  }
  motores(0,0);                      //'s': para (o atexit ainda desliga a softPwm)
}
