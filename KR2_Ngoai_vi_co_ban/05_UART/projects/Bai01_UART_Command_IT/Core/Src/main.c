/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : UART - nhan lenh tu terminal bang ngat (ring buffer) dieu khien LED
  * @board          : Blue Pill STM32F103C8T6, HCLK = 72 MHz (HSE 8 MHz)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
#define RB_SIZE    128U      /* kich thuoc ring buffer (luy thua cua 2) */
#define LINE_MAX   48U
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
static uint8_t           rx_byte;                 /* byte vua nhan trong ngat */
static volatile uint8_t  rb[RB_SIZE];             /* ring buffer: ISR ghi, main doc */
static volatile uint16_t rb_head = 0, rb_tail = 0;

static char     line[LINE_MAX];
static uint8_t  line_len = 0;
static uint8_t  blink_on = 0;
static uint32_t blink_ms = 500;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
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

static int RB_Get(uint8_t *c)
{
  if (rb_head == rb_tail) return 0;               /* rong */
  *c = rb[rb_tail];
  rb_tail = (rb_tail + 1) & (RB_SIZE - 1);
  return 1;
}

static void Print_Help(void)
{
  printf("Lenh:\r\n");
  printf("  ON           - bat LED\r\n");
  printf("  OFF          - tat LED\r\n");
  printf("  TOGGLE       - dao LED\r\n");
  printf("  BLINK <ms>   - nhay LED voi chu ky <ms> (BLINK 0 de dung)\r\n");
  printf("  STATUS       - xem trang thai\r\n");
  printf("  HELP         - tro giup\r\n");
}

static void Process_Command(char *cmd)
{
  for (char *p = cmd; *p; p++) *p = (char)toupper((unsigned char)*p);

  if (strcmp(cmd, "ON") == 0)          { blink_on = 0; LED_ON();     printf("OK: LED ON\r\n"); }
  else if (strcmp(cmd, "OFF") == 0)    { blink_on = 0; LED_OFF();    printf("OK: LED OFF\r\n"); }
  else if (strcmp(cmd, "TOGGLE") == 0) { blink_on = 0; LED_TOGGLE(); printf("OK: TOGGLE\r\n"); }
  else if (strncmp(cmd, "BLINK", 5) == 0)
  {
    long ms = strtol(cmd + 5, NULL, 10);
    if (ms <= 0) { blink_on = 0; printf("OK: dung nhay\r\n"); }
    else         { blink_on = 1; blink_ms = (uint32_t)ms; printf("OK: nhay %lu ms\r\n", blink_ms); }
  }
  else if (strcmp(cmd, "STATUS") == 0)
  {
    printf("LED = %s, blink = %s (%lu ms), uptime = %lu ms\r\n",
           LED_IS_ON() ? "ON" : "OFF", blink_on ? "co" : "khong", blink_ms, HAL_GetTick());
  }
  else if (strcmp(cmd, "HELP") == 0 || cmd[0] == '?') Print_Help();
  else if (cmd[0] != 0) printf("Lenh khong hop le: '%s' (go HELP)\r\n", cmd);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint32_t t_blink = 0;
  uint8_t  c;
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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  printf("\r\n=== Bai01 UART Command (Interrupt) ===\r\n");
  Print_Help();
  printf("> ");
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);       /* bat dau nhan byte dau tien */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* Lay du lieu tu ring buffer, ghep thanh dong lenh */
    while (RB_Get(&c))
    {
      if (c == '\r' || c == '\n')
      {
        if (line_len == 0) continue;
        printf("\r\n");
        line[line_len] = 0;
        Process_Command(line);
        line_len = 0;
        printf("> ");
      }
      else if ((c == 8 || c == 127) && line_len > 0)   /* Backspace */
      {
        line_len--;
        printf("\b \b");
      }
      else if (line_len < LINE_MAX - 1 && c >= 32 && c < 127)
      {
        line[line_len++] = (char)c;
        HAL_UART_Transmit(&huart1, &c, 1, 10);         /* echo ky tu */
      }
    }

    /* Tac vu nhay LED khong chan */
    if (blink_on && HAL_GetTick() - t_blink >= blink_ms)
    {
      t_blink = HAL_GetTick();
      LED_TOGGLE();
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
  * @brief Goi khi nhan xong 1 byte. Chi cat byte vao ring buffer roi nhan tiep.
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    uint16_t next = (rb_head + 1) & (RB_SIZE - 1);
    if (next != rb_tail)              /* bo qua neu buffer day */
    {
      rb[rb_head] = rx_byte;
      rb_head = next;
    }
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}

/**
  * @brief Loi UART (VD overrun) -> khoi dong lai viec nhan.
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
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
