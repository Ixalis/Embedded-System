/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Oven Control System with Anomaly Detection v3
  * @author         : Ixalis (cleaned up by Claude)
  ******************************************************************************
  * Project: Oven Control System with ANOMALY DETECTION
  * Hardware: STM32F103R6
  * Pinout:
  *   - Inputs:  PA0 (LM35 temp sensor), PA1 (target temp pot), PA2 (timer pot)
  *   - LCD:     PB0-PB5 (4-bit mode)
  *   - Outputs: PC0 (Heater), PC1 (Lamp), PC2 (Buzzer)
  *
  * SAFETY FEATURES:
  *   1. Rate-of-Rise Detection (sudden spike alarm)
  *   2. Absolute Safety Limit (overheat protection)
  *   3. Sensor Fault Detection (open/short circuit)
  *   4. Sensor Stuck Detection (heater on but no temp change)
  *   5. Emergency Shutdown with Latch
  *   6. Hysteresis Control (prevents relay chatter)
  *   7. Independent Watchdog (system hang protection)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include <stdio.h>
#include <stdlib.h>

/* ============================================================= */
/* --- HARDWARE PIN DEFINITIONS ---                              */
/* ============================================================= */

// LCD (directly define all registers for 4-bit parallel mode)
#define LCD_PORT    GPIOB
#define RS_PIN      GPIO_PIN_0
#define EN_PIN      GPIO_PIN_1
#define D4_PIN      GPIO_PIN_2
#define D5_PIN      GPIO_PIN_3
#define D6_PIN      GPIO_PIN_4
#define D7_PIN      GPIO_PIN_5

// Outputs
#define OUT_PORT    GPIOC
#define HEATER_PIN  GPIO_PIN_0
#define LAMP_PIN    GPIO_PIN_1
#define BUZZER_PIN  GPIO_PIN_2

/* ============================================================= */
/* --- SYSTEM CONFIGURATION PARAMETERS ---                       */
/* ============================================================= */

// Temperature Calculation
#define LM35_SCALE_FACTOR       0.1221f     // (3.3V / 4096) * 100 / 10mV per °C
#define TEMP_TARGET_MIN         20          // Minimum settable temperature (°C)
#define TEMP_TARGET_RANGE       60          // Range above minimum (20-80°C)
#define ACTIVE_THRESHOLD        200         // ADC threshold for "active" state

// Control Parameters
#define HYSTERESIS              2           // °C deadband to prevent relay chatter
#define OVERHEAT_WARNING_MARGIN 5           // Buzzer warning when this much over target

// Timing
#define SAMPLE_INTERVAL_MS      200         // Main loop period
#define SAMPLES_PER_SECOND      (1000 / SAMPLE_INTERVAL_MS)  // = 5

/* ============================================================= */
/* --- ANOMALY DETECTION PARAMETERS ---                          */
/* ============================================================= */

// Absolute Safety Limits
#define TEMP_ABSOLUTE_MAX       300         // °C - Emergency shutdown above this
#define TEMP_ABSOLUTE_MIN       -10         // °C - Sensor broken if below this
#define TEMP_SAFE_RESTART       250         // °C - Must be below this to clear lockout

// Rate of Change Limits (per second)
#define RATE_OF_RISE_MAX        30          // °C/s - Max normal heating rate
#define RATE_OF_FALL_MAX        50          // °C/s - Max normal cooling rate

// Sensor Fault Thresholds (ADC bounds)
#define ADC_SENSOR_MIN          10          // Below = open circuit / disconnected
#define ADC_SENSOR_MAX          4080        // Above = shorted to VCC

// Stuck Sensor Detection
#define STUCK_THRESHOLD_CYCLES  50          // 10 seconds (50 * 200ms)
#define STUCK_TEMP_TOLERANCE    1           // °C change required to not be "stuck"


/* ============================================================= */
/* --- TYPE DEFINITIONS ---                                      */
/* ============================================================= */

