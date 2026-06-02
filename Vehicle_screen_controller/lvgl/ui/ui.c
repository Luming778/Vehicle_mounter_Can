#include "ui.h"

static int lvgl_cnt = 0;
static lv_obj_t *lvgl_cnt_label;

static void lvgl_btn_click_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    lvgl_cnt++;
    lv_label_set_text_fmt(lvgl_cnt_label, "Taps: %d", lvgl_cnt);
}

void ui_init(void)
{
    lv_obj_t *scr = lv_screen_active();

    /* Title label */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL on STM32F103");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    /* Info label */
    lv_obj_t *info = lv_label_create(scr);
    lv_label_set_text(info, "ATK-MD0280 240x320\nFSMC Interface\nFreeRTOS + LVGL v9");
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info, LV_ALIGN_CENTER, 0, -20);

    /* A button to test touch */
    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Touch Me");

    /* Counter label */
    lvgl_cnt_label = lv_label_create(scr);
    lv_label_set_text_fmt(lvgl_cnt_label, "Taps: %d", lvgl_cnt);
    lv_obj_align(lvgl_cnt_label, LV_ALIGN_BOTTOM_MID, 0, -100);

    /* Button click event */
    lv_obj_add_event_cb(btn, lvgl_btn_click_cb, LV_EVENT_CLICKED, NULL);
}
