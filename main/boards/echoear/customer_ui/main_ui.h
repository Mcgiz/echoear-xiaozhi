#ifndef MAIN_UI_H
#define MAIN_UI_H

#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"
#include "../config.h"

#define SCREEN_WIDTH    DISPLAY_WIDTH
#define SCREEN_HEIGHT   DISPLAY_HEIGHT

#ifdef __cplusplus
class Display;
extern "C" {
#endif

/* Page type enum */
typedef enum {
    MAIN_UI_PAGE_DUMMY = 0,
    MAIN_UI_PAGE_POMODORO,
    MAIN_UI_PAGE_SLEEP,
    MAIN_UI_PAGE_TIME_UP,
} main_ui_page_t;

// Font declarations
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(lv_font_montserrat_40);

// Image declarations
LV_IMG_DECLARE(time_start);
LV_IMG_DECLARE(time_end);
LV_IMG_DECLARE(time_sleep);
LV_IMG_DECLARE(time_arc_texture);
LV_IMG_DECLARE(clock_loop);
LV_IMG_DECLARE(watch_bg);

/**
 * @brief Initialize main UI with container1 (dummy_obj) and container2 (tabview)
 *
 * Creates:
 * - container1: Full screen dummy_obj for detecting LV_EVENT_CLICKED
 * - container2: Tabview with two pages (container_pomodoro and container_sleep)
 */
void create_main_ui(Display *display);

/**
 * @brief Show pomodoro timer UI (make it visible)
 */
void alarm_pomodoro_show(void);

/**
 * @brief Hide pomodoro timer UI (make it invisible)
 */
void alarm_pomodoro_hide(void);

/**
 * @brief Show alarm time up UI (make it visible)
 */
void alarm_time_up_show(void);

/**
 * @brief Hide alarm time up UI (make it invisible)
 */
void alarm_time_up_hide(void);

/* Internal functions - declared here for internal use only */
/* External modules should use customer_ui_api.h instead */
void main_ui_show_pomodoro_with_minutes(int32_t minutes);
void main_ui_show_dummy_page(void);
void main_ui_swipe_event_cb(lv_event_t *e);
size_t main_ui_get_assets_size(void);
const uint8_t* main_ui_get_assets_data(void);
main_ui_page_t main_ui_get_current_page(void);

#ifdef __cplusplus
}
#endif

#endif // MAIN_UI_H
