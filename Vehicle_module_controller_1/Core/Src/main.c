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
#include "can.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define key1 HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_14)
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
RxMsg g_rxMsg = {0};// 定义一个全局的接收消息结构体，初始化为0x00
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
TxMsg g_txMsg[2] = {
  {
    .txHeader.StdId = 0x555,
    .txHeader.IDE = CAN_ID_STD,
    .txHeader.RTR = CAN_RTR_DATA,
    .txHeader.DLC = 4,
    .txMailBox = 0,
    .data = {0x13, 0x24,0x07, 0x40}
  },
  {
    .txHeader.StdId = 0x555,
    .txHeader.IDE = CAN_ID_STD,
    .txHeader.RTR = CAN_RTR_DATA,
    .txHeader.DLC = 4,
    .txMailBox = 0,
    .data = {0x13, 0x24,0x08, 0x40}
  },
}; 
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t key_scan(void)
{
	uint8_t key_val = 0xff;
	static uint8_t key_flag = 1; 
	/*按下过程*/
	//检测按键按下
	if((key1 ==GPIO_PIN_RESET) && key_flag)
	{
		//延时消抖
		HAL_Delay(15);
		//再次检测按键按下
		if((key1 ==GPIO_PIN_RESET))
		{
			//赋值键值
			key_val = 1;
			//锁定标志位
			key_flag=0;
		}
	}
	/*抬起过程*/
	if((key1 !=GPIO_PIN_RESET))
	{
		key_flag=1;
	}
	//返回键值
	return key_val;
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
  MX_CAN_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
	HAL_CAN_Start(&hcan);
  CAN_FilterConfig();
	OLED_Init();
	HAL_GPIO_WritePin(GPIOC,GPIO_PIN_13,GPIO_PIN_RESET);
	OLED_Printf(0,0,OLED_8X16,"你好");
	OLED_Update();
  printf("a");
	uint8_t state=0;
	uint8_t button;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		button =key_scan();
		if(button==1)
		{
			state ^=1;
		  if(state != 0)
      {
        HAL_GPIO_WritePin(GPIOB,GPIO_PIN_5,GPIO_PIN_SET);
        CAN_SendMsg(&g_txMsg[0]);
      }
		  else
      {
        HAL_GPIO_WritePin(GPIOB,GPIO_PIN_5,GPIO_PIN_RESET);
        CAN_SendMsg(&g_txMsg[1]);
      }
      
		}
		
    if(CAN_IsMsgPending())
    {
        CAN_ReceiveMsg(&g_rxMsg);
        if(g_rxMsg.data[0]==0xaa &&g_rxMsg.data[1]==0x11)
        {
          switch (g_rxMsg.data[2])
          {
            case 0x07:
              HAL_GPIO_WritePin(GPIOB,GPIO_PIN_5,GPIO_PIN_SET);
              state = 1;
              CAN_SendMsg(&g_txMsg[0]);
            break;
            case 0x08:
              HAL_GPIO_WritePin(GPIOB,GPIO_PIN_5,GPIO_PIN_RESET);
              state = 0;
              CAN_SendMsg(&g_txMsg[1]);
            break;
            default:
              break;
          }
        }
				OLED_ShowString(0, 0, "STID:",OLED_8X16);
        OLED_ShowHexNum(80, 0, g_rxMsg.rxHeader.StdId, 3,OLED_8X16);
        OLED_ShowHexNum(0, 16, g_rxMsg.data[0],  2,OLED_8X16);
        OLED_ShowHexNum(20, 16, g_rxMsg.data[1], 2,OLED_8X16);
        OLED_ShowHexNum(40, 16, g_rxMsg.data[2], 2,OLED_8X16);
        OLED_ShowHexNum(60, 16, g_rxMsg.data[3], 2,OLED_8X16);
				OLED_Update();
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

/* USER CODE BEGIN 4 */

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
