
/**
 * @file lv_port_lcd_stm32.h
 * @brief LVGL LCD display port header for STM32F103 + ATK-MD0280
 */

/* Copy this file as "lv_port_disp.h" and set this value to "1" to enable content */
#if 1

#ifndef LV_PORT_LCD_STM32_H
#define LV_PORT_LCD_STM32_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/*********************
 *      DEFINES
 *********************/
#define LVGL_LCD_HOR_RES    240
#define LVGL_LCD_VER_RES    320

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/** Initialize LVGL display driver for ATK-MD0280 LCD */
void lv_port_disp_init(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_PORT_LCD_STM32_H */

#endif /* Disable/Enable content */