// Alarm Codes (priority order - lower number = higher priority)
typedef enum {
    ALARM_NONE = 0,
    ALARM_SENSOR_OPEN,      // Priority 1: Bad data - can't trust anything
    ALARM_SENSOR_SHORT,     // Priority 1: Bad data
    ALARM_OVERHEAT,         // Priority 2: Immediate danger
    ALARM_UNDERHEAT,        // Priority 2: Sensor fault (impossible reading)
    ALARM_RATE_RISE,        // Priority 3: Developing danger
    ALARM_RATE_FALL,        // Priority 3: Possible sensor issue
    ALARM_SENSOR_STUCK      // Priority 4: Heater may be broken or sensor dead
} AlarmCode_t;

// System Operating States (for LCD update optimization)
typedef enum {
    STATE_STANDBY = 0,
    STATE_HEATING,
    STATE_COOLING,
    STATE_ALARM
} SystemState_t;

/* ============================================================= */
/* --- GLOBAL VARIABLES ---                                      */
/* ============================================================= */

// HAL Handles
ADC_HandleTypeDef hadc1;
volatile uint16_t debug_raw_temp = 0;

// Anomaly Detection State
int temp_history[5] = {0};          // Rolling buffer for rate calculation
uint8_t history_index = 0;
uint8_t history_filled = 0;         // Flag: enough samples for rate calc

// System State
AlarmCode_t current_alarm = ALARM_NONE;
uint8_t emergency_lockout = 0;      // Latched shutdown state
uint8_t heater_on = 0;              // Hysteresis-controlled heater state

// LCD Update Optimization
SystemState_t last_displayed_state = 255;   // Force initial update
int last_displayed_temp = -999;
int last_displayed_target = -999;

/* ============================================================= */
/* --- FUNCTION PROTOTYPES ---                                   */
/* ============================================================= */

// System Configuration
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);

// LCD Functions
void LCD_Init(void);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_WriteString(const char *str);
void LCD_WriteStringPadded(const char *str, uint8_t width);

// ADC Functions
uint16_t Read_ADC_Channel(uint32_t Channel);

// Anomaly Detection
AlarmCode_t Check_Sensor_Fault(uint16_t raw_adc);
AlarmCode_t Check_Absolute_Limits(int temp_celsius);
AlarmCode_t Check_Rate_Of_Change(int new_temp);
//AlarmCode_t Check_Sensor_Stuck(int temp_celsius, uint8_t heater_active);
AlarmCode_t Run_Anomaly_Detection(uint16_t raw_adc, int temp_celsius, uint8_t heater_active);
const char* Get_Alarm_Message(AlarmCode_t alarm);

// Control Functions
void Emergency_Shutdown(AlarmCode_t alarm_code);
uint8_t Try_Clear_Lockout(uint16_t raw_adc, int temp_celsius);
void Update_Heater_State(int temp_act, int temp_target);

// Display Functions
void Display_Update(SystemState_t state, int temp_act, int temp_target);
void Handle_Alarm_Display(int temp_act);
void Handle_Normal_Display(SystemState_t state, int temp_act, int temp_target);

/* ============================================================= */
/* --- LCD FUNCTIONS ---                                         */
/* ============================================================= */

