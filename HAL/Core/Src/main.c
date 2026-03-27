/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "MAX30102.h"
#include "MAX9814.h"
#include "BH1750.h"
#include "HC06.h"
#include "driver_oled.h"
#include "ppg_algo.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum
{
  SLEEP_UNKNOWN = 0,
  SLEEP_AWAKE,
  SLEEP_LIGHT,
  SLEEP_DEEP
} SleepState_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define LED_PWM_INVERTED 0U
#define NOISE_PLAY_THRESHOLD_PCT  55U
#define NOISE_STOP_THRESHOLD_PCT  45U
#define SNORE_EVENT_COOLDOWN_MS   8000U
#define SNORE_BLOCK_SIZE          128U
#define SNORE_START_THRESHOLD_PCT 56U
#define SNORE_END_THRESHOLD_PCT   43U
#define SNORE_BURST_MIN_FRAMES    25U
#define SNORE_BURST_MAX_FRAMES    162U
#define SNORE_INTERVAL_MIN_FRAMES 44U
#define SNORE_INTERVAL_MAX_FRAMES 375U
#define SNORE_END_GAP_FRAMES      6U
#define SNORE_C0_DELTA_Q8         120U
#define SNORE_C1_MIN_Q8           20

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

MAX30102_HandleTypeDef g_max30102;
MAX9814_HandleTypeDef g_max9814;
uint16_t adc_buf[256];
volatile uint8_t g_adc_half_ready = 0;
volatile uint8_t g_adc_full_ready = 0;

uint32_t g_last_red = 0;
uint32_t g_last_ir = 0;
uint32_t g_lux = 0;
uint8_t g_snd_percent = 0;
uint8_t g_max30102_fail_count = 0;

volatile uint8_t g_key_pressed = 0;
volatile uint32_t g_key_press_tick = 0;
volatile uint8_t g_key_short_event = 0;
volatile uint32_t g_key_last_irq_tick = 0;

uint8_t g_page_index = 0;
uint8_t g_led_duty = 0;
uint8_t g_led_target_duty = 30;

SleepState_t g_sleep_state = SLEEP_UNKNOWN;
SleepState_t g_sleep_candidate = SLEEP_UNKNOWN;
uint8_t g_sleep_stable_count = 0;
uint32_t g_sleep_last_change_tick = 0;

HC06_HandleTypeDef g_hc06;
uint8_t g_bt_linked = 0;
uint8_t g_music_play_sent = 0;
uint8_t g_snore_active = 0;
uint16_t g_snore_count = 0;
uint32_t g_snore_last_event_tick = 0;
uint8_t g_snore_in_burst = 0;
uint8_t g_snore_pattern_score = 0;
uint32_t g_snore_burst_start_frame = 0;
uint32_t g_snore_prev_burst_end_frame = 0;
uint32_t g_snore_last_activity_frame = 0;
uint32_t g_snore_frame_index = 0;
uint16_t g_snore_c0_floor_q8 = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */

