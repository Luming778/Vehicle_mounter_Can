/**
 * @file ui.c
 * @brief Car dashboard status panel - shows lights, doors, windows, sunroof
 *
 * Design: Dark-themed dashboard, 320x480, 2x2 status card grid.
 * Optimized: minimal objects, 2 font sizes only, no overlay.
 */

#include "ui.h"
#include "can.h"

/*********************
 *      DEFINES
 *********************/
#define PANEL_W   94
#define PANEL_H   90
#define GRID_GAP  10
#define MARGIN_X  15
#define GRID_TOP  50
#define HEADER_H  40
#define FOOTER_Y  260

/* Brightness: store current value 10~100 */
static int32_t s_brightness = 80;

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t      *s_dim_overlay;   /* Full-screen overlay for brightness */
static status_panel_t s_panel_light;
static status_panel_t s_panel_door;
static status_panel_t s_panel_win;
static status_panel_t s_panel_sunroof;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void create_header(lv_obj_t *parent);
static void create_footer(lv_obj_t *parent);
static void create_panel(status_panel_t *p, lv_obj_t *parent,
                         lv_coord_t x, lv_coord_t y,
                         const char *symbol, const char *title);
static void update_panel(status_panel_t *p, bool active);
static void panel_click_cb(lv_event_t *e);
static void brightness_cb(lv_event_t *e);

/*--------------------*
 *  HELPER: Create header (no extra line objects - uses border)
 *--------------------*/
