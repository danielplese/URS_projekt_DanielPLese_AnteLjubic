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
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint16_t rawPot = 0;
uint16_t rawCurrent = 0;
uint16_t zeroCurrentRaw = 0; // Prema ovome se automatski ra�?una nula
float struja_A = 0.0f;

// Izmjena: motor_state mora biti volatile!
volatile uint8_t motor_state = 0;

// Nova zastavica za TIM3
volatile uint8_t timer_flag = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  uint16_t dutyCycle = 0;
  uint8_t brzina_postotak = 0;
  char oled_buffer[20];
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  ssd1306_Init();
  ssd1306_Fill(Black);
  ssd1306_UpdateScreen();

   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);   // Crvena LED ON
   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET); // Zelena LED OFF

   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);

    // Kalibracija nule za ACS712[cite: 4]
   uint32_t sumZero = 0;
   HAL_ADC_Start(&hadc1);
   for(int i = 0; i < 100; i++)
   	   {
          HAL_ADC_PollForConversion(&hadc1, 10); // Rank 1 (Pot
          HAL_ADC_PollForConversion(&hadc1, 10); // Rank 2 (ACS712)
          sumZero += HAL_ADC_GetValue(&hadc1);
          HAL_Delay(5);
      }
    HAL_ADC_Stop(&hadc1);

    zeroCurrentRaw = sumZero / 100;

    HAL_TIM_Base_Start_IT(&htim3);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
		// Izvršava se isklju�?ivo svakih 100 ms kada tajmer podigne zastavicu
		if (timer_flag == 1) {
			timer_flag = 0; // Odmah spusti zastavicu za idući ciklus

			// 1. ČITANJE ADC SENZORA (Potenciometar + Struja)
			HAL_ADC_Start(&hadc1);
			if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
				rawPot = HAL_ADC_GetValue(&hadc1);
			}
			if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
				rawCurrent = HAL_ADC_GetValue(&hadc1);
			}
			HAL_ADC_Stop(&hadc1);

			// Prora�?un struje
			float delta_V = (((float) rawCurrent - (float) zeroCurrentRaw)
					/ 4095.0f) * 3.3f;
			struja_A = fabsf(delta_V / 0.0925f);

			// 2. LOGIKA UPRAVLJANJA MOTOROM I LED-ICAMA
			if (motor_state == 1) {
				dutyCycle = ((float) rawPot) / 4095.0f * 1000.0f;
				brzina_postotak = dutyCycle / 10;

				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET); // Zelena ON
				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET); // Crvena OFF
			} else {
				dutyCycle = 0;
				brzina_postotak = 0;

				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET); // Zelena OFF
				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET); // Crvena ON
			}

			__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, dutyCycle);

			// 3. OSVJEŽAVANJE OLED EKRANA
			ssd1306_Fill(Black);

			// STATUS
			ssd1306_SetCursor(10, 0);
			if (motor_state == 1) {
				ssd1306_WriteString("STATUS: ON", Font_7x10, White);
			} else {
				ssd1306_WriteString("STATUS: OFF", Font_7x10, White);
			}

			// BRZINA
			sprintf(oled_buffer, "BRZINA: %d %%", brzina_postotak);
			ssd1306_SetCursor(10, 10);
			ssd1306_WriteString(oled_buffer, Font_7x10, White);

			// STRUJA
			int32_t struja_mA = (int32_t) (struja_A * 1000.0f);
			sprintf(oled_buffer, "STRUJA: %ld mA", struja_mA);
			ssd1306_SetCursor(10, 20);
			ssd1306_WriteString(oled_buffer, Font_7x10, White);

			// Slanje podataka na ekran preko I2C
			ssd1306_UpdateScreen();
		}
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    static uint32_t last_interrupt_time = 0;
    uint32_t current_time = HAL_GetTick();

    if (GPIO_Pin == GPIO_PIN_7)
    {
        if (current_time - last_interrupt_time > 200)
        {
            motor_state = !motor_state;
            last_interrupt_time = current_time;
        }
    }
}

// Novi prekid za TIM3 (okida se svakih 100 ms)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        timer_flag = 1; // Samo podižemo zastavicu, bez teških operacija u prekidu!
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
  /* User can add his own implementation to report the HAL error return state */
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
