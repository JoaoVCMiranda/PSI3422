#include "volante.h"

static int16_t clamp_motor(int32_t v)
{
    if (v > INT16_MAX) return INT16_MAX;
    if (v < INT16_MIN) return INT16_MIN;
    return (int16_t)v;
}

static int16_t aplica_trim(int16_t velocidade, int16_t trim)
{
    if (velocidade > 0) return clamp_motor((int32_t)velocidade + trim);
    if (velocidade < 0) return clamp_motor((int32_t)velocidade - trim);
    return 0;
}

void volante_init(volante_t *volante, motor_t *esquerda, motor_t *direita, int16_t trim_esquerda)
{
    volante->esquerda = esquerda;
    volante->direita = direita;
    volante->trim_esquerda = trim_esquerda;
}

void volante_set(volante_t *volante, int16_t esquerda, int16_t direita)
{
    motor_set(volante->esquerda, aplica_trim(esquerda, volante->trim_esquerda));
    motor_set(volante->direita, direita);
}

void volante_frente(volante_t *volante, int16_t velocidade)
{
    volante_set(volante, velocidade, velocidade);
}

void volante_re(volante_t *volante, int16_t velocidade)
{
    volante_set(volante, (int16_t)-velocidade, (int16_t)-velocidade);
}

void volante_direita(volante_t *volante, int16_t velocidade)
{
    volante_set(volante, velocidade, (int16_t)-velocidade);
}

void volante_esquerda(volante_t *volante, int16_t velocidade)
{
    volante_set(volante, (int16_t)-velocidade, velocidade);
}

void volante_para(volante_t *volante)
{
    motor_freia(volante->esquerda);
    motor_freia(volante->direita);
}
