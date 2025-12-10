#ifndef MAIN_UI_H
#define MAIN_UI_H

#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"
#include "../config.h"

#define SCREEN_WIDTH    DISPLAY_WIDTH
#define SCREEN_HEIGHT   DISPLAY_HEIGHT

#ifdef __cplusplus
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
void create_main_ui(void);

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

/**
 * @brief Show pomodoro page with specified minutes (default 5 min if <= 0)
 *
 * This will:
 *  - reset pomodoro timer to 0
 *  - set remaining time to `minutes` (or 5 if minutes <= 0)
 *  - show pomodoro page
 */
void main_ui_show_pomodoro_with_minutes(int32_t minutes);

/**
 * @brief Switch back to dummy draw page (home screen overlay)
 */
void main_ui_show_dummy_page(void);

/**
 * @brief Global swipe handler callback for child widgets
 *
 * Can be attached to any LVGL object (e.g. child controls inside pomodoro/sleep UIs)
 * to enable page left/right swipe even if the touch starts on that control.
 */
void main_ui_swipe_event_cb(lv_event_t *e);

/**
 * @brief Get assets size
 * @return Size of the clock loop EAF asset in bytes
 */
size_t main_ui_get_assets_size(void);

/**
 * @brief Get assets data
 * @return Pointer to the clock loop EAF asset data
 */
const uint8_t* main_ui_get_assets_data(void);

/**
 * @brief Get current page type
 * @return Current page enum value (MAIN_UI_PAGE_DUMMY, MAIN_UI_PAGE_POMODORO, etc.)
 */
main_ui_page_t main_ui_get_current_page(void);

#ifdef __cplusplus
}
#endif

#endif // MAIN_UI_H
