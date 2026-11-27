/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : I2C - quet dia chi thiet bi tren bus I2C1 (PB6/PB7), doc cam bien MPU6050 neu co
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
#define MPU_ADDR_LOW      0x68     /* chan AD0 = 0 */
#define MPU_ADDR_HIGH     0x69     /* chan AD0 = 1 */
#define MPU_REG_WHO_AM_I  0x75
#define MPU_REG_PWR_MGMT1 0x6B
#define MPU_REG_ACCEL_X   0x3B     /* 14 byte: accel XYZ, temp, gyro XYZ */
#define I2C_TIMEOUT       50U
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
static uint8_t mpu_addr = 0;       /* 0 = chua tim thay */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
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

/* Quet toan bo dia chi 7 bit, tra ve so thiet bi tim thay */
static uint8_t I2C_Scan(void)
{
  uint8_t found = 0;
  printf("Quet bus I2C1 (PB6=SCL, PB7=SDA)...\r\n");
  printf("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");
  for (uint8_t row = 0; row < 8; row++)
  {
    printf("%02X: ", row << 4);
    for (uint8_t col = 0; col < 16; col++)
    {
      uint8_t addr = (uint8_t)((row << 4) | col);
      if (addr < 0x03 || addr > 0x77) { printf("   "); continue; }
      /* HAL dung dia chi 8 bit -> dich trai 1 */
      if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 2, 5) == HAL_OK)
      {
        printf("%02X ", addr);
        found++;
        if (addr == MPU_ADDR_LOW || addr == MPU_ADDR_HIGH) mpu_addr = addr;
      }
      else printf("-- ");
    }
    printf("\r\n");
  }
  printf("Tim thay %u thiet bi.\r\n", found);
  return found;
}

static HAL_StatusTypeDef MPU_Init(void)
{
  uint8_t who = 0, wake = 0x00;
  if (HAL_I2C_Mem_Read(&hi2c1, mpu_addr << 1, MPU_REG_WHO_AM_I, I2C_MEMADD_SIZE_8BIT, &who, 1, I2C_TIMEOUT) != HAL_OK)
    return HAL_ERROR;
  printf("MPU6050 @0x%02X, WHO_AM_I = 0x%02X %s\r\n", mpu_addr, who, (who == 0x68) ? "(dung)" : "(khac - co the la MPU6500/9250)");
  /* Thoat che do sleep */
  return HAL_I2C_Mem_Write(&hi2c1, mpu_addr << 1, MPU_REG_PWR_MGMT1, I2C_MEMADD_SIZE_8BIT, &wake, 1, I2C_TIMEOUT);
}

static void MPU_ReadPrint(void)
{
  uint8_t b[14];
  if (HAL_I2C_Mem_Read(&hi2c1, mpu_addr << 1, MPU_REG_ACCEL_X, I2C_MEMADD_SIZE_8BIT, b, 14, I2C_TIMEOUT) != HAL_OK)
  {
    printf("Loi doc MPU6050 (err=0x%lX)\r\n", HAL_I2C_GetError(&hi2c1));
    return;
  }
  int16_t ax = (int16_t)(b[0] << 8 | b[1]), ay = (int16_t)(b[2] << 8 | b[3]), az = (int16_t)(b[4] << 8 | b[5]);
  int16_t tr = (int16_t)(b[6] << 8 | b[7]);
  int16_t gx = (int16_t)(b[8] << 8 | b[9]), gy = (int16_t)(b[10] << 8 | b[11]), gz = (int16_t)(b[12] << 8 | b[13]);

  /* Mac dinh: +-2g -> 16384 LSB/g ; +-250 do/s -> 131 LSB/(do/s) ; T = raw/340 + 36.53 */
  int32_t t_centi = (int32_t)tr * 100 / 340 + 3653;
  printf("Acc[mg] X=%6ld Y=%6ld Z=%6ld | Gyro[dps] X=%5ld Y=%5ld Z=%5ld | T=%ld.%02ld C\r\n",
         (int32_t)ax * 1000 / 16384, (int32_t)ay * 1000 / 16384, (int32_t)az * 1000 / 16384,
         (int32_t)gx / 131, (int32_t)gy / 131, (int32_t)gz / 131,
         t_centi / 100, (t_centi < 0 ? -t_centi : t_centi) % 100);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint32_t t_last = 0;
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
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  printf("\r\n=== Bai01 I2C Scanner + MPU6050 ===\r\n");
  I2C_Scan();
  if (mpu_addr && MPU_Init() != HAL_OK) mpu_addr = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (mpu_addr)
    {
      if (HAL_GetTick() - t_last >= 500) { t_last = HAL_GetTick(); MPU_ReadPrint(); LED_TOGGLE(); }
    }
    else if (HAL_GetTick() - t_last >= 3000)     /* khong co MPU6050 -> quet lai moi 3 s */
    {
      t_last = HAL_GetTick();
      I2C_Scan();
      if (mpu_addr && MPU_Init() != HAL_OK) mpu_addr = 0;
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
  * @brief I2C1 Initialization Function (Standard mode 100 kHz)
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
