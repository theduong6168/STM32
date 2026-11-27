/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Flash noi - dem so lan khoi dong, luu vao page cuoi (ghi noi tiep, it xoa)
  * @board          : Blue Pill STM32F103C8T6, HCLK = 72 MHz (HSE 8 MHz)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
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
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

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

/* Tra ve 1 khi vua nhan nut BTN (PB12 keo len, nhan = muc thap), co chong doi phim */
static uint8_t Button_WasPressed(void)
{
  static GPIO_PinState stable = GPIO_PIN_SET;
  static uint32_t t_change = 0;
  GPIO_PinState now = HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin);
  if (now != stable && (HAL_GetTick() - t_change) > 30U)
  {
    t_change = HAL_GetTick();
    stable = now;
    if (now == GPIO_PIN_RESET) return 1;
  }
  return 0;
}

/* ---------------- Flash noi: dung PAGE CUOI (page 63: 0x0800FC00 - 0x0800FFFF, 1 KB) ----------------
 * STM32F103C8: 64 KB Flash = 64 page x 1 KB.
 * Flash chi ghi duoc bit 1 -> 0. Muon ghi lai phai XOA ca page (ve 0xFF).
 */
#define USER_FLASH_ADDR     0x0800FC00UL
#define USER_FLASH_SIZE     FLASH_PAGE_SIZE           /* 1024 byte */

static HAL_StatusTypeDef Flash_EraseUserPage(void)
{
  FLASH_EraseInitTypeDef er = {0};
  uint32_t page_error = 0;
  HAL_StatusTypeDef st;

  er.TypeErase   = FLASH_TYPEERASE_PAGES;
  er.PageAddress = USER_FLASH_ADDR;
  er.NbPages     = 1;

  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
  st = HAL_FLASHEx_Erase(&er, &page_error);
  HAL_FLASH_Lock();
  return st;
}

static HAL_StatusTypeDef Flash_WriteWords(uint32_t addr, const uint32_t *data, uint32_t n_words)
{
  HAL_StatusTypeDef st = HAL_OK;
  HAL_FLASH_Unlock();
  for (uint32_t i = 0; i < n_words && st == HAL_OK; i++)
  {
    st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i * 4U, data[i]);
  }
  HAL_FLASH_Lock();
  return st;
}

/* Moi lan khoi dong ghi them 1 word vao o trong tiep theo.
 * So lan khoi dong = so word da ghi. Chi xoa page khi day (256 lan). */
static uint32_t Boot_CountAndLog(void)
{
  const uint32_t *p = (const uint32_t *)USER_FLASH_ADDR;
  const uint32_t n_slots = USER_FLASH_SIZE / 4U;
  uint32_t idx = 0;
  static uint32_t total_base = 0;

  while (idx < n_slots && p[idx] != 0xFFFFFFFFUL) idx++;      /* tim o trong dau tien */
  if (idx > 0) total_base = p[idx - 1];                         /* gia tri cuoi = tong so lan */

  if (idx >= n_slots)                                           /* day -> xoa va ghi lai tu dau */
  {
    printf("Page day, dang xoa...\r\n");
    Flash_EraseUserPage();
    idx = 0;
  }
  uint32_t value = total_base + 1U;                             /* tong so lan khoi dong */
  if (Flash_WriteWords(USER_FLASH_ADDR + idx * 4U, &value, 1) != HAL_OK)
  {
    printf("Loi ghi Flash! (err=0x%lX)\r\n", HAL_FLASH_GetError());
  }
  return value;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint32_t boots;
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
  printf("\r\n=== Bai01 Flash Boot Counter ===\r\n");
  boots = Boot_CountAndLog();
  printf(">>> Day la lan khoi dong thu %lu <<<\r\n", boots);
  printf("Nhan nut RESET de tang so dem. Nhan BTN de XOA page (ve 0).\r\n");
  printf("Xem du lieu: Window > Show View > Memory, dia chi 0x%08lX\r\n", USER_FLASH_ADDR);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (Button_WasPressed())
    {
      uint32_t t0 = HAL_GetTick();
      LED_ON();
      HAL_StatusTypeDef st = Flash_EraseUserPage();
      LED_OFF();
      printf("Xoa page: %s, mat %lu ms. Word dau = 0x%08lX\r\n",
             st == HAL_OK ? "OK" : "LOI", HAL_GetTick() - t0, *(const uint32_t *)USER_FLASH_ADDR);
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

  /*Configure GPIO pin : BTN_Pin */
  GPIO_InitStruct.Pin = BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
