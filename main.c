#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include "fsl_device_registers.h"
void shortDelay(){delay();}
void mediumDelay() {delay(); delay();}

typedef struct color_note{
	int color;			//color of current color note
	struct color_note *next_note;//color of next color note
} note_t;

note_t *sequence = NULL;
note_t *current_note = NULL;
int millisec = 0;
int first = 1;
int redPressed = 0;
int bluePressed = 0;

int add(int new_color) {				//add a color to linked list
	note_t *new_note = malloc(sizeof(struct color_note));
	if (new_note == NULL) return -1;
	new_note->color = new_color;
	if (sequence == NULL) {				//added to the head of the sequence if the sequence is empty
		new_note->next_note = sequence;
		sequence = new_note;
	} else {							//loop through the sequence and be added in the end
		note_t *tmp = sequence;
		while (tmp->next_note != NULL) {
			tmp = tmp->next_note;
		}
		tmp->next_note = new_note;
		new_note->next_note = NULL;
	}
	return 0;
}

void perform(void) {	//display sequence
	note_t *tmp = sequence;
	while (tmp != NULL) {	//while color sequence has not reached the end
		if (tmp->color == 0) {	//color is red
			LEDRed_On();
			mediumDelay();
			LEDRed_Toggle();
		} else if (tmp->color == 1) {	//color is green
			LEDGreen_On();
			mediumDelay();
			LEDGreen_Toggle();
		} else {					//color is blue
			LEDBlue_On();
			mediumDelay();
			LEDBlue_Toggle();
		}
		tmp = tmp->next_note;		//go to next color
		LED_Off();
		shortDelay();
	}
}

int main(void) {	
	SIM->SCGC6 = SIM_SCGC6_PIT_MASK; 	
								//enable the clock to the PIT module
	PIT->MCR = 0;				//For debuggin purposes
	PIT->CHANNEL[1].LDVAL = DEFAULT_SYSTEM_CLOCK/1000; //load 1 millisecond
	NVIC_EnableIRQ(PIT1_IRQn);	
	PIT->CHANNEL[1].TCTRL = 3;
	NVIC_SetPriority(SVCall_IRQn, 0);
	NVIC_SetPriority(PIT1_IRQn, 0); 
	
	SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK;	
	SIM_SCGC5 |= SIM_SCGC5_PORTC_MASK;
	SIM_SCGC5 |= SIM_SCGC5_PORTB_MASK;
	
	PORTC->PCR[6] = (1<<8);	//Set up PTC6 as GPIO
	PORTA->PCR[4] = (1<<8);	//Set up PTA4 as GPIO

	PORTC_PCR6 = 0x90100;
	PORTA_PCR4 = 0x90100;
	GPIOC_PDDR |= (0<<6);
	GPIOA_PDDR |= (0<<4);

	PORTC_ISFR = PORT_ISFR_ISF(0x40);
	PORTA_ISFR = PORT_ISFR_ISF(0x10);
	NVIC_EnableIRQ(PORTC_IRQn);
	NVIC_EnableIRQ(PORTA_IRQn);
	
	
	sequence = NULL;
	LED_Initialize();
	first = 1;			// first interrupt has not been executed
	LEDGreen_Toggle();	//white LED on
	LEDBlue_Toggle();
	LEDRed_Toggle();
	while (first) {}	//wait for player to press either SW2 or SW3 to begin

  while (1) {
		srand(millisec); 
		int new_color = (int)(rand() % 3);	//generate a new random color
		add(new_color);						//add new color to the sequence
		perform();							//display the new sequence of color
		current_note = sequence;			//reset user's turn
		while (current_note != NULL) {		//wait for user to input the entire sequence correctly
			__enable_irq();
			__disable_irq();
		}									//repeat process
		mediumDelay();
	}
	while (1);
	return 0;
}
void PIT1_IRQHandler(void) { //Each time the timer interrupts, millisec is incremented by 1
	if (millisec > 1000) {
		shortDelay();
		millisec = 0;
	} else {
		millisec += 1; 
	}
	PIT->CHANNEL[1].TFLG = 1;
}


void inputGreen(void) {
	if (current_note->color != 1) { //losing
				LEDGreen_Toggle();
				LEDBlue_Toggle();
				LEDRed_Toggle();
			} else { //correct
				LEDGreen_On();
				mediumDelay();
				LEDGreen_Toggle();
				LED_Off();
				current_note = current_note->next_note;
			}
}

void PORTC_IRQHandler(void) { //user inputs blue
	if (first) {
		first = 0;
	} else {
		bluePressed = 1;
		if (redPressed) { //red is pressed before blue
			inputGreen();
		} else { //red has not been press, wait
			NVIC_SetPriority(PORTC_IRQn, 1);	//update priority of portA to be higher than portC
			NVIC_SetPriority(PORTA_IRQn, 0);
			__enable_irq();
			shortDelay();
			__disable_irq();
			if (redPressed) { //red is pressed after blue
				bluePressed = 0;
				redPressed = 0;
			} else if (current_note->color != 2) { //red is not pressed and color is not blue
				LEDGreen_Toggle();
				LEDBlue_Toggle();
				LEDRed_Toggle();
			} else if (current_note->color == 2){ //correct
				LEDBlue_On();
				mediumDelay();
				LEDBlue_Toggle();
				LED_Off();
				current_note = current_note->next_note;
			}
			bluePressed = 0;
			redPressed = 0;
		}
		
	}
	PORTC_ISFR = PORT_ISFR_ISF(0x40);
}



void PORTA_IRQHandler(void) { //user inputs red
	if (first) {
		first = 0;
	} else {
		redPressed = 1;
		if (bluePressed) { //red is pressed after blue
			inputGreen();
		} else { //red is pressed first
			NVIC_SetPriority(PORTC_IRQn, 0);	//update priority of portC to be higher than portA
			NVIC_SetPriority(PORTA_IRQn, 1);
			__enable_irq();
			shortDelay();
			__disable_irq();
			if (bluePressed) { //blue is pressed after red
				bluePressed = 0;
				redPressed = 0;
			} else if (current_note->color != 0) { //losing
				LEDGreen_Toggle();
				LEDBlue_Toggle();
				LEDRed_Toggle();
			} else if(current_note->color == 0){ //correct
				LEDRed_On();
				mediumDelay();
				LEDRed_Toggle();
				LED_Off();
				current_note = current_note->next_note;
			}
			redPressed = 0;
			bluePressed = 0;
		}

	}
	PORTA_ISFR = PORT_ISFR_ISF(0x10);
}


