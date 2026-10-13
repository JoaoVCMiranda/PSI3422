#include <wiringPi.h>
#include <softPwm.h>
#include <cstdlib>

int main() {
    wiringPiSetup();

    if (softPwmCreate(0, 0, 100)) exit(1);
    if (softPwmCreate(1, 0, 100)) exit(1);
    if (softPwmCreate(2, 0, 100)) exit(1);
    if (softPwmCreate(3, 0, 100)) exit(1);

    // Giro horario com PWM de 90%
    softPwmWrite(0, 90);
    softPwmWrite(1, 0);
    softPwmWrite(2, 0);
    softPwmWrite(3, 90);
    delay(2000);

    // Giro anti-horario com PWM de 90%
    softPwmWrite(0, 0);
    softPwmWrite(1, 90);
    softPwmWrite(2, 90);
    softPwmWrite(3, 0);
    delay(2000);

    // Giro horario com PWM de 60%
    softPwmWrite(0, 60);
    softPwmWrite(1, 0);
    softPwmWrite(2, 0);
    softPwmWrite(3, 60);
    delay(2000);

    // Giro anti-horario com PWM de 60%
    softPwmWrite(0, 0);
    softPwmWrite(1, 60);
    softPwmWrite(2, 60);
    softPwmWrite(3, 0);
    delay(2000);

    // Desliga os motores
    softPwmWrite(0, 0);
    softPwmWrite(1, 0);
    softPwmWrite(2, 0);
    softPwmWrite(3, 0);

    softPwmStop(0);
    softPwmStop(1);
    softPwmStop(2);
    softPwmStop(3);

    return 0;
}