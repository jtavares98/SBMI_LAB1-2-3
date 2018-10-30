/* * main.c
 *
 *  Created on: 09/10/2018
 *      Author: 	Jorge Tavares
 *      		      Tiago Martins
 *
 *	 - Implementação em código de uma máquina de estados que controla os semáforos de um cruzamento com uma funcionalidade
 *	de emergência
 *	 - A comutação entre luzes de semáforos é feita usando os intervalos de tempo t_long(50s) e t_short(5s)
 *	 - Em qualquer caso se o botão de emergência (EMR) for pressionado o trânsito deve ser suspendido e retomado na
 *	direção inversão passados 10s (t_emr)
 *
*/

#include <avr/io.h>
#include <util/delay.h>
#include "timer_tools.h"
#include "serial_printf.h"
#include <avr/interrupt.h>

#define RNS PB0
#define YNS PB1
#define GNS PB2
#define REW PB3
#define YEW PB4
#define GEW PB5

#define EMR PD2  //PD7

#define longo 5000
#define curto 3000
#define emr 5000

volatile unsigned char state=1, state_emr=10, pstate=1, emr_flag=0;

ISR(INT0_vect) {
	emr_flag=1; // indica que se entrou no estado de emergencia
	state=0; // desliga a maquina de estados normal
}

void hw_init(void){
  DDRB = DDRB | 0b00111111; //DEFINIR OUTPUTS
  DDRD = DDRD | 0b00000000; //DEFINIR INPUT
  //PORTD = PORTD | (1<<EMR); //RESISTENCIA PULL UP
  EICRA = EICRA | (2<<ISC00); /*Interrupts request at falling edge*/
  EIMSK = EIMSK | (1<<INT0);  /* Enables INT0 */
  EIFR= EIFR | (1<<INTF0);
  sei(); /* Enable global interrupt flag */

}

int main(void) {
	//DDRB = DDRB | (0b00111111); // DEFINIR PB0-PB5 COMO OUTPUTS

	hw_init();

	mili_timer t_long, t_short, t_emr;

	init_mili_timers();

	start_timer(&t_short, curto);
	start_timer(&t_long, longo);

	while (1) {

		// SAIDAS DAS MAQUINAS DE ESTADOS
		if ((1==state) && (10==state_emr))
			PORTB = (PORTB & ~PORTB) | (1<<GNS) | (1<<REW); // LIGAR VERDE NORTE-SUL E VERMELHO ESTE-OESTE
		else if (((2==state)  && (10==state_emr)) || (11==state_emr))
			PORTB = (PORTB & ~PORTB) | (1<<YNS) | (1<<REW); // LIGAR AMARELO NORTE-SUL E VERMELHO ESTE-OESTE
		else if (((3==state) && (10==state_emr)) || (13==state_emr))
			PORTB = (PORTB & ~PORTB) | (1<<RNS) | (1<<REW); // LIGAR VERMELHO NORTE-SUL E VERMELHO ESTE-OESTE
		else if ((4==state) && (10==state_emr))
			PORTB = (PORTB & ~PORTB) | (1<<RNS) | (1<<GEW); // LIGAR VERMELHO NORTE-SUL E VERDE ESTE-OESTE
		else if (((5==state) && (10==state_emr)) || (12==state_emr))
			PORTB = (PORTB & ~PORTB) | (1<<RNS) | (1<<YEW); // LIGAR VERMELHO NORTE-SUL E AMARELO ESTE-OESTE
		else if (((6==state) && (10==state_emr)) || (14==state_emr))
			PORTB = (PORTB & ~PORTB) | (1<<RNS) | (1<<REW); // LIGAR VERMELHO NORTE-SUL E VERMELHO ESTE-OESTE


		// TRANSICOES DA MAQUINA DE ESTADO NORMAL
		if (get_timer(&t_long) && (1==state)) { // TRANSICAO 1->2 [t_long.q E start_timer(t_short)]
			start_timer(&t_short, curto);
			PORTB = (PORTB & ~(1<<GNS)); // DESLIGAR VERDE NORTE-SUL
			state=2;
			pstate=2;
		}

		if (get_timer(&t_short) && (2==state)) { // TRANSICAO 2->3 [t_short.q E start_timer(t_short)]
			PORTB = (PORTB & ~(1<<YNS)); // DESLIGAR AMARELO NORTE-SUL
			start_timer(&t_short, curto);
			state=3;
			pstate=3;
		}

		if (get_timer(&t_short) && (3==state)) { // TRANSICAO 3->4 [t_short.q E start_timer(t_long)]
			PORTB = (PORTB ^ (1<<REW)); // DESLIGAR VERMELHO ESTE-OESTE
			start_timer(&t_long, longo);
			state=4;
			pstate=4;
		}

		if (get_timer(&t_long) && (4==state)) { // TRANSICAO 4->5 [t_long.q E start_timer(t_short)]
			PORTB = (PORTB ^ (1<<GEW));  // DESLIGAR VERDE ESTE-OESTE
			start_timer(&t_short, curto);
			state=5;
			pstate=5;
		}

		if (get_timer(&t_short) && (5==state)) { // TRANSICAO 5->6 [t_short.q E start_timer(t_short)]
			PORTB = (PORTB ^ (1<<YEW)); // DESLIGAR AMARELO ESTE-OESTE
			start_timer(&t_short, curto);
			state=6;
			pstate=6;
		}

		if (get_timer(&t_short) && (6==state)) { // TRANSICAO 6->1 [t_short.q E start_timer(t_long)]
			start_timer(&t_long, longo);
			state=1;
			pstate=1;
		}

		// Maquina de Emergência
		// NORTE-SUL
		if ((1==emr_flag) && ((1==pstate) || (2==pstate)) && (10==state_emr)) { // TRANSICAO 10->11
			if (1==pstate)
				start_timer(&t_short, curto); // se vier do verde liga t_short; se vier do amarelo nao liga
			state_emr=11;
		}

		if ((11==state_emr) && (get_timer(&t_short))) { // TRANSICAO 11->13
			start_timer(&t_emr, emr);
			state_emr=13;
		}

		if ((13==state_emr) && (get_timer(&t_emr))) { // TRANSICAO 13->10
			state_emr=10;
			state=4;
			pstate=4;
			emr_flag=0; // transicao de saida do estado de emergencia
			start_timer(&t_long, longo);
		}

		if ((1==emr_flag) && (3==pstate) && (10==state_emr)) { // TRANSICAO 10->13
			start_timer(&t_emr, emr);
			state_emr=13;
		}

		// ESTE-OESTE
		if ((1==emr_flag) && ((4==pstate) || (5==pstate)) && (10==state_emr)) { // TRANSICAO 10->12
			if (4==pstate)
				start_timer(&t_short, curto); // se vier do verde liga t_short; se vier do amarelo nao
			state_emr=12;
		}

		if ((12==state_emr) && (get_timer(&t_short))) { // TRANSICAO 12->14
			start_timer(&t_emr, emr);
			state_emr=14;
		}

		if ((14==state_emr) && (get_timer(&t_emr))) { // TRANSICAO 14->10
			state_emr=10;
			state=1;
			pstate=1;
			emr_flag=0; // transicao de saida do estado de emergencia
			start_timer(&t_long, longo);
		}

		if ((1==emr_flag) && (6==pstate) && (10==state_emr)) { // TRANSICAO 10->14
			start_timer(&t_emr, emr);
			state_emr=14;
		}
	}

	return 1;
}





