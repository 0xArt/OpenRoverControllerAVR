#include "FreeRTOS.h"
#include "task.h"
#include <avr/io.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/interrupt.h>

#define F_CPU   16000000UL
#define BAUD 9600
#define BRC ((F_CPU/16/BAUD) - 1)
#define  NUMBYTES 3
#define RX_BUFFER_SIZE 128

int8_t receivedBytes[NUMBYTES];
bool newData = false;
double dutyCycle = 0;
char rxBuffer[RX_BUFFER_SIZE];
uint8_t rxReadPos = 0;
uint8_t rxWritePos = 0;
uint8_t serialAvailable = 0;

char getChar(void);
void clearRxBuffer(void);
void setupDigitalPins(void);
void setupFastPwm(void);
void setupUART(void);
void sendByes(void);
void recvBytes(void);
void forward(int8_t speed);
void right(int8_t speed);
void left(int8_t speed);
void backward(int8_t speed);
void pause(void);
void useData(void);

void handleData(void* paramter){
	for(;;){
		sendBytes();
		vTaskDelay(250);
	}
}

void controlMotors(void* paramter){
	for(;;){
		recvBytes();
		useData();
		vTaskDelay(100);
	}
}

// MAIN PROGRAM
int main(void)
{
	setupDigitalPins();
	setupUART();
	setupFastPwm();
	xTaskCreate(controlMotors, "controlMotors", 128, NULL, 5, NULL);
	xTaskCreate(handleData, "handleData", 128, NULL, 5, NULL);

	// START SCHELUDER
	vTaskStartScheduler();
 
	return 0;
}

// IDLE TASK
void vApplicationIdleHook(void){
	// THIS RUNS WHILE NO OTHER TASK RUNS
}


ISR(TIMER0_OVF_vect)
{
	OCR0A = (dutyCycle/100.0)*255;
}


ISR(USART_RX_vect){
	rxBuffer[rxWritePos] = UDR0;
	rxWritePos++;
	if(rxWritePos >= RX_BUFFER_SIZE){
		rxWritePos = 0;
	}
	if(serialAvailable <= RX_BUFFER_SIZE){
		serialAvailable++;
	}
}

void setupDigitalPins(void){
	DDRB &= ~(0x01 << 0); // set Pin 14(Arduino Digital 8) as output 
	DDRD &= ~(0x01 << 7); // set Pin 13(Arduino Digital 7) as output
	PORTB &= ~(0x01 << 0); // set Pin 14 LOW
	PORTD &= ~(0x01 << 7); // set Pin 13 LOW
}

void setupFastPwm(void){
	DDRD = (1 << PORTD6);
	TCCR0A = (1 << COM0A1) | (1 << WGM00) | (1 << WGM01);
	TIMSK0 = (1 << TOIE0);
	OCR0A = (dutyCycle/100.0)*255.0; // Set duty cycle
	TCCR0B = (1 << CS01); //Fast PWM Frequency Scaling Factor
}

void setupUART(void){
	//set baud rate into upper and lower register
	UBRR0H = (BRC >> 8); //upper bits
	UBRR0L = BRC; //lower 8 bits
	//setup control register B
	UCSR0B = (1 << TXEN0) | (1 << RXEN0) | (1 << RXCIE0); //enable TX, enable RX, enable interrupt on RX complete
	//setup control register C
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); //setup 8 bit data frame
}

char getChar(void){
	char ret = '\0';
	if(rxReadPos!= rxWritePos){
		ret = rxBuffer[rxReadPos];
		rxReadPos++;
		serialAvailable--;
	}
	return ret;
}

void clearRxBuffer(void){
	rxWritePos = 0;
	rxReadPos = 0;
	serialAvailable = 0;
}

void sendBytes(void){
	int8_t zero = 255;
	int8_t one = (int8_t)(rand()%255);
	int8_t two = (int8_t)(rand()%255);
	int8_t three = one^two;
	int8_t toSend[] = {zero, one, two, three};
}

void forward(int8_t speed){
	dutyCycle = speed;
	PORTB &= ~(0x01 << 0);// set Pin 14 LOW
	PORTD &= ~(0x01 << 7); // set Pin 13 LOW
}

void right(int8_t speed){
	dutyCycle = speed;
	PORTB &= ~(0x01 << 0);// set Pin 14 LOW
	PORTD |= (0x01 << 7); // set Pin 13 HIGH
}

void left(int8_t speed){
	dutyCycle = speed;
	PORTB |= (0x01 << 0); // set Pin 14 HIGH
	PORTD &= ~(0x01 << 7); // set Pin 13 LOW
}

void backward(int8_t speed){
	dutyCycle = speed;
	PORTB |= (0x01 << 0); // set Pin 14 HIGH
	PORTD |= (0x01 << 7); // set Pin 13 HIGH
}

void pause(){
	dutyCycle = 0;
	PORTB &= ~(0x01 << 0);// set Pin 14 LOW
	PORTD |= (0x01 << 7); // set Pin 13 HIGH
}

void useData(void) {
	if (newData == true) {
		if( (receivedBytes[0]^receivedBytes[1]) == receivedBytes[2]){
			switch (receivedBytes[0]) {
				case 1:
				forward(receivedBytes[1]);
				break;
				case 2:
				right(receivedBytes[1]);
				break;
				case 3:
				backward(receivedBytes[1]);
				break;
				case 4:
				left(receivedBytes[1]);
				break;
				default:
				pause();
				break;
			}
		}
		else{
			pause();
		}
		newData = false;
	}
}

void recvBytes(void) {
	bool recvInProgress = false;
	uint8_t ndx = 0;
	char startMarker = 255;
	char rb;
	
	while (serialAvailable > 0 && newData == false) {
		rb = getChar();
		if (recvInProgress == true) {
			if (ndx < NUMBYTES) {
				receivedBytes[ndx] = rb;
				ndx++;
				if(ndx == NUMBYTES){
					recvInProgress = false;
					ndx = 0;
					newData = true;
				}
			}
		}
		if (rb == startMarker) {
			recvInProgress = true;
		}
	}
}

