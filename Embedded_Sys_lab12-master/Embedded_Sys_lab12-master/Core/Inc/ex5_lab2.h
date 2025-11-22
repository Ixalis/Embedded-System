/*
 * ex5_lab2.h
 *
 *  Created on: Oct 7, 2025
 *      Author: Admin
 */

#ifndef INC_EX5_LAB2_H_
#define INC_EX5_LAB2_H_

#include "main.h"

//extern TIM_HandleTypeDef htim2;
//extern SPI_HandleTypeDef hspi1;

void ex5_lab2_init(void);
void ex5_lab2_scan(void);
void ex5_lab2_run(void);
void ex5_lab2_setDigit(uint8_t digit, uint8_t value);
void ex5_lab2_task(void);

#endif /* INC_EX5_LAB2_H_ */