static void UpdateSleepState(const PPG_Result_t *result, uint8_t snd_percent, uint32_t lux);
static void HandleKeyEvents(void);
static void RenderPage(const PPG_Result_t *result);
static void SetLedDutyPercent(uint8_t duty);
static void UpdateLedTarget(void);
static void ProcessLedDimmingStep(void);
static void SendTelemetry(const PPG_Result_t *result);
static void ProcessMusicCommand(void);
static void ProcessSnoreFrame(const uint16_t *samples, uint16_t count);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void UpdateSleepState(const PPG_Result_t *result, uint8_t snd_percent, uint32_t lux)
{
  SleepState_t next_state;
  uint8_t stable_needed = 3U;
  uint32_t cooldown_ms;
  static uint8_t invalid_streak = 0U;

  if (((result->sqi == PPG_SQI_BAD) && (result->hr_valid == 0U) && (result->spo2_valid == 0U)) ||
      ((result->hr_valid == 0U) && (result->spo2_valid == 0U)))
  {
    if (invalid_streak < 10U)
    {
      invalid_streak++;
    }

    if (invalid_streak < 4U)
    {
      return;
    }

    next_state = SLEEP_UNKNOWN;
  }
  else if ((snd_percent > 35U) || (lux > 120U) || ((result->hr_valid != 0U) && (result->hr_bpm > 95U)))
  {
    invalid_streak = 0U;
    next_state = SLEEP_AWAKE;
  }
  else if ((snd_percent < 15U) && (lux < 30U) && ((result->hr_valid == 0U) || (result->hr_bpm < 72U)) && (result->spo2_valid != 0U) && (result->spo2 >= 93U))
  {
    invalid_streak = 0U;
    next_state = SLEEP_DEEP;
  }
  else
  {
    invalid_streak = 0U;
    next_state = SLEEP_LIGHT;
  }

  if (next_state == g_sleep_candidate)
  {
    if (g_sleep_stable_count < 5U)
    {
      g_sleep_stable_count++;
    }
  }
  else
  {
    g_sleep_candidate = next_state;
    g_sleep_stable_count = 1U;
  }

  if (next_state == SLEEP_UNKNOWN)
  {
    stable_needed = 2U;
  }

  cooldown_ms = (next_state == SLEEP_UNKNOWN) ? 3000U : 10000U;
  if (g_sleep_stable_count >= stable_needed)
  {
    if ((g_sleep_state == SLEEP_UNKNOWN) ||
        ((HAL_GetTick() - g_sleep_last_change_tick) >= cooldown_ms))
    {
      if (g_sleep_state != g_sleep_candidate)
      {
        g_sleep_state = g_sleep_candidate;
        g_sleep_last_change_tick = HAL_GetTick();
      }
    }
  }
}

static void HandleKeyEvents(void)
{
  if (g_key_short_event != 0U)
  {
    g_key_short_event = 0U;
    g_page_index = (uint8_t)((g_page_index + 1U) % 3U);
    OLED_Clear();
  }
}

static void RenderPage(const PPG_Result_t *result)
{
  if (g_page_index == 0U)
  {
    OLED_ShowString(1, 1, "ENVIRONMENT");
    OLED_ShowString(2, 1, "SND:");
    OLED_ShowNum(2, 5, g_snd_percent, 3);
    OLED_ShowString(2, 8, "%");
    OLED_ShowString(3, 1, "LUX:");
    OLED_ShowNum(3, 5, g_lux, 4);
    OLED_ShowString(4, 1, "PAGE1/3");
    OLED_ShowString(4, 9, "D:");
    OLED_ShowNum(4, 11, g_led_duty, 2);
  }
  else if (g_page_index == 1U)
  {
    OLED_ShowString(1, 1, "BIO SIGNAL");
    OLED_ShowString(2, 1, "HR:");
    if (result->hr_valid != 0U)
    {
      OLED_ShowNum(2, 4, result->hr_bpm, 3);
    }
    else
    {
      OLED_ShowString(2, 4, "---");
    }
    OLED_ShowString(2, 8, "bpm");

    OLED_ShowString(3, 1, "SpO2:");
    if (result->spo2_valid != 0U)
    {
      OLED_ShowNum(3, 6, result->spo2, 3);
    }
    else
    {
      OLED_ShowString(3, 6, "---");
    }
    OLED_ShowString(3, 10, "%");
    OLED_ShowString(4, 1, "PAGE2/3");
  }
  else
  {
    OLED_ShowString(1, 1, "SLEEP QUALITY");
    OLED_ShowString(2, 1, "ST:");
    if (g_sleep_state == SLEEP_AWAKE)
    {
      OLED_ShowString(2, 4, "AWAKE ");
    }
    else if (g_sleep_state == SLEEP_LIGHT)
    {
      OLED_ShowString(2, 4, "LIGHT ");
    }
    else if (g_sleep_state == SLEEP_DEEP)
    {
      OLED_ShowString(2, 4, "DEEP  ");
    }
    else
    {
      OLED_ShowString(2, 4, "UNKNWN");
    }

    OLED_ShowString(3, 1, "SQI:");
    if (result->sqi == PPG_SQI_GOOD)
    {
      OLED_ShowString(3, 5, "GOOD");
    }
    else if (result->sqi == PPG_SQI_MID)
    {
      OLED_ShowString(3, 5, "MID ");
    }
    else
    {
      OLED_ShowString(3, 5, "BAD ");
    }

    OLED_ShowString(3, 10, "H:");
    if (result->hr_valid != 0U)
    {
      OLED_ShowNum(3, 12, result->hr_bpm, 2);
    }
    else
    {
      OLED_ShowString(3, 12, "--");
    }

  OLED_ShowString(4, 1, "BT:");
  if (g_bt_linked != 0U)
  {
    OLED_ShowString(4, 4, "L ");
  }
  else
  {
    OLED_ShowString(4, 4, "S ");
  }
  OLED_ShowString(4, 7, "SN:");
  if (g_snore_active != 0U)
  {
    OLED_ShowString(4, 10, "Y");
  }
  else
  {
    OLED_ShowString(4, 10, "N");
  }
  OLED_ShowNum(4, 12, g_snore_pattern_score, 1);
  }
}

