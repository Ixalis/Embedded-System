/*
 * software_timer.c
 *
 *  Created on: Oct 30, 2023
 *      Author: HP
 */

#include"software_timer.h"

#define NUMBER_OF_TIMER	10

// Mảng flag để đánh dấu timer đã hết thời gian
uint16_t flag_timer[10] = {0};

/*
 * bief: state --> timer is on or off (1: on, 0: off)
 * */
struct {
	bool state;
	unsigned int count;
} timer[NUMBER_OF_TIMER];
/* timer[0]: to read sensor and button
 * timer[1]: to blink number
 * timer[2]: to increase value by 1 over time
 * timer[3]: to lcd show sensor time
 * timer[4]: to notify Potentiometer
 * timer[6]: to update graph
 * */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	run_timer();
}

/*
 * @brief:	turn timer on and set value
 * @para:	i: id of timer
 * 			time: unit is ms
 * @retval:	none
 * */
void set_timer(unsigned i, unsigned int time) {
	timer[i].count = time * FREQUENCY_OF_TIM / 1000.0;
	timer[i].state = 1;
	flag_timer[i] = 0; // Reset flag khi khởi động timer
}

/*
 * @brief:	run all timers that is on
 * @para:	none
 * @retval:	none
 * */
void run_timer(void) {
	for (unsigned i = 0; i < NUMBER_OF_TIMER; i++) {
		if (timer[i].state) {
			timer[i].count--;
			if (timer[i].count <= 0) {
				timer[i].state = 0;
				flag_timer[i] = 1; // Set flag khi timer hết thời gian
			}
		}
	}
}

bool is_timer_on(unsigned i) {
	return (timer[i].state == 1);
}
