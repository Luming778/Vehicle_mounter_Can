/**
 ****************************************************************************************************
 * @file        atk_md0280_touch_spi.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-06-21
 * @brief       ATK-MD0280模块触摸SPI接口驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32F103开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#include "atk_md0280_touch_spi.h"
#include "FreeRTOS.h"
#include "task.h"

#if (ATK_MD0280_USING_TOUCH != 0)

/**
 * @brief       微秒延时（适配 FreeRTOS）
 * @param       us: 延时时间（微秒）
 * @retval      无
 * @note        当延时 >= 1ms 且 FreeRTOS 调度器已运行时，使用 vTaskDelay 实现非阻塞延时；
 *              不足 1ms 的部分使用 DWT 周期计数器忙等，以保证 SPI 时序精度
 */
void delay_us(uint32_t us)
{
    if (us == 0)
    {
        return;
    }

    /* FreeRTOS 已启动且延时 >= 1ms 时，使用任务延时（非阻塞） */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED && us >= 1000)
    {
        vTaskDelay(pdMS_TO_TICKS(us / 1000));
        us %= 1000;
    }

    /* 剩余微秒级部分使用 DWT 周期计数器忙等 */
    if (us > 0)
    {
        /* 使能 DWT CYCCNT（STM32F103 内核支持） */
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

        uint32_t ticks = us * (SystemCoreClock / 1000000);
        uint32_t start = DWT->CYCCNT;
        while ((DWT->CYCCNT - start) < ticks)
        {
        }
    }
}

/**
 * @brief       ATK-MD0280模块触摸SPI接口写数据
 * @param       待写入的数据
 * @retval      无
 */
static void atk_md0280_touch_spi_write(uint8_t dat)
{
    uint8_t dat_index;
    
    for (dat_index=0; dat_index<8; dat_index++)
    {
        if ((dat & 0x80) != 0)
        {
            ATK_MD0280_TOUCH_SPI_MO(1);
        }
        else
        {
            ATK_MD0280_TOUCH_SPI_MO(0);
        }
        
        dat <<= 1;
        ATK_MD0280_TOUCH_SPI_CLK(0);
        delay_us(1);
        ATK_MD0280_TOUCH_SPI_CLK(1);
    }
}

/**
 * @brief       ATK-MD0280模块触摸SPI接口初始化
 * @param       无
 * @retval      无
 */
void atk_md0280_touch_spi_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};
    
    /* 使能时钟 */
    ATK_MD0280_TOUCH_SPI_MI_GPIO_CLK_ENABLE();
    ATK_MD0280_TOUCH_SPI_MO_GPIO_CLK_ENABLE();
    ATK_MD0280_TOUCH_SPI_TCS_GPIO_CLK_ENABLE();
    ATK_MD0280_TOUCH_SPI_CLK_GPIO_CLK_ENABLE();
    
    /* 初始化MI引脚 */
    gpio_init_struct.Pin    = ATK_MD0280_TOUCH_SPI_MI_GPIO_PIN;
    gpio_init_struct.Mode   = GPIO_MODE_INPUT;
    gpio_init_struct.Pull   = GPIO_PULLUP;
    gpio_init_struct.Speed  = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ATK_MD0280_TOUCH_SPI_MI_GPIO_PORT, &gpio_init_struct);
    
    /* 初始化MO引脚 */
    gpio_init_struct.Pin    = ATK_MD0280_TOUCH_SPI_MO_GPIO_PIN;
    gpio_init_struct.Mode   = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull   = GPIO_PULLUP;
    gpio_init_struct.Speed  = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ATK_MD0280_TOUCH_SPI_MO_GPIO_PORT, &gpio_init_struct);
    
    /* 初始化TCS引脚 */
    gpio_init_struct.Pin    = ATK_MD0280_TOUCH_SPI_TCS_GPIO_PIN;
    gpio_init_struct.Mode   = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull   = GPIO_PULLUP;
    gpio_init_struct.Speed  = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ATK_MD0280_TOUCH_SPI_TCS_GPIO_PORT, &gpio_init_struct);
    
    /* 初始化CLK引脚 */
    gpio_init_struct.Pin    = ATK_MD0280_TOUCH_SPI_CLK_GPIO_PIN;
    gpio_init_struct.Mode   = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull   = GPIO_PULLUP;
    gpio_init_struct.Speed  = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ATK_MD0280_TOUCH_SPI_CLK_GPIO_PORT, &gpio_init_struct);
}

/**
 * @brief       ATK-MD0280模块触摸SPI接口读数据
 * @param       无
 * @retval      读出的数据
 */
uint16_t atk_md0280_touch_spi_read(uint8_t cmd)
{
    uint16_t dat = 0;
    uint8_t dat_index;
    
    ATK_MD0280_TOUCH_SPI_CLK(0);
    ATK_MD0280_TOUCH_SPI_MO(0);
    ATK_MD0280_TOUCH_SPI_TCS(0);
    atk_md0280_touch_spi_write(cmd);
    delay_us(6);
    ATK_MD0280_TOUCH_SPI_CLK(0);
    delay_us(1);
    ATK_MD0280_TOUCH_SPI_CLK(1);
    delay_us(1);
    ATK_MD0280_TOUCH_SPI_CLK(0);
    
    for (dat_index=0; dat_index<16; dat_index++)
    {
        dat <<= 1;
        ATK_MD0280_TOUCH_SPI_CLK(0);
        delay_us(1);
        ATK_MD0280_TOUCH_SPI_CLK(1);
        if (ATK_MD0280_TOUCH_SPI_READ_MI() != 0)
        {
            dat++;
        }
    }
    
    dat >>= 4;
    ATK_MD0280_TOUCH_SPI_TCS(1);
    
    return dat;
}

#endif /* ATK_MD0280_USING_TOUCH */
