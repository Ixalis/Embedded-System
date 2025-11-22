/*
 * ex4_lab2.c
 *
 *  Created on: Oct 7, 2025
 *      Author: Admin
 */

#include "ex4_lab2.h"

#define TIMER_CYCLE 1

// LED 7-segment array
static const uint8_t arrayOfNum[10] = {0x03, 0x9f, 0x25, 0x0d, 0x99, 0x49, 0x41, 0x1f, 0x01, 0x09};
static uint16_t spi_buffer = 0xffff;
static uint8_t led7seg[4] = {0, 1, 2, 3};
static uint8_t led7_index = 0;

// Software timer variables
static uint16_t flag_timer = 0;
static uint16_t timer_counter = 0;
static uint16_t timer_MUL = 0;

// Clock variables
static uint8_t hours;
static uint8_t minutes;
static uint8_t seconds;
static uint8_t colon_state;

// Forward declarations
static void led7_SetDigit(int num, int position, uint8_t show_dot);
static void led7_SetColon(uint8_t status);

// Software timer functions
static void setTimer(uint16_t duration) {
	timer_MUL = duration / TIMER_CYCLE;
	timer_counter = timer_MUL;
	flag_timer = 0;
}

static void timer_run(void) {
	if (timer_counter > 0) {
		timer_counter--;
		if (timer_counter == 0) {
			flag_timer = 1;
			timer_counter = timer_MUL;
		}
	}
}

void ex4_lab2_init(void) {
	hours = 10;
	minutes = 11;
	seconds = 50;
	colon_state = 1;
	
	led7_SetDigit(0, 0, 0);
	led7_SetDigit(0, 1, 0);
	led7_SetDigit(0, 2, 0);
	led7_SetDigit(0, 3, 0);
	led7_SetColon(1);
	
	HAL_TIM_Base_Start_IT(&htim2);
	setTimer(500); // Colon chớp 2Hz
}

void ex4_lab2_scan(void) {
	spi_buffer &= 0x00ff;
	spi_buffer |= led7seg[led7_index] << 8;
	switch(led7_index){
	case 0:
		spi_buffer |= 0x00b0;
		spi_buffer &= 0xffbf;
		break;
	case 1:
		spi_buffer |= 0x00d0;
		spi_buffer &= 0xffdf;
		break;
	case 2:
		spi_buffer |= 0x00e0;
		spi_buffer &= 0xffef;
		break;
	case 3:
		spi_buffer |= 0x0070;
		spi_buffer &= 0xff7f;
		break;
	default:
		break;
	}
	led7_index = (led7_index + 1)%4;
	HAL_GPIO_WritePin(LD_LATCH_GPIO_Port, LD_LATCH_Pin, 0);
	HAL_SPI_Transmit(&hspi1, (void*)&spi_buffer, 2, 1);
	HAL_GPIO_WritePin(LD_LATCH_GPIO_Port, LD_LATCH_Pin, 1);
}

void ex4_lab2_run(void) {
	if (flag_timer) {
		flag_timer = 0;
		
		seconds++;
		
		if (seconds >= 60) {
			seconds = 0;
			minutes++;
			
			if (minutes >= 60) {
				minutes = 0;
				hours++;
				
				if (hours >= 24) {
					hours = 0;
				}
			}
		}
		
		//HH:MM
		led7_SetDigit(hours / 10, 0, 0);
		led7_SetDigit(hours % 10, 1, 0);
		led7_SetDigit(minutes / 10, 2, 0);
		led7_SetDigit(minutes % 10, 3, 0);
		
		// Colon chớp tắt 2Hz
		colon_state = !colon_state;
		led7_SetColon(colon_state);
		
		setTimer(500);
	}
}

static void led7_SetDigit(int num, int position, uint8_t show_dot){
	if(num >= 0 && num <= 9){
		led7seg[position] = arrayOfNum[num];
	}
}

static void led7_SetColon(uint8_t status){
	if(status == 1) spi_buffer &= ~(1 << 3);
	else spi_buffer |= (1 << 3);
}

void ex4_lab2_setDigit(uint8_t digit, uint8_t value) {
	led7_SetDigit(value, digit, 0);
}

void ex4_lab2_setColon(uint8_t state) {
	led7_SetColon(state);
}

void ex4_lab2_task(void) {
	timer_run();
	ex4_lab2_scan(); // Quét LED 7 đoạn
}
