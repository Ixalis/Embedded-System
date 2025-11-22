/*
 * ex5_lab2.c
 *
 *  Created on: Oct 7, 2025
 *      Author: Admin
 */

#include "ex5_lab2.h"

#define TIMER_CYCLE 1

// LED 7-segment array (theo setup của bạn)
static const uint8_t arrayOfNum[10] = {0x03, 0x9f, 0x25, 0x0d, 0x99, 0x49, 0x41, 0x1f, 0x01, 0x09};

// LED 7-segment variables
static uint16_t spi_buffer = 0xffff;
static uint8_t led7seg[4] = {0, 1, 2, 3};
static uint8_t led7_index = 0;

// Software timer variables
static uint16_t flag_timer = 0;
static uint16_t timer_counter = 0;
static uint16_t timer_MUL = 0;

// Number shift variables
static uint8_t numbers[4] = {1, 2, 3, 4};

// Forward declarations
static void led7_SetDigit(int num, int position, uint8_t show_dot);

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

void ex5_lab2_init(void) {
	numbers[0] = 1;
	numbers[1] = 2;
	numbers[2] = 3;
	numbers[3] = 4;
	
	for (uint8_t i = 0; i < 4; i++) {
		led7_SetDigit(numbers[i], i, 0);
	}
	
	HAL_TIM_Base_Start_IT(&htim2);
	setTimer(1000); // Dịch mỗi 1s
}

void ex5_lab2_scan(void) {
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

void ex5_lab2_run(void) {
	if (flag_timer) {
		flag_timer = 0;
		
		// Lưu số cuối cùng
		uint8_t last = numbers[3];
		
		// Dịch tất cả các số sang phải
		for (int8_t i = 3; i > 0; i--) {
			numbers[i] = numbers[i - 1];
		}
		
		// Số cuối chuyển lên đầu (hiệu ứng vòng tròn)
		numbers[0] = last;
		
		// Cập nhật hiển thị
		for (uint8_t i = 0; i < 4; i++) {
			led7_SetDigit(numbers[i], i, 0);
		}
		
		setTimer(1000);
	}
}

static void led7_SetDigit(int num, int position, uint8_t show_dot){
	if(num >= 0 && num <= 9){
		led7seg[position] = arrayOfNum[num];
	}
}

void ex5_lab2_setDigit(uint8_t digit, uint8_t value) {
	if (digit < 4 && value < 10) {
		numbers[digit] = value;
		led7_SetDigit(value, digit, 0);
	}
}

void ex5_lab2_task(void) {
	timer_run();
	ex5_lab2_scan(); // Quét LED 7 đoạn
}
