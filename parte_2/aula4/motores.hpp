//motores.hpp - Raspberry - controle das rodas, usado pelo servidor4.
//Mesmo mapeamento GPIO e mesma calibracao do ../aula3/tarefa6/servidor2.cpp
//(la' esta' a explicacao de como calibrar). Se recalibrar, atualize os dois.
#ifndef MOTORES_HPP
#define MOTORES_HPP
#include "../projeto.hpp"
#include <wiringPi.h>
#include <softPwm.h>
#include <csignal>

//Conferido roda a roda em 27/10/2026 (rodas no ar, frente = lado da camera):
//motor A (pinos 0,1) = roda ESQUERDA, frente no pino 1;
//motor B (pinos 2,3) = roda DIREITA, frente no pino 2.
const int ESQ_FRENTE=1, ESQ_TRAS=0;
const int DIR_FRENTE=2, DIR_TRAS=3;

//Potencias calibradas (%): 100% do comando = esta potencia naquela roda
int frenteEsq=80, frenteDir=80;
int reEsq=80, reDir=80;

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

//Os motores NUNCA podem ficar ligados quando o programa termina - seja por
//comando, queda da conexao (erro() -> exit) ou Ctrl+C/kill.
void desligaMotores() {
  softPwmStop(ESQ_FRENTE); softPwmStop(ESQ_TRAS);
  softPwmStop(DIR_FRENTE); softPwmStop(DIR_TRAS);
}

void trataSinalMotores(int) {
  motores(0,0);   //zera ja'; o softPwmStop roda no atexit
  exit(1);
}

//Le as potencias opcionais da linha de comando e liga o softPwm.
//  prog [frenteEsq frenteDir reEsq reDir]
void iniciaMotores(int argc, char* argv[], const string& prog) {
  if (argc==5) {
    frenteEsq=atoi(argv[1]); frenteDir=atoi(argv[2]);
    reEsq=atoi(argv[3]);     reDir=atoi(argv[4]);
  } else if (argc!=1) erro(prog+" [frenteEsq frenteDir reEsq reDir]");
  printf("Potencias: frente %d/%d  re %d/%d (esq/dir)\n",frenteEsq,frenteDir,reEsq,reDir);

  wiringPiSetup();
  if (softPwmCreate(ESQ_FRENTE,0,100)) erro("Erro: softPwmCreate");
  if (softPwmCreate(ESQ_TRAS,0,100))   erro("Erro: softPwmCreate");
  if (softPwmCreate(DIR_FRENTE,0,100)) erro("Erro: softPwmCreate");
  if (softPwmCreate(DIR_TRAS,0,100))   erro("Erro: softPwmCreate");
  atexit(desligaMotores);
  signal(SIGINT, trataSinalMotores);
  signal(SIGTERM, trataSinalMotores);
}

#endif
