#ifndef MAIN_UI_H
#define MAIN_UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize main UI with container1 (dummy_obj) and container2 (tabview)
 * 
 * Creates:
 * - container1: Full screen dummy_obj for detecting LV_EVENT_CLICKED
 * - container2: Tabview with two pages (timer_container and range_container)
 */
void create_main_ui(void);

/**
 * @brief Get timer container (for timer_ui functions)
 */
lv_obj_t *main_ui_get_timer_container(void);

/**
 * @brief Get range container (for timer_range_ui functions)
 */
lv_obj_t *main_ui_get_range_container(void);

#ifdef __cplusplus
}
#endif

#endif // MAIN_UI_H