static void create_header(lv_obj_t *parent)
{
    lv_obj_t *hdr = lv_obj_create(parent);
    lv_obj_set_size(hdr, 240, HEADER_H);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(hdr, CLR_BG, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    /* Bottom divider via border - no extra lv_line object */
    lv_obj_set_style_border_color(hdr, CLR_DIVIDER, 0);
    lv_obj_set_style_border_width(hdr, 1, 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    /* Car icon */
    lv_obj_t *car_icon = lv_label_create(hdr);
    lv_label_set_text(car_icon, LV_SYMBOL_DRIVE);
    lv_obj_set_style_text_color(car_icon, lv_color_hex(0x3498DB), 0);
    lv_obj_set_style_text_font(car_icon, &lv_font_montserrat_14, 0);
    lv_obj_align(car_icon, LV_ALIGN_LEFT_MID, 8, 0);

    /* Title */
    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, "CAR STATUS");
    lv_obj_set_style_text_color(title, CLR_TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 32, 0);

    /* WiFi symbol */
    lv_obj_t *wifi = lv_label_create(hdr);
    lv_label_set_text(wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi, CLR_TEXT, 0);
    lv_obj_set_style_text_font(wifi, &lv_font_montserrat_14, 0);
    lv_obj_align(wifi, LV_ALIGN_RIGHT_MID, -8, 0);
}

/*--------------------*
 *  HELPER: Create footer (brightness slider only)
 *--------------------*/
static void create_footer(lv_obj_t *parent)
{
    lv_obj_t *foot = lv_obj_create(parent);
    lv_obj_set_size(foot, 240, 50);
    lv_obj_align(foot, LV_ALIGN_TOP_MID, 0, FOOTER_Y);
    lv_obj_set_style_bg_color(foot, CLR_BG, 0);
    lv_obj_set_style_bg_opa(foot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(foot, 0, 0);
    lv_obj_set_style_pad_all(foot, 0, 0);
    lv_obj_set_style_radius(foot, 0, 0);
    /* Top divider via border */
    lv_obj_set_style_border_color(foot, CLR_DIVIDER, 0);
    lv_obj_set_style_border_width(foot, 1, 0);
    lv_obj_set_style_border_side(foot, LV_BORDER_SIDE_TOP, 0);
    lv_obj_clear_flag(foot, LV_OBJ_FLAG_SCROLLABLE);

    /* Battery icon */
    lv_obj_t *bat = lv_label_create(foot);
    lv_label_set_text(bat, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(bat, CLR_STATE_ON, 0);
    lv_obj_set_style_text_font(bat, &lv_font_montserrat_14, 0);
    lv_obj_align(bat, LV_ALIGN_LEFT_MID, 10, 0);

    /* Brightness icon */
    lv_obj_t *bright_icon = lv_label_create(foot);
    lv_label_set_text(bright_icon, LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_color(bright_icon, CLR_TEXT, 0);
    lv_obj_set_style_text_font(bright_icon, &lv_font_montserrat_14, 0);
    lv_obj_align(bright_icon, LV_ALIGN_RIGHT_MID, -100, 0);

    /* Brightness slider */
    lv_obj_t *slider = lv_slider_create(foot);
    lv_obj_set_size(slider, 80, 6);
    lv_obj_align(slider, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_slider_set_range(slider, 10, 100);
    lv_slider_set_value(slider, s_brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, brightness_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Slider track */
    lv_obj_set_style_bg_color(slider, CLR_DIVIDER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_all(slider, 0, LV_PART_MAIN);

    /* Slider indicator */
    lv_obj_set_style_bg_color(slider, CLR_STATE_ON, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 3, LV_PART_INDICATOR);

    /* Slider knob */
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, 6, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);
}

/*--------------------*
 *  HELPER: Apply brightness to screen background
 *--------------------*/
static void apply_brightness(void)
{
    /* val 100 = full bright (overlay transparent), val 10 = dim (overlay dark) */
    lv_opa_t opa = (lv_opa_t)(255 - s_brightness * 255 / 100);
    lv_obj_set_style_bg_opa(s_dim_overlay, opa, 0);
}

/*--------------------*
 *  HELPER: Create one status panel
 *  Uses border_top for accent bar - no extra child object
 *--------------------*/
static void create_panel(status_panel_t *p, lv_obj_t *parent,
                         lv_coord_t x, lv_coord_t y,
                         const char *symbol, const char *title)
{
    p->active = false;

    /* Panel container - border_top acts as accent bar */
    p->panel = lv_obj_create(parent);
    lv_obj_set_size(p->panel, PANEL_W, PANEL_H);
    lv_obj_set_pos(p->panel, x, y);
    lv_obj_set_style_bg_color(p->panel, CLR_CARD, 0);
    lv_obj_set_style_bg_opa(p->panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(p->panel, 10, 0);
    lv_obj_set_style_pad_all(p->panel, 0, 0);
    /* Accent bar via top border */
    lv_obj_set_style_border_color(p->panel, CLR_STATE_OFF, 0);
    lv_obj_set_style_border_width(p->panel, 3, 0);
    lv_obj_set_style_border_side(p->panel, LV_BORDER_SIDE_TOP, 0);
    lv_obj_clear_flag(p->panel, LV_OBJ_FLAG_SCROLLABLE);

    /* Click support */
    lv_obj_add_flag(p->panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(p->panel, panel_click_cb, LV_EVENT_CLICKED, p);

    /* Symbol icon */
    p->icon = lv_label_create(p->panel);
    lv_label_set_text(p->icon, symbol);
    lv_obj_set_style_text_color(p->icon, CLR_STATE_OFF, 0);
    lv_obj_set_style_text_font(p->icon, &lv_font_montserrat_16, 0);
    lv_obj_align(p->icon, LV_ALIGN_TOP_MID, 0, 12);

    /* Title label */
    p->label = lv_label_create(p->panel);
    lv_label_set_text(p->label, title);
    lv_obj_set_style_text_color(p->label, CLR_TEXT, 0);
    lv_obj_set_style_text_font(p->label, &lv_font_montserrat_14, 0);
    lv_obj_align(p->label, LV_ALIGN_TOP_MID, 0, 40);

    /* Status label */
    p->status = lv_label_create(p->panel);
    lv_label_set_text(p->status, "OFF");
    lv_obj_set_style_text_color(p->status, CLR_STATE_OFF, 0);
    lv_obj_set_style_text_font(p->status, &lv_font_montserrat_14, 0);
    lv_obj_align(p->status, LV_ALIGN_TOP_MID, 0, 60);
}

/*--------------------*
 *  HELPER: Update panel state
 *--------------------*/
static void update_panel(status_panel_t *p, bool active)
{
    p->active = active;
    lv_color_t accent = active ? CLR_STATE_ON : CLR_STATE_OFF;

    lv_obj_set_style_bg_color(p->panel, active ? CLR_CARD_SEL : CLR_CARD, 0);
    lv_obj_set_style_border_color(p->panel, accent, 0);
    lv_obj_set_style_text_color(p->icon, accent, 0);
    lv_label_set_text(p->status, active ? "ON" : "OFF");
    lv_obj_set_style_text_color(p->status, accent, 0);
}

/*--------------------*
 *  EVENT: Panel click toggles state
 *--------------------*/
static void panel_click_cb(lv_event_t *e)
{
    status_panel_t *p = (status_panel_t *)lv_event_get_user_data(e);
    if(!p) return;

    bool new_state = !p->active;
    update_panel(p, new_state);

    /* Send CAN command based on which panel was clicked */
    uint8_t cmd = 0;
    uint16_t can_id = 0;

    if (p == &s_panel_light) {
        cmd = new_state ? 0x01 : 0x02;  /* 车灯开/关 */
        can_id = 0x77F;
    } else if (p == &s_panel_door) {
        cmd = new_state ? 0x07 : 0x08;  /* 车门开/关 */
        can_id = 0x7BF;
    } else if (p == &s_panel_win) {
        cmd = new_state ? 0x03 : 0x04;  /* 车窗开/关 */
        can_id = 0x77F;
    } else if (p == &s_panel_sunroof) {
        cmd = new_state ? 0x05 : 0x06;  /* 天窗开/关 */
        can_id = 0x77F;
    }

    if (cmd != 0) {
        uint8_t data[3] = {0xAA, 0x11, cmd};
        CAN_SendMsg(can_id, data, 3);
    }
}

/*--------------------*
 *  EVENT: Brightness slider changed
 *--------------------*/
static void brightness_cb(lv_event_t *e)
{
    s_brightness = lv_slider_get_value(lv_event_get_target(e));
    apply_brightness();
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void car_dashboard_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, CLR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    create_header(scr);

    lv_coord_t gx = MARGIN_X;
    lv_coord_t gy = GRID_TOP;

    create_panel(&s_panel_light, scr, gx, gy,
                 LV_SYMBOL_EYE_OPEN, "LIGHTS");
    create_panel(&s_panel_door, scr, gx + PANEL_W + GRID_GAP, gy,
                 LV_SYMBOL_LIST, "DOORS");
    create_panel(&s_panel_win, scr, gx, gy + PANEL_H + GRID_GAP,
                 LV_SYMBOL_IMAGE, "WINDOWS");
    create_panel(&s_panel_sunroof, scr, gx + PANEL_W + GRID_GAP, gy + PANEL_H + GRID_GAP,
                 LV_SYMBOL_DOWN, "SUNROOF");

    create_footer(scr);

    /* Dim overlay: last child = topmost, covers everything */
    s_dim_overlay = lv_obj_create(scr);
    lv_obj_set_size(s_dim_overlay, 240, 320);
    lv_obj_align(s_dim_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_dim_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_dim_overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_dim_overlay, 0, 0);
    lv_obj_set_style_radius(s_dim_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_dim_overlay, 0, 0);
    lv_obj_clear_flag(s_dim_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_dim_overlay, LV_OBJ_FLAG_CLICKABLE);

    /* Apply initial brightness */
    apply_brightness();
}

/*--------------------*
 *  Wrapper: ui_init
 *--------------------*/
void ui_init(void)
{
    car_dashboard_create();
}

/*--------------------*
 *  Update UI from CAN message
 *--------------------*/
void ui_update_status(uint8_t cmd, bool active)
{
    switch (cmd) {
        case 0x01:  /* 车灯开 */
        case 0x02:  /* 车灯关 */
            update_panel(&s_panel_light, active);
            break;
        case 0x03:  /* 车窗开 */
        case 0x04:  /* 车窗关 */
            update_panel(&s_panel_win, active);
            break;
        case 0x05:  /* 天窗开 */
        case 0x06:  /* 天窗关 */
            update_panel(&s_panel_sunroof, active);
            break;
        case 0x07:  /* 车门开 */
        case 0x08:  /* 车门关 */
            update_panel(&s_panel_door, active);
            break;
        default:
            break;
    }
}
