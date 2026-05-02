
#include "driver_key.h"
#include "driver_led.h"
#include "gpio.h"

#define KEY_GPIO_GROUP GPIOB
#define KEY_GPIO_PIN   GPIO_PIN_14


/**********************************************************************
 * 函数名称： Key_Init
 * 功能描述： 按键初始化函数
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 0 - 成功, 其他值 - 失败
 ***********************************************************************/
void Key_Init(void)
{
    /*在main.c GPIO.init()初始化*/
   
}

/**********************************************************************
 * 函数名称： Key_Read
 * 功能描述： 按键读取函数
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 1 - 按键被按键, 0 - 按键被松开
 ***********************************************************************/
uint8_t Key_Read(void)
{
	uint8_t KeyNum = 0;
	if (HAL_GPIO_ReadPin(GPIOA, key1_Pin) == 0)
	{
		HAL_Delay(10);
		while (HAL_GPIO_ReadPin(GPIOA, key1_Pin) == 0);
		HAL_Delay(10);
		KeyNum = 1;
	}
	if (HAL_GPIO_ReadPin(GPIOB, key2_Pin) == 0)
	{
		HAL_Delay(10);
		while (HAL_GPIO_ReadPin(GPIOB, key2_Pin) == 0);
		HAL_Delay(10);
		KeyNum = 2;
	}
	if (HAL_GPIO_ReadPin(GPIOB, key3_Pin) == 0)
	{
			HAL_Delay(10);
			while (HAL_GPIO_ReadPin(GPIOB, key3_Pin) == 0);
			HAL_Delay(10);
			KeyNum = 3;
	}
	if (HAL_GPIO_ReadPin(GPIOB, key4_Pin) == 0)
	{
			HAL_Delay(10);
			while (HAL_GPIO_ReadPin(GPIOB, key4_Pin) == 0);
			HAL_Delay(10);
			KeyNum = 4;
	}
	return KeyNum;
}

/**********************************************************************
 * 函数名称： Key_Test
 * 功能描述： 按键测试程序
 * 输入参数： 无
 * 输出参数： 无
 *            无
 * 返 回 值： 0 - 成功, 其他值 - 失败
 ***********************************************************************/
void Key_Test(void)
{
		int state = 0;
    int val = 0;
	 while(1)
	 {
     val = Key_Read();
    switch (val)
    {
    case 1:
        state ^= 1;  // 切换状态
        Led_Control(LED_1, state);
        break;
    case 2:
        state ^= 1;  // 切换状态
        Led_Control(LED_2, state);
        break;
    case 3:
        state ^= 1;  // 切换状态
        Led_Control(LED_3, state);
        break;
    case 4:
        state ^= 1;  // 切换状态
        Led_Control(LED_4, state);
        break;
    default:
        break;
    }
	 }				 


}

