/**
 * @file lv_port_indev.c
 * @brief LVGL input device port using ATK-MD0280 resistive touch
 *
 * Hardware: STM32F103 + ATK-MD0280 touch (XPT2046 via SPI)
 */

/* Copy this file as "lv_port_indev.c" and set this value to "1" to enable content */
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "atk_md0280.h"
#include "atk_md0280_touch.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void touchpad_init(void);
static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_indev_t *indev_touchpad;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void)
{
    /*------------------
     * Touchpad (Resistive)
     * -----------------*/

    /* Initialize touch hardware */
    touchpad_init();

    /* Register a touchpad input device */
    indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, touchpad_read);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*------------------
 * Touchpad
 * -----------------*/

/**
 * @brief Initialize the touchpad hardware
 * @note  ATK-MD0280 touch is initialized by atk_md0280_init() ->
 *        atk_md0280_touch_init(), which includes calibration.
 *        This function is a placeholder since HW init occurs there.
 */
static void touchpad_init(void)
{
    /* Touch controller (XPT2046) is already initialized by atk_md0280_touch_init()
     * called within atk_md0280_init() in main. Nothing to do here. */
}

/**
 * @brief LVGL callback: read touchpad state and coordinates
 * @param indev  Input device object
 * @param data   Output data structure to fill
 */
static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    static int32_t last_x = 0;
    static int32_t last_y = 0;

    uint16_t x, y;

    /* Use the ATK-MD0280 touch scan function */
    if (atk_md0280_touch_scan(&x, &y) == ATK_MD0280_TOUCH_EOK)
    {
        last_x = (int32_t)x;
        last_y = (int32_t)y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    /* Set the last known coordinates */
    data->point.x = last_x;
    data->point.y = last_y;
}

#else /* Enable this file at the top */

/* This dummy typedef exists purely to silence -Wpedantic. */
typedef int keep_pedantic_happy;
#endif