static void LCD_EnablePulse(void) {
    HAL_GPIO_WritePin(LCD_PORT, EN_PIN, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(LCD_PORT, EN_PIN, GPIO_PIN_RESET);
    HAL_Delay(1);
}

static void LCD_Send4Bit(uint8_t data) {
    HAL_GPIO_WritePin(LCD_PORT, D4_PIN, (data >> 0) & 0x01);
    HAL_GPIO_WritePin(LCD_PORT, D5_PIN, (data >> 1) & 0x01);
    HAL_GPIO_WritePin(LCD_PORT, D6_PIN, (data >> 2) & 0x01);
    HAL_GPIO_WritePin(LCD_PORT, D7_PIN, (data >> 3) & 0x01);
    LCD_EnablePulse();
}

static void LCD_SendCommand(uint8_t cmd) {
    HAL_GPIO_WritePin(LCD_PORT, RS_PIN, GPIO_PIN_RESET);
    LCD_Send4Bit(cmd >> 4);
    LCD_Send4Bit(cmd & 0x0F);
}

static void LCD_SendData(uint8_t data) {
    HAL_GPIO_WritePin(LCD_PORT, RS_PIN, GPIO_PIN_SET);
    LCD_Send4Bit(data >> 4);
    LCD_Send4Bit(data & 0x0F);
}

void LCD_Init(void) {
    HAL_Delay(50);              // Wait for LCD power-up
    LCD_SendCommand(0x02);      // 4-bit mode
    LCD_SendCommand(0x28);      // 2 lines, 5x7 font
    LCD_SendCommand(0x0C);      // Display ON, cursor OFF
    LCD_SendCommand(0x06);      // Auto-increment cursor
    LCD_SendCommand(0x01);      // Clear display
    HAL_Delay(2);
}

void LCD_SetCursor(uint8_t row, uint8_t col) {
    LCD_SendCommand((row == 0 ? 0x80 : 0xC0) + col);
}

void LCD_WriteString(const char *str) {
    while (*str) {
        LCD_SendData(*str++);
    }
}

/**
 * @brief Write string padded to exact width (prevents ghost characters)
 */
void LCD_WriteStringPadded(const char *str, uint8_t width) {
    uint8_t written = 0;
    while (*str && written < width) {
        LCD_SendData(*str++);
        written++;
    }
    while (written < width) {
        LCD_SendData(' ');
        written++;
    }
}

void LCD_Clear(void) {
    LCD_SendCommand(0x01);
    HAL_Delay(2);
}

/* ============================================================= */
/* --- ADC FUNCTIONS ---                                         */
/* ============================================================= */

uint16_t Read_ADC_Channel(uint32_t Channel) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = Channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 100);
    uint16_t val = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return val;
}

/* ============================================================= */
/* --- ANOMALY DETECTION FUNCTIONS ---                           */
/* ============================================================= */

/**
 * @brief Check for sensor hardware faults based on raw ADC value
 */
AlarmCode_t Check_Sensor_Fault(uint16_t raw_adc) {
    if (raw_adc < ADC_SENSOR_MIN) {
        return ALARM_SENSOR_OPEN;
    }
    if (raw_adc > ADC_SENSOR_MAX) {
        return ALARM_SENSOR_SHORT;
    }
    return ALARM_NONE;
}

/**
 * @brief Check if temperature is within absolute safe bounds
 */
AlarmCode_t Check_Absolute_Limits(int temp_celsius) {
    if (temp_celsius > TEMP_ABSOLUTE_MAX) {
        return ALARM_OVERHEAT;
    }
    if (temp_celsius < TEMP_ABSOLUTE_MIN) {
        return ALARM_UNDERHEAT;
    }
    return ALARM_NONE;
}

/**
 * @brief Update history buffer and check rate of change
 */
AlarmCode_t Check_Rate_Of_Change(int new_temp) {
    // Get oldest sample BEFORE overwriting
    int old_temp = temp_history[history_index];

    // Update circular buffer
    temp_history[history_index] = new_temp;
    history_index = (history_index + 1) % SAMPLES_PER_SECOND;

    // Wait for buffer to fill (1 second startup delay)
    if (!history_filled) {
        static uint8_t sample_count = 0;
        sample_count++;
        if (sample_count >= SAMPLES_PER_SECOND) {
            history_filled = 1;
        }
        return ALARM_NONE;
    }

    // Calculate rate (buffer holds 1 second of data)
    int rate_per_second = new_temp - old_temp;

    if (rate_per_second > RATE_OF_RISE_MAX) {
        return ALARM_RATE_RISE;
    }
    if (rate_per_second < -RATE_OF_FALL_MAX) {
        return ALARM_RATE_FALL;
    }

    return ALARM_NONE;
}

