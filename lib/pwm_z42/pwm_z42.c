/*
 * pwm_z42.c — biblioteca original do Prof. Gustavo Rehder.
 * Base: PSI3441/entregas/5/src/pwm_z42.c (fonte confiável indicada
 * pelo usuário).
 *
 * ÚNICA MUDANÇA em relação ao original: as validações de faixa de pino
 * usavam `||` onde o autor claramente queria um intervalo fechado
 * (`&&`) — ex.: `if((pin>=3) || (pin<=5))` é sempre verdadeira para
 * qualquer inteiro, então a checagem nunca rejeitava um pino inválido.
 * Isso não importava enquanto a lib ficava sem uso, mas agora ela
 * pilota o enA/enB reais dos motores — um pino errado nunca detectado
 * pode energizar o pino físico errado da placa. Corrigido em todos os
 * pontos, mantendo os limites numéricos originais (só o operador
 * mudou). A lógica de registrador (SIM, PORT, TPM) não foi alterada.
 */
#include "pwm_z42.h"

bool pwm_tpm_Init(TPM_MemMapPtr tpm, uint16_t clk, uint16_t module, uint8_t clock_mode,
                  uint8_t ps, bool counting_mode)
{
	if(tpm == TPM0)
	{
		SIM->SCGC6 |= SIM_SCGC6_TPM0_MASK;
	}
	else if(tpm == TPM1)
	{
		SIM->SCGC6 |= SIM_SCGC6_TPM1_MASK;
	}
	else if(tpm == TPM2)
	{
		SIM->SCGC6 |= SIM_SCGC6_TPM2_MASK;
	}
	else
	{
		return false;
	}

	SIM->SOPT2 |= SIM_SOPT2_TPMSRC(clk);

	tpm->MOD = module;

	tpm->SC |= TPM_SC_CMOD(clock_mode) | TPM_SC_PS(ps);

	if(counting_mode == CENTER_PWM)
	{
		tpm->SC |= TPM_SC_CPWMS_MASK;
	}
	else if(counting_mode == EDGE_PWM)
	{
		tpm->SC &= ~TPM_SC_CPWMS_MASK;
	}
	else
	{
		return false;
	}
	return true;
}
/****************************************************************************************
*
*****************************************************************************************/
bool pwm_tpm_Ch_Init(TPM_MemMapPtr tpm, uint16_t channel, uint8_t mode,
                     GPIO_MemMapPtr gpio,uint8_t pin)
{
	if(tpm == TPM0)
	{
		if(gpio == GPIOA)
		{
			if((channel<=2)||(channel==5))
			{
				if((pin>=3) && (pin<=5))
				{
					SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;
					PORTA->PCR[pin] = PORT_PCR_MUX(3);
				}
				else return false;
			}
			else return false;
		}
		else if(gpio == GPIOC)
		{
			if(channel<=5)
			{
				if((pin==8) || (pin==9))
				{
					SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
					PORTC->PCR[pin] = PORT_PCR_MUX(3);
				}
				else if((pin >= 1 && pin <= 4))
				{
					SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
					PORTC->PCR[pin] = PORT_PCR_MUX(4);
				}
				else return false;
			}
			else return false;
		}
		else if(gpio == GPIOD)
		{
			if(channel<=5)
			{
				if(pin<=5)
				{
					SIM->SCGC5 |= SIM_SCGC5_PORTD_MASK;
					PORTD->PCR[pin] = PORT_PCR_MUX(4);
				}
				else return false;
			}
			else return false;
		}
		else if(gpio == GPIOE)
		{
			if(channel<=4)
			{
				if( (pin>=24 && pin<=25) || (pin>=29 && pin<=31) )
				{
					SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;
					PORTE->PCR[pin] = PORT_PCR_MUX(3);
				}
				else return false;
			}
			else return false;
		}
		else return false;
	}
	else if(tpm == TPM1)
	{
		if(channel <= 1)
		{
			if(gpio == GPIOA)
			{
				if(pin>=12 && pin<=13)
				{
					SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;
					PORTA->PCR[pin] = PORT_PCR_MUX(3);
				}
				else return false;
			}
			else if(gpio == GPIOB)
			{
				if(pin<=1)
				{
					SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
					PORTB->PCR[pin] = PORT_PCR_MUX(3);
				}
				else return false;
			}
			else if(gpio == GPIOE)
			{
				if(pin>=20 && pin<=21)
				{
					SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;
					PORTE->PCR[pin] = PORT_PCR_MUX(3);
				}
				else return false;
			}
			else return false;
		}
		else return false;
	}
	else if(tpm == TPM2)
	{
		if(channel <= 1)
		{
			if(gpio == GPIOA)
			{
				if(pin>=1 && pin<=2)
				{
					SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;
					PORTA->PCR[pin] = PORT_PCR_MUX(3);
				}
				else return false;
			}
			else if(gpio == GPIOB)
			{
				if((pin>=2 && pin<=3) || (pin>=18 && pin<=19))
				{
					SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
					PORTB->PCR[pin] = PORT_PCR_MUX(3);
				}
				else return false;
			}
			else if(gpio == GPIOE)
			{
				if(pin>=22 && pin<=23)
				{
					SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;
					PORTE->PCR[pin] = PORT_PCR_MUX(3);
				}
				else return false;
			}
			else return false;
		}
		else return false;
	}
	else return false;

	tpm->CONTROLS[channel].CnSC |= mode;

	return true;
}
/****************************************************************************************
*
*****************************************************************************************/
void pwm_tpm_CnV(TPM_MemMapPtr tpm, uint16_t channel, uint16_t value)
{
	tpm->CONTROLS[channel].CnV = value;
}
/***************************************************************************************/
