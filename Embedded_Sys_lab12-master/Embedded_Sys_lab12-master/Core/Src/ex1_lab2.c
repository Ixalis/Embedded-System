/*
 * ex1_lab2.c
 *
 *  Created on: Oct 7, 2025
 *      Author: Admin
 */

#include "ex1_lab2.h"

#define TIMER_CYCLE 1

// Software timer variables
static uint16_t flag_timer = 0;
static uint16_t timer_counter = 0;
static uint16_t timer_MUL = 0;

// LED variables
static uint8_t state_debug = 0;
static uint16_t counter_debug = 0;

static uint8_t state_y0 = 0;
static uint16_t counter_y0 = 0;

static uint8_t state_y1 = 0;
static uint16_t counter_y1 = 0;

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
void ex1_lab2_init(void) {
	state_debug = 0;
	counter_debug = 0;
	state_y0 = 0;
	counter_y0 = 0;
	state_y1 = 0;
	counter_y1 = 0;
	
	HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(OUTPUT_Y0_GPIO_Port, OUTPUT_Y0_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, GPIO_PIN_RESET);
	
	HAL_TIM_Base_Start_IT(&htim2);
	setTimer(1000); // 1s
}

void ex1_lab2_run(void) {
	if (flag_timer) {
		flag_timer = 0;
		
		// DEBUG_LED chớp tắt mỗi 2s
		counter_debug++;
		if (counter_debug >= 2) {
			counter_debug = 0;
			state_debug = !state_debug;
			HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, state_debug);
		}
		
		// OUTPUT_Y0 sáng 2s, tắt 4s
		counter_y0++;
		if (state_y0 == 0) {
			// Đang tắt 4s
			if (counter_y0 >= 4) {
				state_y0 = 1;
				counter_y0 = 0;
				HAL_GPIO_WritePin(OUTPUT_Y0_GPIO_Port, OUTPUT_Y0_Pin, GPIO_PIN_SET);
			}
		} else {
			// Đang sáng 2s
			if (counter_y0 >= 2) {
				state_y0 = 0;
				counter_y0 = 0;
				HAL_GPIO_WritePin(OUTPUT_Y0_GPIO_Port, OUTPUT_Y0_Pin, GPIO_PIN_RESET);
			}
		}
		
		// OUTPUT_Y1 sáng 5s, tắt 1s
		counter_y1++;
		if (state_y1 == 0) {
			// Đang tắt 1s
			if (counter_y1 >= 1) {
				state_y1 = 1;
				counter_y1 = 0;
				HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, GPIO_PIN_SET);
			}
		} else {
			// Đang sáng 5s
			if (counter_y1 >= 5) {
				state_y1 = 0;
				counter_y1 = 0;
				HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, GPIO_PIN_RESET);
			}
		}
		
		setTimer(1000);
	}
}

void ex1_lab2_task(void) {
	timer_run();
}
