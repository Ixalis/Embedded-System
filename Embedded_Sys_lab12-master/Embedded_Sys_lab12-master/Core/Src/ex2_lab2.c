/*
 * ex2_lab2.c
 *
 *  Created on: Oct 7, 2025
 *      Author: Admin
 */

#include "ex2_lab2.h"

#define TIMER_CYCLE 1

// Software timer variables
static uint16_t flag_timer = 0;
static uint16_t timer_counter = 0;
static uint16_t timer_MUL = 0;

// Traffic light variables
static uint8_t trafficState = 0; // 0: đỏ, 1: xanh, 2: vàng
static uint16_t counterMs = 0;

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

// Main functions
void ex2_lab2_init(void) {
	trafficState = 0;
	counterMs = 0;
	
	HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(OUTPUT_Y0_GPIO_Port, OUTPUT_Y0_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, GPIO_PIN_RESET);
	
	HAL_TIM_Base_Start_IT(&htim2);
	setTimer(1000);
}

void ex2_lab2_run(void) {
	if (flag_timer) {
		flag_timer = 0;
		counterMs += 1;
		
		switch (trafficState) {
		case 0: // Đèn đỏ (DEBUG_LED)
			HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_SET);
			HAL_GPIO_WritePin(OUTPUT_Y0_GPIO_Port, OUTPUT_Y0_Pin, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, GPIO_PIN_RESET);
			if (counterMs >= 5) { // 5s
				trafficState = 1;
				counterMs = 0;
			}
			break;
			
		case 1: // Đèn xanh (OUTPUT_Y0)
			HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(OUTPUT_Y0_GPIO_Port, OUTPUT_Y0_Pin, GPIO_PIN_SET);
			HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, GPIO_PIN_RESET);
			if (counterMs >= 3) { // 3s
				trafficState = 2;
				counterMs = 0;
			}
			break;
			
		case 2: // Đèn vàng (OUTPUT_Y1)
			HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(OUTPUT_Y0_GPIO_Port, OUTPUT_Y0_Pin, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, GPIO_PIN_SET);
			if (counterMs >= 1) { // 1s
				trafficState = 0;
				counterMs = 0;
			}
			break;
			
		default:
			HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(OUTPUT_Y0_GPIO_Port, OUTPUT_Y0_Pin, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, GPIO_PIN_RESET);
			trafficState = 0;
			counterMs = 0;
			break;
		}
		
		setTimer(1000);
	}
}

void ex2_lab2_task(void) {
	timer_run();
}