static void SetLedDutyPercent(uint8_t duty)
{
  uint32_t arr;
  uint32_t ccr;
  uint8_t applied;

  if (duty > 100U)
  {
    duty = 100U;
  }

  g_led_duty = duty;
  applied = duty;
#if (LED_PWM_INVERTED == 1U)
  applied = (uint8_t)(100U - duty);
#endif

  arr = __HAL_TIM_GET_AUTORELOAD(&htim3);
  ccr = ((arr + 1U) * applied) / 100U;
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ccr);
}

static void UpdateLedTarget(void)
{
  int16_t target;

  if (g_sleep_state == SLEEP_AWAKE)
  {
    target = 78;
  }
  else if (g_sleep_state == SLEEP_LIGHT)
  {
    target = 45;
  }
  else if (g_sleep_state == SLEEP_DEEP)
  {
    target = 12;
  }
  else
  {
    target = 35;
  }

  if (g_lux <= 20U)
  {
    target -= 10;
  }
  else if (g_lux <= 120U)
  {
    target += 0;
  }
  else if (g_lux <= 500U)
  {
    target += 10;
  }
  else if (g_lux <= 2000U)
  {
    target += 20;
  }
  else
  {
    target += 30;
  }

  if (target < 5)
  {
    target = 5;
  }
  if (target > 95)
  {
    target = 95;
  }

  g_led_target_duty = (uint8_t)target;
}

static void ProcessLedDimmingStep(void)
{
  if (g_led_duty < g_led_target_duty)
  {
    SetLedDutyPercent((uint8_t)(g_led_duty + 1U));
  }
  else if (g_led_duty > g_led_target_duty)
  {
    SetLedDutyPercent((uint8_t)(g_led_duty - 1U));
  }
}

static void SendTelemetry(const PPG_Result_t *result)
{
  char line[96];
  int len;
  uint8_t slp;
  uint16_t hr = (result->hr_valid != 0U) ? result->hr_bpm : 0U;
  uint8_t spo2 = (result->spo2_valid != 0U) ? result->spo2 : 0U;

  if (g_sleep_state == SLEEP_DEEP)
  {
    slp = 2U;
  }
  else if (g_sleep_state == SLEEP_LIGHT)
  {
    slp = 1U;
  }
  else
  {
    slp = 0U;
  }

  len = snprintf(line,
                 sizeof(line),
                 "DATA:HR=%u,SPO2=%u,SLP=%u,LGT=%lu,NS=%u\r\n",
                 (unsigned)hr,
                 (unsigned)spo2,
                 (unsigned)slp,
                 (unsigned long)g_lux,
                 (unsigned)g_snd_percent);
  if (len > 0)
  {
    len = snprintf(line,
                   sizeof(line),
                   "DATA:HR=%u,SPO2=%u,SLP=%u,LGT=%lu,NS=%u,SNR=%u\r\n",
                   (unsigned)hr,
                   (unsigned)spo2,
                   (unsigned)slp,
                   (unsigned long)g_lux,
                   (unsigned)g_snd_percent,
                   (unsigned)g_snore_count);
  }

  if (len > 0)
  {
    HAL_StatusTypeDef st = HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)len, 100U);
    g_bt_linked = (st == HAL_OK) ? 1U : 0U;
  }
}

