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
#include "driver_key.h"
#include "driver_led.h"
#include "driver_can.h"
#include "driver_oled.h"
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
  volatile uint8_t led1_state ;
  volatile uint8_t led2_state ;
  volatile uint8_t led3_state ;
  volatile uint8_t led4_state ;
}LED_State;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
LED_State g_led_state = {0};

TxMsg g_txMsg[6] = {
    {   // 消息 1
        .txHeader.StdId = 0x666,
        .txHeader.IDE = CAN_ID_STD,
        .txHeader.RTR = CAN_RTR_DATA,
        .txHeader.DLC = 4,
        .txMailBox = 0,
        .data = {0x13, 0x24,0x01, 0x40}
    },
    {   // 消息 2
        .txHeader.StdId = 0x666,
        .txHeader.IDE = CAN_ID_STD,
        .txHeader.RTR = CAN_RTR_DATA,
        .txHeader.DLC = 4,
        .txMailBox = 0,
        .data = {0x13, 0x24,0x02, 0x44}
    },
    {   // 消息 3
        .txHeader.StdId = 0x666,
        .txHeader.IDE = CAN_ID_STD,
        .txHeader.RTR = CAN_RTR_DATA,
        .txHeader.DLC = 4,
        .txMailBox = 0,
        .data = {0x13, 0x24, 0x03, 0x45}
    },
    {   // 消息 4
        .txHeader.StdId = 0x666,
        .txHeader.IDE = CAN_ID_STD,
        .txHeader.RTR = CAN_RTR_DATA,
        .txHeader.DLC = 4,
        .txMailBox = 0,
        .data = {0x13, 0x24, 0x04, 0x46}
    },
    {   // 消息 5
        .txHeader.StdId = 0x666,
        .txHeader.IDE = CAN_ID_STD,
        .txHeader.RTR = CAN_RTR_DATA,
        .txHeader.DLC = 4,
        .txMailBox = 0,
        .data = {0x13, 0x24, 0x05, 0x46}
    },
    {   // 消息 6
        .txHeader.StdId = 0x666,
        .txHeader.IDE = CAN_ID_STD,
        .txHeader.RTR = CAN_RTR_DATA,
        .txHeader.DLC = 4,
        .txMailBox = 0,
        .data = {0x13, 0x24, 0x06, 0x46}
    }
};// 定义一个全局的发送消息结构体数组，每个元素为一个标准ID的4字节数据帧

RxMsg g_rxMsg = {0}; // 定义一个全局的接收消息结构体，初始化为0x00
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
  MX_CAN_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
	/*MX_CAN_Init()中初始化can配置、开启can通信、初始化can过滤器*/
	int val = 0;

  OLED_Init();
  OLED_ShowString(1, 1, "STID:");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    val = Key_Read();
    switch (val)
    {
    case 1:
        g_led_state.led1_state ^= 1;
        if(!g_led_state.led1_state)
        {
          CAN_SendMsg(&g_txMsg[1]);
        }
        else
        {
          CAN_SendMsg(&g_txMsg[0]);
        }
        break;
    case 2:
        g_led_state.led2_state ^= 1;
        if(!g_led_state.led2_state)
        {
          CAN_SendMsg(&g_txMsg[3]);
        }
        else
        {
          CAN_SendMsg(&g_txMsg[2]);
        }
        break;
    case 3:
        g_led_state.led3_state ^= 1;
        if(!g_led_state.led3_state)
        {
          CAN_SendMsg(&g_txMsg[5]);
        }
        else
        {
          CAN_SendMsg(&g_txMsg[4]);
        }
        break;
    case 4:
        g_led_state.led4_state ^= 1;
        // CAN_SendMsg(&g_txMsg[4]);
        break;
    default:
        break;
    }
		
    if(CAN_IsMsgPending())
    {
        CAN_ReceiveMsg(&g_rxMsg);
        if(g_rxMsg.data[0]==0xaa &&g_rxMsg.data[1]==0x11)
        {
          switch (g_rxMsg.data[2])
          {
          case 0x01:
            if(g_led_state.led1_state != 1)
            {
                g_led_state.led1_state = 1;
               // g_txMsg[0].data[2] = 0x01;
                CAN_SendMsg(&g_txMsg[0]);
            }
          break;
          case 0x02:
            if(g_led_state.led1_state != 0)
            {
                g_led_state.led1_state = 0;
               // g_txMsg[0].data[2] = 0x02;
                CAN_SendMsg(&g_txMsg[1]);
            }
          break;
          case 0x03:
            if(g_led_state.led2_state != 1)
            {
                g_led_state.led2_state = 1;
              //  g_txMsg[1].data[2] = 0x03;
                CAN_SendMsg(&g_txMsg[2]);
            }
          break;
          case 0x04:
            if(g_led_state.led2_state != 0)
            {
                g_led_state.led2_state = 0;
                //g_txMsg[1].data[2] = 0x04;
                CAN_SendMsg(&g_txMsg[3]);
            }
          break;
          case 0x05:
            if(g_led_state.led3_state != 1)
            {
                g_led_state.led3_state = 1;
               // g_txMsg[2].data[2] = 0x05;
                CAN_SendMsg(&g_txMsg[4]);
            }
          break;
          case 0x06:
            if(g_led_state.led3_state != 0)
            {
                g_led_state.led3_state = 0;
                //g_txMsg[2].data[2] = 0x06;
                CAN_SendMsg(&g_txMsg[5]);
            }
          break;
          case 0x07:
            if(g_led_state.led4_state != 1)
            {
                g_led_state.led4_state = 1;
               // g_txMsg[3].data[2] = 0x07;
               // CAN_SendMsg(&g_txMsg[3]);
            }
          break;
          case 0x08:
            if(g_led_state.led4_state != 0)
            {
                g_led_state.led4_state = 0;
               // g_txMsg[3].data[2] = 0x08;
               // CAN_SendMsg(&g_txMsg[3]);
            }
          break;
          default:
            break;
          }
        }
				OLED_ShowString(1, 1, "STID:");
        OLED_ShowHexNum(1, 8, g_rxMsg.rxHeader.StdId, 3);
        OLED_ShowHexNum(2, 1, g_rxMsg.data[0], 2);
        OLED_ShowHexNum(2, 5, g_rxMsg.data[1], 2);
        OLED_ShowHexNum(2, 9, g_rxMsg.data[2], 2);
        OLED_ShowHexNum(2, 13, g_rxMsg.data[3], 2);
    }
    
    Led_Control(LED_1, g_led_state.led1_state);
    Led_Control(LED_2, g_led_state.led2_state);
    Led_Control(LED_3, g_led_state.led3_state);
    Led_Control(LED_4, g_led_state.led4_state);

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
