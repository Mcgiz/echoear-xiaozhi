#include "main_ui.h"
#include "timer_ui.h"
#include "timer_range_ui.h"
#include "../config.h"
#include <esp_log.h>
#include <lvgl.h>
#include <esp_lv_adapter.h>

#define TAG "MainUI"

static lv_obj_t *dummy_obj = NULL;
static lv_obj_t *tabview_obj = NULL;
static lv_obj_t *timer_container = NULL;
static lv_obj_t *range_container = NULL;

static void dummy_monitor_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    lv_display_t *disp = lv_display_get_default();
    if (disp == nullptr) {
        return;
    }

    ESP_LOGI(TAG, "disable dummy draw");
    esp_lv_adapter_set_dummy_draw(disp, false);
    lv_obj_add_flag(dummy_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(timer_container, LV_OBJ_FLAG_HIDDEN);
}

static void create_dummy_monitor(void)
{
    lv_obj_t *scr = lv_scr_act();
    
    // Create full screen dummy_obj on top
    dummy_obj = lv_obj_create(scr);
    lv_obj_remove_style_all(dummy_obj);
    lv_obj_set_size(dummy_obj, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_align(dummy_obj, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(dummy_obj, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(dummy_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(dummy_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(dummy_obj, dummy_monitor_event_cb, LV_EVENT_CLICKED, NULL);
    
    ESP_LOGI(TAG, "Dummy monitor layer created");
}

static void create_timer_tabview(void)
{
    lv_obj_t *scr = lv_scr_act();
#if 0
    tabview_obj = lv_tabview_create(scr);
    lv_obj_set_size(tabview_obj, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(tabview_obj, lv_color_black(), 0);
    lv_obj_set_style_border_width(tabview_obj, 0, 0);
    lv_obj_set_style_pad_all(tabview_obj, 0, 0);
    
    // Hide tab buttons (we'll use swipe to switch)
    lv_tabview_set_tab_bar_position(tabview_obj, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabview_obj, 0);  // Hide tab bar
    
    lv_obj_t *timer_tab = lv_tabview_add_tab(tabview_obj, "");
    timer_container = lv_obj_create(timer_tab);
    lv_obj_set_size(timer_container, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(timer_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(timer_container, 0, 0);
    lv_obj_set_style_pad_all(timer_container, 0, 0);
    create_timer_ui_with_parent(timer_container);
    
    lv_obj_t *range_tab = lv_tabview_add_tab(tabview_obj, "");
    range_container = lv_obj_create(range_tab);
    lv_obj_set_size(range_container, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(range_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(range_container, 0, 0);
    lv_obj_set_style_pad_all(range_container, 0, 0);
    create_timer_range_ui_with_parent(range_container);
#else
    timer_container = lv_obj_create(scr);
    lv_obj_set_size(timer_container, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(timer_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(timer_container, 0, 0);
    lv_obj_set_style_pad_all(timer_container, 0, 0);
    create_timer_ui_with_parent(timer_container);
#endif
}

void create_main_ui(void)
{
    lv_display_t *disp = lv_display_get_default();
    if (disp == nullptr) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);

    create_timer_tabview();
    create_dummy_monitor();

    lv_obj_move_foreground(dummy_obj);

    lv_obj_clear_flag(dummy_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(timer_container, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "Main UI created");
}

lv_obj_t *main_ui_get_timer_container(void)
{
    return timer_container;
}

lv_obj_t *main_ui_get_range_container(void)
{
    return range_container;
}

