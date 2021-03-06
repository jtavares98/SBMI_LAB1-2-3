
/* * main.c
 *
 *  Created on: 09/10/2018
 *      Author: 	Jorge Tavares
 *      		Tiago Martins
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

#define longo 10000
#define curto 5000
#define emr 10000

#define TIME_SHORT 300
#define TIME_LONG 500
#define TIME_EMR 500

#define T1TOP 625  // CTC mode

volatile unsigned char state=1, state_emr=10, pstate=1, emr_flag=0;
int time=0; // MUDEI O TIME PARA INT PQ ESTAVA A TRUNCAR VALORES ELEVADOS (ASSIM ERAM MUITO RAPIDAS AS TRANSICOES)

/* INICIALIZACAO DO TIMER1 NO MODO CTC */
void tc1_init(void) {
	// Stop TC1 and clear pending interrupts
	TCCR1B = 0;
	TIFR1 |= (7<<TOV1);

	// Define mode CTC
	TCCR1A = 0;
	TCCR1B = (1<<WGM12);

	// Load BOTTOM and TOP values
	TCNT1 = 0;
	OCR1A = T1TOP;

	// Enable COMPA interrupt
	TIMSK1 = (1<<OCIE1A);

	// Start TC1 with a prescaller of 256
	TCCR1B |= 4;
}

ISR(INT0_vect) {
	emr_flag=1; // indica que se entrou no estado de emergencia
	state=0; // desliga a maquina de estados normal
}

ISR(TIMER1_COMPA_vect) {
	// ReLoad BOTTOM and TOP values
	TCNT1 = 0;
	OCR1A = T1TOP;

	time--;
}

void hw_init(void){
  DDRB = DDRB | 0b00111111; //DEFINIR OUTPUTS
  DDRD = DDRD | 0b00000000; //DEFINIR INPUT
  //PORTD = PORTD | (1<<EMR); //RESISTENCIA PULL UP
  EICRA = EICRA | (3<<ISC00); /*Interrupts request at RISING edge*/
  EIMSK = EIMSK | (1<<INT0);  /* Enables INT0 */
  EIFR= EIFR | (1<<INTF0);
  sei(); /* Enable global interrupt flag */
}

int main(void) {
	//DDRB = DDRB | (0b00111111); // DEFINIR PB0-PB5 COMO OUTPUTS

	printf("\n\n\rHello WORLD!\n\r");

	hw_init();
	sei(); // enable global intr service

	time = TIME_LONG;
	tc1_init();

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
		if ((!time) && (1==state)) { // TRANSICAO 1->2 [t_long.q=(t_big==5) E start_timer(t_short)]
			time = TIME_SHORT; // start_timer(t_short)
			tc1_init();
			PORTB = (PORTB & ~(1<<GNS)); // DESLIGAR VERDE NORTE-SUL
			state=2;
			pstate=2;
		}

		if ((!time) && (2==state)) { // TRANSICAO 2->3 [t_short.q E start_timer(t_short)]
			PORTB = (PORTB & ~(1<<YNS)); // DESLIGAR AMARELO NORTE-SUL
			time = TIME_SHORT; // start_timer(&t_short, curto);
			tc1_init();
			state=3;
			pstate=3;
		}

		if ((!time) && (3==state)) { // TRANSICAO 3->4 [t_short.q E start_timer(t_long)]
			PORTB = (PORTB ^ (1<<REW)); // DESLIGAR VERMELHO ESTE-OESTE
			time = TIME_LONG; // start_timer(&t_long, longo);
			tc1_init();
			state=4;
			pstate=4;
		}

		if ((!time) && (4==state)) { // TRANSICAO 4->5 [t_long.q E start_timer(t_short)]
			PORTB = (PORTB ^ (1<<GEW));  // DESLIGAR VERDE ESTE-OESTE
			time = TIME_SHORT; // start_timer(&t_short, curto);
			tc1_init();
			state=5;
			pstate=5;
		}

		if ((!time) && (5==state)) { // TRANSICAO 5->6 [t_short.q E start_timer(t_short)]
			PORTB = (PORTB ^ (1<<YEW)); // DESLIGAR AMARELO ESTE-OESTE
			time = TIME_SHORT; // start_timer(&t_short, curto);
			tc1_init();
			state=6;
			pstate=6;
		}

		if ((!time) && (6==state)) { // TRANSICAO 6->1 [t_short.q E start_timer(t_long)]
			time = TIME_LONG; // start_timer(&t_long, longo);
			tc1_init();
			state=1;
			pstate=1;
		}

		// Maquina de Emergência
		// NORTE-SUL
		if ((1==emr_flag) && ((1==pstate) || (2==pstate)) && (10==state_emr)) { // TRANSICAO 10->11
			if (1==pstate) {
				time = TIME_SHORT; //start_timer(&t_short, curto); // se vier do verde liga t_short; se vier do amarelo nao liga
				tc1_init();
			}
			state_emr=11;
		}

		if ((11==state_emr) && (!time)) { // TRANSICAO 11->13
			time = TIME_EMR; // start_timer(&t_emr, emr);
			tc1_init();
			state_emr=13;
		}

		if ((13==state_emr) && (!time)) { // TRANSICAO 13->10
			state_emr=10;
			state=4;
			pstate=4;
			emr_flag=0; // transicao de saida do estado de emergencia
			time = TIME_LONG; // start_timer(&t_long, longo);
			tc1_init();
		}

		if ((1==emr_flag) && (3==pstate) && (10==state_emr)) { // TRANSICAO 10->13
			time = TIME_EMR; // start_timer(&t_emr, emr);
			tc1_init();
			state_emr=13;
		}

		// ESTE-OESTE
		if ((1==emr_flag) && ((4==pstate) || (5==pstate)) && (10==state_emr)) { // TRANSICAO 10->12
			if (4==pstate) {
				time = TIME_SHORT; // start_timer(&t_short, curto); // se vier do verde liga t_short; se vier do amarelo nao
				tc1_init();
			}
			state_emr=12;
		}

		if ((12==state_emr) && (!time)) { // TRANSICAO 12->14
			time = TIME_EMR; // start_timer(&t_emr, emr);
			tc1_init();
			state_emr=14;
		}

		if ((14==state_emr) && (!time)) { // TRANSICAO 14->10
			state_emr=10;
			state=1;
			pstate=1;
			emr_flag=0; // transicao de saida do estado de emergencia
			time = TIME_LONG; // start_timer(&t_long, longo);
			tc1_init();
		}

		if ((1==emr_flag) && (6==pstate) && (10==state_emr)) { // TRANSICAO 10->14
			time = TIME_EMR; // start_timer(&t_emr, emr);
			tc1_init();
			state_emr=14;
		}
	}

	return 1;
}










