/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : RTC - dong ho thoi gian thuc (LSE 32.768 kHz) + bao thuc, chinh gio qua UART
  * @board          : Blue Pill STM32F103C8T6, HCLK = 72 MHz (HSE 8 MHz)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <ctype.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* LED PC13 tren Blue Pill noi len 3V3 -> SANG khi xuat muc THAP */
#define LED_ON()       HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET)
#define LED_OFF()      HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET)
#define LED_TOGGLE()   HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin)
#define LED_SET(on)    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, (on) ? GPIO_PIN_RESET : GPIO_PIN_SET)
#define LED_IS_ON()    (HAL_GPIO_ReadPin(LED_GPIO_Port, LED_Pin) == GPIO_PIN_RESET)
#define RTC_BKP_MAGIC   0x32F2U    /* DR1: da cai gio roi */
/* F1: RTC chi dem GIAY, ngay thang chi luu trong RAM -> tu luu ngay vao Backup register */
#define BKP_DATE        RTC_BKP_DR2  /* (thang << 8) | ngay */
#define BKP_YEAR        RTC_BKP_DR3
#define ALARM_LED_MS    5000U
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
static volatile uint8_t alarm_flag = 0;
static uint8_t          alarm_on = 0;
static RTC_AlarmTypeDef alarm_cfg = {0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RTC_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Chuyen huong printf() ra USART1 (syscalls.c goi ham nay) */
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

/* Doc 1 dong lenh tu UART theo kieu polling khong chan.
 * Tra ve 1 khi nhan du 1 dong (ket thuc bang Enter). */
static uint8_t UART_ReadLine(char *buf, uint8_t size)
{
  static uint8_t len = 0;
  uint8_t c;
  while (HAL_UART_Receive(&huart1, &c, 1, 0) == HAL_OK)
  {
    if (c == '\r' || c == '\n')
    {
      if (len == 0) continue;
      buf[len] = 0;
      len = 0;
      printf("\r\n");
      return 1;
    }
    if (len < size - 1 && c >= 32 && c < 127)
    {
      buf[len++] = (char)c;
      HAL_UART_Transmit(&huart1, &c, 1, 10);   /* echo */
    }
  }
  return 0;
}

static void Save_Date(const RTC_DateTypeDef *d)
{
  HAL_RTCEx_BKUPWrite(&hrtc, BKP_DATE, ((uint32_t)d->Month << 8) | d->Date);
  HAL_RTCEx_BKUPWrite(&hrtc, BKP_YEAR, d->Year);
}

static void Restore_Date(void)
{
  uint32_t v = HAL_RTCEx_BKUPRead(&hrtc, BKP_DATE);
  if (v == 0) return;                              /* chua tung luu */
  RTC_DateTypeDef d = {0};
  d.Date = (uint8_t)(v & 0xFF); d.Month = (uint8_t)(v >> 8);
  d.Year = (uint8_t)HAL_RTCEx_BKUPRead(&hrtc, BKP_YEAR);
  d.WeekDay = RTC_WEEKDAY_MONDAY;
  HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN);
}

static void Print_Help(void)
{
  printf("Lenh:  T hh:mm:ss   - dat gio      (VD: T 08:30:00)\r\n");
  printf("       D dd/mm/yy   - dat ngay     (VD: D 27/11/26)\r\n");
  printf("       A hh:mm:ss   - dat bao thuc (VD: A 08:31:00)\r\n");
  printf("       X            - huy bao thuc\r\n");
}

