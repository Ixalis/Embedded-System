/*
 * ex4_lab2.h
 *
 *  Created on: Oct 7, 2025
 *      Author: Admin
 */

#ifndef INC_EX4_LAB2_H_
#define INC_EX4_LAB2_H_

#include "main.h"

//extern TIM_HandleTypeDef htim2;
//extern SPI_HandleTypeDef hspi1;

void ex4_lab2_init(void);
void ex4_lab2_scan(void);
void ex4_lab2_run(void);
void ex4_lab2_setDigit(uint8_t digit, uint8_t value);
void ex4_lab2_setColon(uint8_t state);
void ex4_lab2_task(void);

#endif /* INC_EX4_LAB2_H_ */

