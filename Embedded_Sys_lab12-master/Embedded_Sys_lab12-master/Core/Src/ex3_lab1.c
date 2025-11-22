/*
 * ex3_lab1.c
 *
 *  Created on: Oct 8, 2025
 *      Author: Admin
 */

#include "ex3_lab1.h"
#include "main.h"

void ex3_lab1_task(void) {
	static uint8_t trafficState = 0; // 0: đỏ, 1: xanh, 2: vàng
	static uint16_t counter = 0;

	switch (trafficState) {
	case 0: 
		HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(OUTPUT_Y0_GPIO_Port, OUTPUT_Y0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, GPIO_PIN_RESET);
		if (counter >= 5) { // thời gian đỏ 5s
			trafficState = 1;
			counter = 0;
		}
		break;
	case 1:
		HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(OUTPUT_Y0_GPIO_Port, OUTPUT_Y0_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, GPIO_PIN_RESET);
		if (counter >= 3) { // thời gian xanh 3s
			trafficState = 2;
			counter = 0;
		}
		break;
	case 2: 
		HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(OUTPUT_Y0_GPIO_Port, OUTPUT_Y0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, GPIO_PIN_SET);
		if (counter >= 1) { // thời gian vàng 1s
			trafficState = 0;
			counter = 0;
		}
		break;
	default:
		HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(OUTPUT_Y0_GPIO_Port, OUTPUT_Y0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, GPIO_PIN_RESET);
		trafficState = 0;
		counter = 0;
		break;
	}

	HAL_Delay(1000);
	counter += 1;
}


