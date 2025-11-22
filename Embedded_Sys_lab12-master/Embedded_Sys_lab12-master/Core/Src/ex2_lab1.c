/*
 * ex2_lab1.c
 *
 *  Created on: Oct 7, 2025
 *      Author: Admin
 */
#include "ex2_lab1.h"

static uint8_t ledState = 0;
static uint16_t counterMs = 0;

void ex2_lab1_init(void) {
    ledState = 0;
    counterMs = 0;
    HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_SET);
}

void ex2_lab1_task(void) {
    counterMs++;
    
    if (ledState == 0) {
        if (counterMs >= 2) {
            HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_RESET);
            ledState = 1;
            counterMs = 0;
        }
    } else { // LED OFF
        if (counterMs >= 4) {
            HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_SET);
            ledState = 0;
            counterMs = 0;
        }
    }
}

