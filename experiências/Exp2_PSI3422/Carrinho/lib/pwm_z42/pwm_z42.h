/*
 * pwm_z42.h — por Prof. Gustavo Rehder (gprehder/pwm)
 * Cópia de PSI3441/entregas/include/pwm_z42.h (fonte confiável indicada
 * pelo usuário) + PSI3441/entregas/5/src/pwm_z42.c.
 *
 * Por que essa biblioteca existe e por que o MVP usa ela para o PWM
 * dos motores (enA/enB), em vez da API padrão do Zephyr:
 *
 *   1. Motivo histórico: pwm_z42 é de 2017 (ver header original,
 *      "Created on: 07/01/2017"), anterior a este curso ter adotado
 *      Zephyr — é a camada de acesso a TPM que o Prof. Rehder já usava
 *      em cursos bare-metal (SDK Kinetis), reaproveitada depois porque
 *      resolve um problema real que a API Zephyr não cobre (item 2).
 *
 *   2. Motivo técnico documentado (ver PSI3441 Ativ.5, relatório):
 *      pwm_set_pulse_dt()/pwm_set_dt() da API Zephyr só escrevem
 *      TPM_CnV (duty). Não existe chamada Zephyr para reconfigurar
 *      TPM_MOD (período) em runtime — só dá para fazer isso escrevendo
 *      direto no registrador, que é exatamente o que pwm_z42 faz.
 *
 *   3. Motivo específico deste MVP, verificado nesta árvore de
 *      compilação (framework-zephyr@2.20701.220422, board frdm_kl25z):
 *      não existe NENHUM nó de TPM na devicetree deste port
 *      (conferido em dts/arm/nxp/nxp_kl25z.dtsi — só há flash, mcg,
 *      i2c0/i2c1, sim, uart0, adc0, pinmux porta-e, gpioa-e, usbotg).
 *      Ou seja: usar a API Zephyr de PWM aqui não seria só "não dá pra
 *      mudar o período" — literalmente não existe pwm_dt_spec possível
 *      para TPM neste board, porque não há binding nem nó de
 *      devicetree para isso. Criar um do zero (binding + dtsi +
 *      pinctrl) é infraestrutura fora do escopo de um MVP. pwm_z42
 *      contorna isso porque não depende do modelo de device do Zephyr
 *      — mexe direto nos registradores via CMSIS (MKL25Z4.h), então
 *      não importa se a devicetree "sabe" que o TPM existe.
 *
 * A abordagem Zephyr nativa (comentada, não usada) está em motor.c.
 */
#ifndef SOURCES_PWM_H_
#define SOURCES_PWM_H_

#include "stdbool.h"
#include <stdint.h>
#include "MKL25Z4.h"

/*
 * MKL25Z4.h vem do hal_nxp que o framework-zephyr já baixa (CMSIS
 * moderno: `TPM0`/`SIM`/`PORTA`/`GPIOA`/... já existem como ponteiros
 * `TPM_Type*`/`SIM_Type*`/etc — não precisamos copiar um MKL25Z4.h
 * próprio nem redefinir esses nomes; os #ifndef abaixo só entram em
 * ação se este arquivo for usado fora deste framework, contra um
 * MKL25Z4.h estilo SDK antigo que só define *_BASE_PTR). Só os
 * ALIASES DE TIPO (TPM_MemMapPtr/GPIO_MemMapPtr, usados nos protótipos
 * abaixo) faltam no header moderno — esses sim precisam ser definidos
 * aqui sempre. */
#ifndef TPM_MemMapPtr
#define TPM_MemMapPtr TPM_Type*
#endif
#ifndef GPIO_MemMapPtr
#define GPIO_MemMapPtr GPIO_Type*
#endif

/* Aliases: mapeiam nomes CMSIS-2 para os ponteiros base do MKL25Z4.h */
#ifndef SIM
#define SIM    SIM_BASE_PTR
#endif
#ifndef TPM0
#define TPM0   TPM0_BASE_PTR
#endif
#ifndef TPM1
#define TPM1   TPM1_BASE_PTR
#endif
#ifndef TPM2
#define TPM2   TPM2_BASE_PTR
#endif
#ifndef GPIOA
#define GPIOA  PTA_BASE_PTR
#endif
#ifndef GPIOB
#define GPIOB  PTB_BASE_PTR
#endif
#ifndef GPIOC
#define GPIOC  PTC_BASE_PTR
#endif
#ifndef GPIOD
#define GPIOD  PTD_BASE_PTR
#endif
#ifndef GPIOE
#define GPIOE  PTE_BASE_PTR
#endif
#ifndef PORTA
#define PORTA  PORTA_BASE_PTR
#endif
#ifndef PORTB
#define PORTB  PORTB_BASE_PTR
#endif
#ifndef PORTC
#define PORTC  PORTC_BASE_PTR
#endif
#ifndef PORTD
#define PORTD  PORTD_BASE_PTR
#endif
#ifndef PORTE
#define PORTE  PORTE_BASE_PTR
#endif

/* TPM clock source select */
#define TPM_CLK_DIS   0
#define TPM_PLLFLL    1
#define TPM_OSCERCLK  2
#define TPM_MCGIRCLK  3

#define TPM_CNT_DIS   0
#define TPM_CLK       1
#define TPM_EXT_CLK   2

/* Prescaler */
#define PS_1    0
#define PS_2    1
#define PS_4    2
#define PS_8    3
#define PS_16   4
#define PS_32   5
#define PS_64   6
#define PS_128  7

/* Channel modes */
#define TPM_OC_TOGGLE  TPM_CnSC_MSA_MASK|TPM_CnSC_ELSA_MASK
#define TPM_OC_CLR     TPM_CnSC_MSA_MASK|TPM_CnSC_ELSB_MASK
#define TPM_OC_SET     TPM_CnSC_MSA_MASK|TPM_CnSC_ELSA_MASK|TPM_CnSC_ELSB_MASK
#define TPM_OC_OUTL    TPM_CnSC_MSB_MASK|TPM_CnSC_MSA_MASK|TPM_CnSC_ELSB_MASK
#define TPM_OC_OUTH    TPM_CnSC_MSB_MASK|TPM_CnSC_MSA_MASK|TPM_CnSC_ELSA_MASK

#define TPM_PWM_H   TPM_CnSC_MSB_MASK|TPM_CnSC_ELSB_MASK
#define TPM_PWM_L   TPM_CnSC_MSB_MASK|TPM_CnSC_ELSA_MASK

#define EDGE_PWM    0
#define CENTER_PWM  1

/* TPM_MemMapPtr e GPIO_MemMapPtr ja sao tipos definidos em MKL25Z4.h */

bool pwm_tpm_Init(TPM_MemMapPtr tpm, uint16_t clk, uint16_t module,
                  uint8_t clock_mode, uint8_t ps, bool counting_mode);

bool pwm_tpm_Ch_Init(TPM_MemMapPtr tpm, uint16_t channel, uint8_t mode,
                     GPIO_MemMapPtr gpio, uint8_t pin);

void pwm_tpm_CnV(TPM_MemMapPtr TPMx, uint16_t channel, uint16_t value);

#endif /* SOURCES_PWM_H_ */
