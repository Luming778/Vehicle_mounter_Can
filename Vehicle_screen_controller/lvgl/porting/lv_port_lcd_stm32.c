/**
 * @file lv_port_lcd_stm32.c
 * @brief LVGL LCD display driver for ATK-MD0280 (ILI9341) via FSMC on STM32F103
 *
 * Hardware: STM32F103 + ATK-MD0280 LCD module
 * Interface: FSMC Bank 4, 16-bit parallel, RGB565
 * Resolution: 240 x 320
 */

/* Copy this file as "lv_port_disp.c" and set this value to "1" to enable content */
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_lcd_stm32.h"
#include "atk_md0280.h"
#include "atk_md0280_fsmc.h"

/*********************
 *      DEFINES
 *********************/
#define MY_DISP_HOR_RES     240
#define MY_DISP_VER_RES     320

/* Partial buffer: reduce lines to fit STM32F103 64KB SRAM
   (240 * 20 * 2 bytes * 2 buffers = 19.2KB, + LV_MEM_SIZE 32KB ≈ 51KB total) */
// #define LVGL_BUF_LINES      15

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lcd_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /* ATK-MD0280 LCD is already initialized by atk_md0280_init() called in main */
    /* Just set up the LVGL display interface */

    /* Create the LVGL display object */
    lv_display_t *disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    if (disp == NULL) {
        return;
    }

    /* Set display rotation: 0° (portrait) */
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);

    /* Set color format to RGB565 */
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

    /* Allocate single draw buffer for partial rendering */
    /* Only 1/20 screen to save RAM: 240*16*2 = 7.68KB */
    static lv_color_t buf1[MY_DISP_HOR_RES * 16];

    lv_display_set_buffers(disp,
                           buf1,
                           NULL,                     /* single buffer */
                           sizeof(buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Register flush callback */
    lv_display_set_flush_cb(disp, lcd_flush_cb);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief LVGL flush callback: send pixel data from px_map to LCD via FSMC
 * @param disp   Display object
 * @param area   Area to update
 * @param px_map Pixel data buffer (RGB565 format)
 */
static void lcd_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    uint32_t size = w * h;

    /* Set column address (X range) */
    atk_md0280_fsmc_write_cmd(0x2A);
    atk_md0280_fsmc_write_dat((area->x1 >> 8) & 0xFF);
    atk_md0280_fsmc_write_dat(area->x1 & 0xFF);
    atk_md0280_fsmc_write_dat((area->x2 >> 8) & 0xFF);
    atk_md0280_fsmc_write_dat(area->x2 & 0xFF);

    /* Set page address (Y range) */
    atk_md0280_fsmc_write_cmd(0x2B);
    atk_md0280_fsmc_write_dat((area->y1 >> 8) & 0xFF);
    atk_md0280_fsmc_write_dat(area->y1 & 0xFF);
    atk_md0280_fsmc_write_dat((area->y2 >> 8) & 0xFF);
    atk_md0280_fsmc_write_dat(area->y2 & 0xFF);

    /* Start memory write */
    atk_md0280_fsmc_write_cmd(0x2C);

    /* Write pixel data via FSMC 16-bit parallel interface */
    uint16_t *color_p = (uint16_t *)px_map;
    for (uint32_t i = 0; i < size; i++) {
        ATK_MD0280_FSMC_DAT_REG = color_p[i];
    }

    /* Notify LVGL that flushing is complete */
    lv_display_flush_ready(disp);
}

#else /* Enable this file at the top */

/* This dummy typedef exists purely to silence -Wpedantic. */
typedef int keep_pedantic_happy;
#endif
