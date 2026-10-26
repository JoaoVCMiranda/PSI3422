//testarodas.cpp - Raspberry - diagnostico da Tarefa 6
//Liga UM pino da ponte-H por vez (60%, 2 s), para descobrir qual roda cada
//pino move e em que sentido. Com isso se acerta o mapeamento no topo do
//servidor2.cpp (ESQ_FRENTE, ESQ_TRAS, DIR_FRENTE, DIR_TRAS).
//"Frente" do carrinho = o lado para onde a camera aponta.
//raspberry$ compila testarodas -w
//raspberry$ ./testarodas
#include <wiringPi.h>
#include <softPwm.h>
#include <cstdio>
#include <cstdlib>
#include <csignal>

void desliga() { for (int p=0; p<4; p++) softPwmStop(p); }
void trataSinal(int) { for (int p=0; p<4; p++) softPwmWrite(p,0); exit(1); }

int main() {
  wiringPiSetup();
  for (int p=0; p<4; p++) if (softPwmCreate(p,0,100)) exit(1);
  atexit(desliga);
  signal(SIGINT, trataSinal);

  printf("Carrinho com as RODAS NO AR. Frente = lado para onde a camera aponta.\n");
  printf("Para cada pino, anote: qual roda (esquerda/direita) e o sentido (frente/tras).\n\n");
  for (int p=0; p<4; p++) {
    printf("Pino %d: ligando em 3 s...\n", p); fflush(stdout);
    delay(3000);
    printf("Pino %d LIGADO (2 s) -> qual roda? para frente ou para tras?\n", p); fflush(stdout);
    softPwmWrite(p,60);
    delay(2000);
    softPwmWrite(p,0);
  }
  printf("\nFim. Me passe as 4 respostas (ex.: 0 = esquerda tras, 1 = esquerda frente, ...).\n");
}
