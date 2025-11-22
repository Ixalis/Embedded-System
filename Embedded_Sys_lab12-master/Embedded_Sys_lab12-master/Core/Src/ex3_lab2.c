/*
 * ex3_lab2.c
 *
 *  Created on: Oct 7, 2025
 *      Author: Admin
 */

#include "ex3_lab2.h"

#define TIMER_CYCLE 1

// Software timer variables
static uint16_t flag_timer = 0;
static uint16_t timer_counter = 0;
static uint16_t timer_MUL = 0;

// LED state
static uint8_t led_state = 0;
static uint8_t frequency_mode = 0; // 0: 1Hz, 1: 25Hz, 2: 100Hz
static uint16_t mode_counter = 0;   // Đếm thời gian ở mỗi mode (đếm mỗi 1ms)

// Software timer functions
static void setTimer(uint16_t duration) {
	timer_MUL = duration / TIMER_CYCLE;
	timer_counter = timer_MUL;
	flag_timer = 0;
}

static void timer_run(void) {
	// Tăng mode_counter mỗi 1ms (gọi từ interrupt)
	mode_counter++;
	if (mode_counter >= 5000) {
		mode_counter = 0;
		frequency_mode = (frequency_mode + 1) % 3; // 0 → 1 → 2 → 0 ...
	}
	
	if (timer_counter > 0) {
		timer_counter--;
		if (timer_counter == 0) {
			flag_timer = 1;
			timer_counter = timer_MUL;
		}
	}
}

void ex3_lab2_init(void) {
	led_state = 0;
	frequency_mode = 0;
	mode_counter = 0;
	HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, GPIO_PIN_SET);
	
	HAL_TIM_Base_Start_IT(&htim2);
	setTimer(1000); // 1Hz (1000ms - chớp chậm)
}

void ex3_lab2_run(void) {
	if (flag_timer) {
		flag_timer = 0;
		
		// Toggle LED Y1
		led_state = !led_state;
		HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, led_state);
		
		switch(frequency_mode) {
		case 0: // 1Hz
			setTimer(1000);
			break;
		case 1: // 25Hz
			setTimer(40);
			break;
		case 2: // 100Hz
			setTimer(10);
			break;
		default:
			setTimer(1000);
			break;
		}
	}
}

void ex3_lab2_task(void) {
	timer_run();
}
