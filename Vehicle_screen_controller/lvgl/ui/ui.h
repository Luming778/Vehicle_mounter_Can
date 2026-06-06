#ifndef UI_H
#define UI_H

#include "lvgl.h"

/*********************
 *      DEFINES
 *********************/
/* Dark theme colors */
#define CLR_BG          lv_color_hex(0x1A1A2E)   /* Background */
#define CLR_CARD        lv_color_hex(0x16213E)   /* Card background */
#define CLR_CARD_SEL    lv_color_hex(0x1A2744)   /* Card background when active */
#define CLR_TEXT        lv_color_hex(0xEAEAEA)   /* Primary text */
#define CLR_DIVIDER     lv_color_hex(0x2C3E6B)   /* Divider lines */

/* Status accent colors - unified */
#define CLR_STATE_OFF   lv_color_hex(0x5A6070)   /* Gray - off / closed */
#define CLR_STATE_ON    lv_color_hex(0x2ECC71)   /* Green - on / open */

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t *panel;
    lv_obj_t *icon;
    lv_obj_t *label;
    lv_obj_t *status;
    bool      active;
} status_panel_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief Create the car dashboard UI (called from main)
 */
void car_dashboard_create(void);

/**
 * @brief Initialize UI (wrapper for car_dashboard_create)
 */
void ui_init(void);

/**
 * @brief Update device status from CAN message
 * @param cmd  Command byte (0x01-0x08)
 * @param active  true=ON, false=OFF
 */
void ui_update_status(uint8_t cmd, bool active);

#endif /* UI_H */
