/*
 * ex1_lab1.c
 *
 *  Created on: Oct 7, 2025
 *      Author: Admin
 */
#include "ex1_lab1.h"

void ex1_lab1_task(void) {
	HAL_GPIO_WritePin ( DEBUG_LED_GPIO_Port , DEBUG_LED_Pin, GPIO_PIN_SET);
    HAL_Delay(2000);
    HAL_GPIO_WritePin ( DEBUG_LED_GPIO_Port , DEBUG_LED_Pin, GPIO_PIN_RESET);
    HAL_Delay(4000);
}

