//paramotor.cpp - PARADA DE EMERGENCIA
//Deixe este programa SEMPRE compilado e pronto para usar: se algum programa
//terminar com os motores ligados, rode-o para desliga-los.
//raspberry$ compila paramotor -w
#include <wiringPi.h>

int main () {
  wiringPiSetup () ;
  pinMode (0, OUTPUT) ;
  pinMode (1, OUTPUT) ;
  pinMode (2, OUTPUT) ;
  pinMode (3, OUTPUT) ;
  digitalWrite (0,  LOW) ;
  digitalWrite (1,  LOW) ;
  digitalWrite (2,  LOW) ;
  digitalWrite (3,  LOW) ;
}