static void ProcessSnoreFrame(const uint16_t *samples, uint16_t count)
{
  uint8_t in_sleep;
  uint16_t i;
  int32_t sum = 0;
  uint32_t sum_abs = 0U;
  int16_t c0_q8;
  int16_t c1_q8;
  int16_t start_thr_q8;
  int16_t end_thr_q8;
  uint32_t burst_frames;
  uint32_t interval_frames;

  in_sleep = (g_sleep_state == SLEEP_LIGHT) || (g_sleep_state == SLEEP_DEEP);

  if ((samples == NULL) || (count == 0U))
  {
    return;
  }

  for (i = 0U; i < count; i++)
  {
    sum += (int32_t)samples[i];
  }
  sum /= (int32_t)count;

  for (i = 0U; i < count; i++)
  {
    int32_t d = (int32_t)samples[i] - sum;
    if (d < 0)
    {
      d = -d;
    }
    sum_abs += (uint32_t)d;
  }

  c0_q8 = (int16_t)(((sum_abs / count) * 256U) / 4095U);

  sum_abs = 0U;
  for (i = 1U; i < count; i++)
  {
    int32_t d = (int32_t)samples[i] - (int32_t)samples[i - 1U];
    if (d < 0)
    {
      d = -d;
    }
    sum_abs += (uint32_t)d;
  }
  c1_q8 = (int16_t)(((sum_abs / (count - 1U)) * 256U) / 4095U);

  g_snore_frame_index++;

  if (in_sleep == 0U)
  {
    g_snore_active = 0U;
    g_snore_in_burst = 0U;
    g_snore_pattern_score = 0U;
    return;
  }

  if ((g_snore_in_burst == 0U) && (c0_q8 < (int16_t)(g_snore_c0_floor_q8 + (8 * 256 / 100))))
  {
    if (g_snore_c0_floor_q8 == 0U)
    {
      g_snore_c0_floor_q8 = (uint16_t)c0_q8;
    }
    else
    {
      g_snore_c0_floor_q8 = (uint16_t)((15U * g_snore_c0_floor_q8 + (uint16_t)c0_q8) / 16U);
    }
  }

  start_thr_q8 = (int16_t)(g_snore_c0_floor_q8 + SNORE_C0_DELTA_Q8);
  if (start_thr_q8 < (int16_t)(SNORE_START_THRESHOLD_PCT * 256 / 100))
  {
    start_thr_q8 = (int16_t)(SNORE_START_THRESHOLD_PCT * 256 / 100);
  }

  end_thr_q8 = (int16_t)(g_snore_c0_floor_q8 + (SNORE_C0_DELTA_Q8 / 2));
  if (end_thr_q8 < (int16_t)(SNORE_END_THRESHOLD_PCT * 256 / 100))
  {
    end_thr_q8 = (int16_t)(SNORE_END_THRESHOLD_PCT * 256 / 100);
  }

  if ((g_snore_in_burst == 0U) && (c0_q8 >= start_thr_q8) && (c1_q8 >= SNORE_C1_MIN_Q8))
  {
    g_snore_in_burst = 1U;
    g_snore_burst_start_frame = g_snore_frame_index;
    g_snore_last_activity_frame = g_snore_frame_index;
  }

  if (g_snore_in_burst != 0U)
  {
    if (c0_q8 > end_thr_q8)
    {
      g_snore_last_activity_frame = g_snore_frame_index;
    }

    if ((c0_q8 <= end_thr_q8) && ((g_snore_frame_index - g_snore_last_activity_frame) >= SNORE_END_GAP_FRAMES))
    {
      g_snore_in_burst = 0U;
      burst_frames = g_snore_frame_index - g_snore_burst_start_frame;

      if ((burst_frames >= SNORE_BURST_MIN_FRAMES) && (burst_frames <= SNORE_BURST_MAX_FRAMES))
      {
        if (g_snore_prev_burst_end_frame != 0U)
        {
          interval_frames = g_snore_burst_start_frame - g_snore_prev_burst_end_frame;
          if ((interval_frames >= SNORE_INTERVAL_MIN_FRAMES) && (interval_frames <= SNORE_INTERVAL_MAX_FRAMES))
          {
            if (g_snore_pattern_score < 6U)
            {
              g_snore_pattern_score++;
            }
          }
          else
          {
            if (g_snore_pattern_score > 0U)
            {
              g_snore_pattern_score--;
            }
          }
        }
        else
        {
          g_snore_pattern_score = 1U;
        }
        g_snore_prev_burst_end_frame = g_snore_frame_index;
      }
    }
  }

  if ((g_snore_pattern_score >= 2U) && ((HAL_GetTick() - g_snore_last_event_tick) >= SNORE_EVENT_COOLDOWN_MS))
  {
    g_snore_active = 1U;
    g_snore_count++;
    g_snore_last_event_tick = HAL_GetTick();
  }

  if ((g_snore_active != 0U) && ((HAL_GetTick() - g_snore_last_event_tick) > 6000U) && (c0_q8 < end_thr_q8))
  {
    g_snore_active = 0U;
  }

  if ((g_snore_pattern_score > 0U) && ((g_snore_frame_index - g_snore_prev_burst_end_frame) > 750U))
  {
    g_snore_pattern_score--;
  }
}