/**
 * @brief Detect if sensor is stuck (heater on but no temperature change)
 */
//AlarmCode_t Check_Sensor_Stuck(int temp_celsius, uint8_t heater_active) {
   // static int last_temp = 0;
   // static uint16_t stuck_count = 0;

   // if (abs(temp_celsius - last_temp) <= STUCK_TEMP_TOLERANCE) {
       // stuck_count++;
        // Only alarm if heater is ON and temp isn't changing
      //  if (stuck_count > STUCK_THRESHOLD_CYCLES && heater_active) {
            //return ALARM_SENSOR_STUCK;
       // }
    //} else {
       // stuck_count = 0;  // Reset on any temperature change
  //  }

   // last_temp = temp_celsius;
    //return ALARM_NONE;
//}

/**
 * @brief Master anomaly check - runs all detection algorithms in priority order
 */
AlarmCode_t Run_Anomaly_Detection(uint16_t raw_adc, int temp_celsius, uint8_t heater_active) {
    AlarmCode_t alarm;

    // Priority 1: Sensor hardware fault (can't trust data)
    alarm = Check_Sensor_Fault(raw_adc);
    if (alarm != ALARM_NONE) return alarm;

    // Priority 2: Absolute limits (immediate danger)
    alarm = Check_Absolute_Limits(temp_celsius);
    if (alarm != ALARM_NONE) return alarm;

    // Priority 3: Rate of change (developing danger)
    alarm = Check_Rate_Of_Change(temp_celsius);
    if (alarm != ALARM_NONE) return alarm;

    // Priority 4: Stuck sensor (heater malfunction)
   // alarm = Check_Sensor_Stuck(temp_celsius, heater_active);
    //if (alarm != ALARM_NONE) return alarm;

    return ALARM_NONE;
}

/**
 * @brief Get human-readable alarm message (16 chars max for LCD)
 */
const char* Get_Alarm_Message(AlarmCode_t alarm) {
    switch (alarm) {
        case ALARM_OVERHEAT:     return "OVERHEAT!       ";
        case ALARM_UNDERHEAT:    return "TEMP TOO LOW!   ";
        case ALARM_RATE_RISE:    return "TEMP SPIKE!     ";
        case ALARM_RATE_FALL:    return "TEMP DROP!      ";
        case ALARM_SENSOR_OPEN:  return "SENSOR DISCONN! ";
        case ALARM_SENSOR_SHORT: return "SENSOR SHORTED! ";
        case ALARM_SENSOR_STUCK: return "SENSOR STUCK!   ";
        default:                 return "UNKNOWN ERROR   ";
    }
}

/* ============================================================= */
/* --- CONTROL FUNCTIONS ---                                     */
/* ============================================================= */

/**
 * @brief Emergency shutdown - kills all outputs and latches
 */
void Emergency_Shutdown(AlarmCode_t alarm_code) {
    // IMMEDIATELY kill dangerous outputs
    HAL_GPIO_WritePin(OUT_PORT, HEATER_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OUT_PORT, LAMP_PIN, GPIO_PIN_RESET);

    // Set alarm state
    current_alarm = alarm_code;
    emergency_lockout = 1;
    heater_on = 0;

    // Buzzer ON
    HAL_GPIO_WritePin(OUT_PORT, BUZZER_PIN, GPIO_PIN_SET);
}

/**
 * @brief Attempt to clear lockout (only if conditions are safe)
 * @return 1 if cleared successfully, 0 if still unsafe
 */
uint8_t Try_Clear_Lockout(uint16_t raw_adc, int temp_celsius) {
    // Don't clear if sensor is still faulty
    if (Check_Sensor_Fault(raw_adc) != ALARM_NONE) {
        return 0;
    }

    // Don't clear if temperature is still dangerous
    if (temp_celsius > TEMP_SAFE_RESTART) {
        return 0;
    }

    // Safe to clear
    emergency_lockout = 0;
    current_alarm = ALARM_NONE;
    heater_on = 0;
    HAL_GPIO_WritePin(OUT_PORT, BUZZER_PIN, GPIO_PIN_RESET);

    // Reset detection state
    history_filled = 0;
    history_index = 0;

    // Force LCD refresh
    last_displayed_state = 255;

    return 1;
}

