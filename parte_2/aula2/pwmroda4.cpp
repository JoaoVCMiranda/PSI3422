//pwmroda4.cpp - LICAO DE CASA #2/3 (b) da aula 2 (vale 2,5 junto com a montagem)
//Testa softPwm: carrinho gira horario e anti-horario, primeiro com PWM 90%,
//depois repete com 60%.
//raspberry$ compila pwmroda4 -w -ocv
//
//Tensoes esperadas nos pinos 0,1,2,3 (fisicos 11,12,13,15):
//  ~2,97V (90%), ~1,98V (60%) e 0V - onda retangular de 100Hz no osciloscopio.
#include <wiringPi.h>
#include <softPwm.h>
#include <cstdlib>

int main() {
  wiringPiSetup();
  if (softPwmCreate(0, 0, 100)) exit(1);
  if (softPwmCreate(1, 0, 100)) exit(1);
  if (softPwmCreate(2, 0, 100)) exit(1);
  if (softPwmCreate(3, 0, 100)) exit(1);
  for (int i=0; i<2; i++) {
    softPwmWrite(0, 90); softPwmWrite(1, 0);
    softPwmWrite(2, 0);  softPwmWrite(3, 90);  delay(2000);

    softPwmWrite(0, 0);  softPwmWrite(1, 90);
    softPwmWrite(2, 90); softPwmWrite(3,  0);  delay(2000);

    softPwmWrite(0, 60); softPwmWrite(1, 0);
    softPwmWrite(2, 0);  softPwmWrite(3, 60);  delay(2000);

    softPwmWrite(0, 0);  softPwmWrite(1, 60);
    softPwmWrite(2, 60); softPwmWrite(3,  0);  delay(2000);
  }
  //Sem estas 4 linhas, os motores podem continuar energizados ao terminar.
  softPwmStop(0); softPwmStop(1);
  softPwmStop(2); softPwmStop(3);
}
