#include "odometria.h"

static float fabsf_local(float v)
{
    return v < 0.0f ? -v : v;
}

void odometria_init(odometria_pose_t *pose)
{
    pose->theta_rad = 0.0f;
    pose->distancia_percorrida_m = 0.0f;
}

void odometria_atualiza(odometria_pose_t *pose, const odometria_calibracao_t *calib,
                         int32_t delta_pulsos_esq, int32_t delta_pulsos_dir)
{
    float d_esq = (float)delta_pulsos_esq * calib->circunferencia_roda_m / (float)calib->pulsos_por_volta;
    float d_dir = (float)delta_pulsos_dir * calib->circunferencia_roda_m / (float)calib->pulsos_por_volta;

    pose->theta_rad += (d_dir - d_esq) / calib->distancia_entre_rodas_m;
    pose->distancia_percorrida_m += (fabsf_local(d_esq) + fabsf_local(d_dir)) * 0.5f;
}
