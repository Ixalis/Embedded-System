/* USER CODE BEGIN Header */
/**
  * Project: Oven Control System (FINAL FIX)
  * Hardware: STM32F103R6
  * Pinout: Inputs PA0-PA2, LCD PB0-PB5, Out PC0-PC2
  */
/* USER CODE END Header */

#include "main.h"
#include <stdio.h>

/* --- HARDWARE DEFINITIONS --- */
#define LCD_PORT GPIOB
#define RS_PIN GPIO_PIN_0
#define EN_PIN GPIO_PIN_1
#define D4_PIN GPIO_PIN_2
#define D5_PIN GPIO_PIN_3
#define D6_PIN GPIO_PIN_4
#define D7_PIN GPIO_PIN_5

#define OUT_PORT GPIOC
#define HEATER_PIN GPIO_PIN_0
#define LAMP_PIN   GPIO_PIN_1
#define BUZZER_PIN GPIO_PIN_2

ADC_HandleTypeDef hadc1;

/* --- LCD FUNCTIONS --- */
void LCD_EnablePulse(void) {
    HAL_GPIO_WritePin(LCD_PORT, EN_PIN, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(LCD_PORT, EN_PIN, GPIO_PIN_RESET);
    HAL_Delay(1);
}

void LCD_Send4Bit(uint8_t Data) {
    HAL_GPIO_WritePin(LCD_PORT, D4_PIN, (Data >> 0) & 0x01);
    HAL_GPIO_WritePin(LCD_PORT, D5_PIN, (Data >> 1) & 0x01);
    HAL_GPIO_WritePin(LCD_PORT, D6_PIN, (Data >> 2) & 0x01);
    HAL_GPIO_WritePin(LCD_PORT, D7_PIN, (Data >> 3) & 0x01);
    LCD_EnablePulse();
}

void LCD_SendCommand(uint8_t cmd) {
    HAL_GPIO_WritePin(LCD_PORT, RS_PIN, GPIO_PIN_RESET);
    LCD_Send4Bit(cmd >> 4);
    LCD_Send4Bit(cmd & 0x0F);
}

void LCD_SendData(uint8_t data) {
    HAL_GPIO_WritePin(LCD_PORT, RS_PIN, GPIO_PIN_SET);
    LCD_Send4Bit(data >> 4);
    LCD_Send4Bit(data & 0x0F);
}

void LCD_Init(void) {
    HAL_Delay(50);
    LCD_SendCommand(0x02); // 4-bit mode
    LCD_SendCommand(0x28); // 2 Lines, 5x7
    LCD_SendCommand(0x0C); // Display ON
    LCD_SendCommand(0x01); // Clear
    HAL_Delay(2);
}

void LCD_SetCursor(uint8_t row, uint8_t col) {
    LCD_SendCommand((row == 0 ? 0x80 : 0xC0) + col);
}

void LCD_WriteString(char *str) {
    while (*str) LCD_SendData(*str++);
}

/* --- ADC HELPER --- */
uint16_t Read_ADC_Channel(uint32_t Channel) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = Channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 100);
    uint16_t val = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return val;
}

/* --- SYSTEM CONFIGURATION --- */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();

    /* =========================================================== */
    /* !!! CRITICAL FIX: UNLOCK PB3 & PB4 FOR LCD USE !!!          */
    /* =========================================================== */
    __HAL_RCC_AFIO_CLK_ENABLE();       // Enable Alternate Function Clock
    __HAL_AFIO_REMAP_SWJ_NOJTAG();     // DISCONNECT JTAG so PB3/PB4 work as GPIO!

    LCD_Init();
    LCD_SetCursor(0, 0);
    LCD_WriteString("SYSTEM BOOT...");
    HAL_Delay(1000);
    LCD_SendCommand(0x01); // Clear

    uint16_t raw_lm35, raw_temp, raw_time;
    int temp_act, temp_target, active;
    char buf[16];

    while (1) {
            // Read Inputs
            raw_lm35 = Read_ADC_Channel(ADC_CHANNEL_0);
            raw_temp = Read_ADC_Channel(ADC_CHANNEL_1);
            raw_time = Read_ADC_Channel(ADC_CHANNEL_2);

            // --- CALCULATION FIX ---
            // Formula for 5V Reference: (ADC_VAL * 5000mV / 4095) / 10mV per degree
            // Multiplier = 500 / 4095 = 0.1221
            temp_act = (int)(raw_lm35 * 0.1221);

            // Set Target Temp (0 to 250 C)
            temp_target = (raw_temp * 250) / 4095;

            // Timer/System Switch
            active = (raw_time > 200) ? 1 : 0;

            // --- CONTROL LOGIC ---
            if(active) {
                HAL_GPIO_WritePin(OUT_PORT, LAMP_PIN, GPIO_PIN_SET); // Lamp ON

                // Hysteresis (Prevents relay flickering)
                if(temp_act < temp_target) {
                    HAL_GPIO_WritePin(OUT_PORT, HEATER_PIN, GPIO_PIN_SET); // Heater ON
                }
                else if (temp_act > temp_target) { // Only turn off if strictly greater
                    HAL_GPIO_WritePin(OUT_PORT, HEATER_PIN, GPIO_PIN_RESET); // Heater OFF
                }
            } else {
                // System OFF
                HAL_GPIO_WritePin(OUT_PORT, LAMP_PIN, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(OUT_PORT, HEATER_PIN, GPIO_PIN_RESET);
            }

            // --- DISPLAY FIX (Add spaces to clear "Ghost" numbers) ---
            LCD_SetCursor(0, 0);
            // We use %3d to force the number to take up 3 spaces (e.g. " 92")
            sprintf(buf, "Temp:%3dC Set:%3dC", temp_act, temp_target);
            LCD_WriteString(buf);

            LCD_SetCursor(1, 0);
            if(active) {
                 LCD_WriteString("Status: ON      ");
            } else {
                 LCD_WriteString("Status: OFF     ");
            }

            HAL_Delay(200);
        }
}

/* --- CLOCK CONFIGURATION (Forced to Internal HSI for Stability) --- */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    // Use Internal 8MHz Clock (Robust for Proteus)
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
}

static void MX_ADC1_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};
    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    HAL_ADC_Init(&hadc1);
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Reset Outputs */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_RESET);

    /* LCD Pins (PB0-PB5) */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Control Output Pins (PC0-PC2) */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}