static void Handle_Command(char *s)
{
  unsigned a, b, c;
  char cmd = (char)toupper((unsigned char)s[0]);

  if (cmd == 'T' && sscanf(s + 1, "%u:%u:%u", &a, &b, &c) == 3 && a < 24 && b < 60 && c < 60)
  {
    RTC_TimeTypeDef t = {0};
    t.Hours = (uint8_t)a; t.Minutes = (uint8_t)b; t.Seconds = (uint8_t)c;
    HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_BKP_MAGIC);
    printf("OK: da dat gio\r\n");
  }
  else if (cmd == 'D' && sscanf(s + 1, "%u/%u/%u", &a, &b, &c) == 3 && a >= 1 && a <= 31 && b >= 1 && b <= 12 && c <= 99)
  {
    RTC_DateTypeDef d = {0};
    d.Date = (uint8_t)a; d.Month = (uint8_t)b; d.Year = (uint8_t)c; d.WeekDay = RTC_WEEKDAY_MONDAY;
    HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN);
    Save_Date(&d);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_BKP_MAGIC);
    printf("OK: da dat ngay\r\n");
  }
  else if (cmd == 'A' && sscanf(s + 1, "%u:%u:%u", &a, &b, &c) == 3 && a < 24 && b < 60 && c < 60)
  {
    alarm_cfg.AlarmTime.Hours = (uint8_t)a; alarm_cfg.AlarmTime.Minutes = (uint8_t)b; alarm_cfg.AlarmTime.Seconds = (uint8_t)c;
    alarm_cfg.Alarm = RTC_ALARM_A;
    if (HAL_RTC_SetAlarm_IT(&hrtc, &alarm_cfg, RTC_FORMAT_BIN) == HAL_OK)
    {
      alarm_on = 1;
      printf("OK: bao thuc luc %02u:%02u:%02u (moi ngay)\r\n", a, b, c);
    }
    else printf("Loi dat bao thuc\r\n");
  }
  else if (cmd == 'X')
  {
    HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
    alarm_on = 0;
    printf("OK: da huy bao thuc\r\n");
  }
  else
  {
    Print_Help();
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
  RTC_TimeTypeDef t;
  RTC_DateTypeDef d;
  uint8_t  last_sec = 0xFF, last_day = 0;
  uint32_t t_alarm_led = 0;
  char     line[24];
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
  MX_RTC_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  printf("\r\n=== RTC Clock + Alarm ===\r\n");
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) == RTC_BKP_MAGIC)
  {
    Restore_Date();
    printf("Gio duoc giu tu lan truoc\r\n");
  }
  else
  {
    printf("RTC moi khoi tao 00:00:00 - hay dat gio bang lenh T\r\n");
  }
  Print_Help();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (UART_ReadLine(line, sizeof(line))) Handle_Command(line);

    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);    /* F1: GetTime tu cap nhat ngay khi qua 24h */
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
    if (t.Seconds != last_sec)
    {
      last_sec = t.Seconds;
      printf("%02u:%02u:%02u  %02u/%02u/20%02u\r\n", t.Hours, t.Minutes, t.Seconds, d.Date, d.Month, d.Year);
      if (d.Date != last_day) { last_day = d.Date; Save_Date(&d); }   /* qua ngay moi -> luu lai */
    }

    if (alarm_flag)
    {
      alarm_flag = 0;
      t_alarm_led = HAL_GetTick();
      LED_ON();
      printf("*** BAO THUC! *** (LED sang %u s)\r\n", ALARM_LED_MS / 1000U);
      if (alarm_on) HAL_RTC_SetAlarm_IT(&hrtc, &alarm_cfg, RTC_FORMAT_BIN);   /* hen lai ngay mai */
    }
    if (t_alarm_led && HAL_GetTick() - t_alarm_led >= ALARM_LED_MS)
    {
      t_alarm_led = 0;
      LED_OFF();
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  *        HSE 8 MHz -> PLL x9 -> SYSCLK = HCLK = 72 MHz
  *        APB1 = 36 MHz (TIM2/3/4 clock 72 MHz), APB2 = 72 MHz
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef DateToUpdate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */
  /* Neu da cai gio truoc do (co magic trong Backup register) -> KHONG ghi de gio */
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) == RTC_BKP_MAGIC)
  {
    return;
  }
  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;

  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  DateToUpdate.WeekDay = RTC_WEEKDAY_MONDAY;
  DateToUpdate.Month = RTC_MONTH_JANUARY;
  DateToUpdate.Date = 0x1;
  DateToUpdate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &DateToUpdate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief USART1 Initialization Function (115200 8N1, PA9 TX / PA10 RX)
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
  huart1.Init.BaudRate = 115200;
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
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
  * @brief Goi tu HAL_RTC_AlarmIRQHandler khi den gio bao thuc.
  */
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
  (void)hrtc;
  alarm_flag = 1;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