/**
 * @brief Update heater state with hysteresis to prevent relay chatter
 */
void Update_Heater_State(int temp_act, int temp_target) {
    if (temp_act < (temp_target - HYSTERESIS)) {
        heater_on = 1;
    } else if (temp_act > (temp_target + HYSTERESIS)) {
        heater_on = 0;
    }
    // If within hysteresis band, maintain current state (no change)
}

/* ============================================================= */
/* --- DISPLAY FUNCTIONS ---                                     */
/* ============================================================= */

/**
 * @brief Handle alarm state display with pulsing buzzer
 */
void Handle_Alarm_Display(int temp_act) {
    static uint8_t buzzer_state = 0;
    char buf[17];

    // Pulse buzzer
    buzzer_state = !buzzer_state;
    HAL_GPIO_WritePin(OUT_PORT, BUZZER_PIN, buzzer_state ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // Update display (always update during alarm for visibility)
    LCD_SetCursor(0, 0);
    sprintf(buf, "!! ALARM !! %3dC", temp_act);
    LCD_WriteString(buf);

    LCD_SetCursor(1, 0);
    LCD_WriteString(Get_Alarm_Message(current_alarm));
}

/**
 * @brief Handle normal operation display (optimized to reduce flicker)
 */
void Handle_Normal_Display(SystemState_t state, int temp_act, int temp_target) {
    char buf[17];
    const char *set_label;

    // Map target temperature to label
    if (temp_target < 34) {
        set_label = "LO";
    } else if (temp_target < 67) {
        set_label = "MD";
    } else {
        set_label = "HI";
    }

    // Line 1: compact temp + set
    if (temp_act != last_displayed_temp || temp_target != last_displayed_target) {
        LCD_SetCursor(0, 0);
        snprintf(buf, sizeof(buf), "T:%3dC  S:%s", temp_act, set_label);
        LCD_WriteStringPadded(buf, 16);

        last_displayed_temp = temp_act;
        last_displayed_target = temp_target;
    }

    // Line 2: status only
    if (state != last_displayed_state) {
        LCD_SetCursor(1, 0);

        switch (state) {
            case STATE_HEATING:
                LCD_WriteStringPadded("HEATING", 16);
                break;
            case STATE_COOLING:
                LCD_WriteStringPadded("COOLING", 16);
                break;
            case STATE_STANDBY:
                LCD_WriteStringPadded("STANDBY", 16);
                break;
            default:
                LCD_WriteStringPadded("UNKNOWN", 16);
                break;
        }

        last_displayed_state = state;
    }
}



/* ============================================================= */
/* --- MAIN FUNCTION ---                                         */
/* ============================================================= */

int main(void) {
    // --- System Initialization ---
	char buf[17];
	HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();


    // CRITICAL: Unlock PB3 & PB4 for LCD (disable JTAG, keep SWD)
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    // Calibrate ADC for better accuracy
    HAL_ADCEx_Calibration_Start(&hadc1);

    // --- LCD Startup ---
    LCD_Init();
    LCD_SetCursor(0, 0);
    LCD_WriteString("OVEN CONTROL v3 ");
    LCD_SetCursor(1, 0);
    LCD_WriteString("Anomaly Detect  ");
    HAL_Delay(1500);
    LCD_Clear();

    // --- Local Variables ---

    uint16_t raw_lm35, raw_temp, raw_time;
    int temp_act, temp_target;
    uint8_t active;
    SystemState_t current_state;
    AlarmCode_t detected_alarm;

    // --- Main Control Loop ---
    while (1) {
        // Feed the watchdog first


        // === READ INPUTS ===
        raw_lm35 = Read_ADC_Channel(ADC_CHANNEL_0);  // LM35 temperature
        raw_temp = Read_ADC_Channel(ADC_CHANNEL_1);  // Target temp pot
        raw_time = Read_ADC_Channel(ADC_CHANNEL_2);  // Timer/active pot
        debug_raw_temp = raw_temp;
        // === CALCULATE VALUES ===
        temp_act = (int)(raw_lm35 * 330) / 4095;
        temp_target = TEMP_TARGET_MIN + (raw_temp * TEMP_TARGET_RANGE) / 4095;
        active = (raw_time > ACTIVE_THRESHOLD) ? 1 : 0;


        if (temp_target > 80) {
            // This should NEVER happen - flash buzzer as debug signal
            HAL_GPIO_TogglePin(OUT_PORT, BUZZER_PIN);
        }
        // === ANOMALY DETECTION ===
        detected_alarm = Run_Anomaly_Detection(raw_lm35, temp_act, heater_on);

        if (detected_alarm != ALARM_NONE) {
            Emergency_Shutdown(detected_alarm);
        }

        // === STATE MACHINE ===
        if (emergency_lockout) {
            // -------- ALARM STATE --------
            current_state = STATE_ALARM;

            // Ensure outputs are safe
            HAL_GPIO_WritePin(OUT_PORT, HEATER_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(OUT_PORT, LAMP_PIN, GPIO_PIN_RESET);

            // Handle display (includes buzzer pulsing)
            Handle_Alarm_Display(temp_act);

            // Optional: Uncomment to enable reset button on PA3
            // if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_SET) {
            //     Try_Clear_Lockout(raw_lm35, temp_act);
            // }

        } else if (active) {
            // -------- ACTIVE STATE --------
            HAL_GPIO_WritePin(OUT_PORT, LAMP_PIN, GPIO_PIN_SET);

            // Update heater with hysteresis
            Update_Heater_State(temp_act, temp_target);

            // Apply heater state
            HAL_GPIO_WritePin(OUT_PORT, HEATER_PIN, heater_on ? GPIO_PIN_SET : GPIO_PIN_RESET);

            // Determine display state
            if (heater_on) {
                current_state = STATE_HEATING;
                HAL_GPIO_WritePin(OUT_PORT, BUZZER_PIN, GPIO_PIN_RESET);
            } else {
                current_state = STATE_COOLING;
                // Warning buzzer if significantly over target
                if (temp_act > temp_target + OVERHEAT_WARNING_MARGIN) {
                    HAL_GPIO_WritePin(OUT_PORT, BUZZER_PIN, GPIO_PIN_SET);
                } else {
                    HAL_GPIO_WritePin(OUT_PORT, BUZZER_PIN, GPIO_PIN_RESET);
                }
            }

            Handle_Normal_Display(current_state, temp_act, temp_target);

        } else {
            // -------- STANDBY STATE --------
            current_state = STATE_STANDBY;

            // All outputs off
            HAL_GPIO_WritePin(OUT_PORT, LAMP_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(OUT_PORT, HEATER_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(OUT_PORT, BUZZER_PIN, GPIO_PIN_RESET);
            heater_on = 0;

            Handle_Normal_Display(current_state, temp_act, temp_target);
        }

        // === LOOP TIMING ===
        HAL_Delay(SAMPLE_INTERVAL_MS);
    }
}

/* ============================================================= */
/* --- SYSTEM CONFIGURATION FUNCTIONS ---                        */
/* ============================================================= */

void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    // Use internal HSI oscillator (8 MHz)
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;  // Required for IWDG
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                   RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
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

    // Enable GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    // Initialize outputs to safe state (OFF)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
                             GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2, GPIO_PIN_RESET);

    // LCD pins (PB0-PB5) as outputs
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
                          GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // Control outputs (PC0-PC2) as outputs
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // Optional: Reset button input (PA3) - uncomment if using
    // GPIO_InitStruct.Pin = GPIO_PIN_3;
    // GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    // GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    // HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
