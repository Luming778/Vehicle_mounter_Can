
#include "driver_led.h"
#include "gpio.h"
#include "stdio.h"

/**********************************************************************
 * 函数名称： Led_Init
 * 功能描述： LED初始化函数, 在HAL的初始化代码里已经配置好了GPIO引脚, 
 *            所以本函数可以写为空
 *            但是为了不依赖于stm32cubemx, 此函数也实现了GPIO的配置
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int Led_Init(void)
{
   /*在main.c GPIO.init()初始化*/
    
    return 0;
}

/**********************************************************************
 * 函数名称： Led_Control
 * 功能描述： LED控制函数
 * 输入参数： which-哪个LED, 在driver_led.h里定义, 比如LED_GREEN
 *            on-状态, 1-亮, 0-灭
 * 输出参数： 无
 * 返 回 值： 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int Led_Control(int which, int on)
{
    if (on)
    {
        switch (which)
        {
        case LED_1:
             HAL_GPIO_WritePin(GPIOA, led1_Pin, GPIO_PIN_SET);
             break;
        case LED_2:
            /* code */
            HAL_GPIO_WritePin(GPIOA, led2_Pin, GPIO_PIN_SET);
            break;
        case LED_3:
            /* code */
            HAL_GPIO_WritePin(GPIOA, led3_Pin, GPIO_PIN_SET);
            break;
        case LED_4:
            /* code */
            HAL_GPIO_WritePin(GPIOA, led4_Pin, GPIO_PIN_SET);
            break;
        default:
            printf("LED_Control: invalid LED index\n");
            break;
        }
    }
    else
    {
        switch (which)
        {
        case LED_1:
             HAL_GPIO_WritePin(GPIOA, led1_Pin, GPIO_PIN_RESET);
             break;
        case LED_2:
            /* code */
            HAL_GPIO_WritePin(GPIOA, led2_Pin, GPIO_PIN_RESET);
            break;
        case LED_3:
            /* code */
            HAL_GPIO_WritePin(GPIOA, led3_Pin, GPIO_PIN_RESET);
            break;
        case LED_4:
            /* code */
            HAL_GPIO_WritePin(GPIOA, led4_Pin, GPIO_PIN_RESET);
            break;
        default:
            printf("LED_Control: invalid LED index\n");
            break;
        }
    }
    return 0;
}

/**********************************************************************
 * 函数名称： Led_Test
 * 功能描述： Led测试程序
 * 输入参数： 无
 * 输出参数： 无
 *            无
 * 返 回 值： 0 - 成功, 其他值 - 失败
 ***********************************************************************/
void Led_Test(void)
{
    Led_Init();

    while (1)
    {
        Led_Control(LED_1, 1);
        Led_Control(LED_2, 1);
        Led_Control(LED_3, 1);
        Led_Control(LED_4, 1);
        HAL_Delay(500);

        Led_Control(LED_1, 0);
        Led_Control(LED_2, 0);
        Led_Control(LED_3, 0);
        Led_Control(LED_4, 0);
        HAL_Delay(500);
    }
}