static void ProcessMusicCommand(void)
{
  HAL_StatusTypeDef st;
  static const uint8_t cmd_play[] = "CMD:PLAY\r\n";
  static const uint8_t cmd_stop[] = "CMD:STOP\r\n";

  if ((g_music_play_sent == 0U) && (g_snd_percent >= NOISE_PLAY_THRESHOLD_PCT))
  {
    st = HAL_UART_Transmit(&huart1, (uint8_t *)cmd_play, (uint16_t)(sizeof(cmd_play) - 1U), 100U);
    if (st == HAL_OK)
    {
      g_music_play_sent = 1U;
      g_bt_linked = 1U;
    }
  }
  else if ((g_music_play_sent != 0U) && (g_snd_percent <= NOISE_STOP_THRESHOLD_PCT))
  {
    st = HAL_UART_Transmit(&huart1, (uint8_t *)cmd_stop, (uint16_t)(sizeof(cmd_stop) - 1U), 100U);
    if (st == HAL_OK)
    {
      g_music_play_sent = 0U;
      g_bt_linked = 1U;
    }
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

  HAL_ADCEx_Calibration_Start(&hadc1);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, 256);
  HAL_TIM_OC_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  SetLedDutyPercent(30U);
  g_led_target_duty = 30U;

  OLED_Init();
  OLED_Clear();
  OLED_ShowString(1, 1, "INIT...");

  MAX9814_Init(&g_max9814, 2048U, 4095U);
  (void)MAX30102_Init(&g_max30102);
  (void)HC06_Init(&g_hc06, &huart1, NULL, 0U, 100U);
  Init_BH1750();
  PPG_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t tick_10ms = HAL_GetTick();
  uint32_t tick_200ms = HAL_GetTick();
  uint32_t tick_1000ms = HAL_GetTick();
  uint32_t tick_50ms = HAL_GetTick();
  uint32_t tick_30ms = HAL_GetTick();
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HandleKeyEvents();

    if ((HAL_GetTick() - tick_50ms) >= 50U)
    {
      tick_50ms = HAL_GetTick();
      if ((g_key_pressed != 0U) && (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_SET))
      {
        uint32_t press_ms = HAL_GetTick() - g_key_press_tick;
        g_key_pressed = 0U;
        if ((press_ms >= 40U) && (press_ms < 1000U))
        {
          g_key_short_event = 1U;
        }
      }
    }

    if (g_adc_half_ready != 0U)
    {
      MAX9814_ProcessBlock(&g_max9814, &adc_buf[0], 128U);
      ProcessSnoreFrame(&adc_buf[0], SNORE_BLOCK_SIZE);
      g_adc_half_ready = 0U;
    }

    if (g_adc_full_ready != 0U)
    {
      MAX9814_ProcessBlock(&g_max9814, &adc_buf[128], 128U);
      ProcessSnoreFrame(&adc_buf[128], SNORE_BLOCK_SIZE);
      g_adc_full_ready = 0U;
    }

    if ((HAL_GetTick() - tick_10ms) >= 10U)
    {
      uint8_t wr = 0;
      uint8_t rd = 0;
      uint8_t avail;
      HAL_StatusTypeDef st;

      tick_10ms = HAL_GetTick();
      st = MAX30102_GetFifoPtrs(&g_max30102, &wr, &rd);
      if (st == HAL_OK)
      {
        avail = MAX30102_GetFifoSamplesAvailable(wr, rd);
        while (avail > 0U)
        {
          uint8_t n = (avail > 8U) ? 8U : avail;
          uint32_t red[8];
          uint32_t ir[8];
          uint8_t i;

          st = MAX30102_ReadSamples(&g_max30102, red, ir, n);
          if (st != HAL_OK)
          {
            g_max30102_fail_count++;
            break;
          }

          for (i = 0U; i < n; i++)
          {
            g_last_red = red[i];
            g_last_ir = ir[i];
            PPG_PushSample(red[i], ir[i]);
          }

          avail = (uint8_t)(avail - n);
        }
      }
      else
      {
        g_max30102_fail_count++;
      }

      if (g_max30102_fail_count >= 5U)
      {
        (void)MAX30102_Init(&g_max30102);
        g_max30102_fail_count = 0U;
      }
    }

    if ((HAL_GetTick() - tick_1000ms) >= 1000U)
    {
      PPG_Result_t state_result;
      tick_1000ms = HAL_GetTick();
      g_lux = Value_GY30();
      g_snd_percent = MAX9814_GetLevelPercent(&g_max9814);
      PPG_Update1s();
      state_result = PPG_GetResult();
      UpdateSleepState(&state_result, g_snd_percent, g_lux);
      UpdateLedTarget();
      SendTelemetry(&state_result);
      ProcessMusicCommand();
    }

    if ((HAL_GetTick() - tick_30ms) >= 30U)
    {
      tick_30ms = HAL_GetTick();
      ProcessLedDimmingStep();
    }

    if ((HAL_GetTick() - tick_200ms) >= 200U)
    {
      PPG_Result_t result;
      tick_200ms = HAL_GetTick();
      result = PPG_GetResult();
      RenderPage(&result);
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_CC2;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 124;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_OC2REF;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, software_sdl_Pin|software_sda_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : KEY1_Pin */
  GPIO_InitStruct.Pin = KEY1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(KEY1_GPIO_Port, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /*Configure GPIO pins : software_sdl_Pin software_sda_Pin */
  GPIO_InitStruct.Pin = software_sdl_Pin|software_sda_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1)
  {
    g_adc_half_ready = 1U;
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1)
  {
    g_adc_full_ready = 1U;
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  uint32_t now;

  if (GPIO_Pin != KEY1_Pin)
  {
    return;
  }

  now = HAL_GetTick();
  if ((now - g_key_last_irq_tick) < 30U)
  {
    return;
  }

  g_key_last_irq_tick = now;
  if (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)
  {
    g_key_pressed = 1U;
    g_key_press_tick = now;
  }
}

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM4 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM4)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
