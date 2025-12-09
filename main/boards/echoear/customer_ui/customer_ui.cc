#include "customer_ui.h"
#include "../config.h"
#include <esp_log.h>
#include <lvgl.h>
#include <esp_lv_adapter.h>
#include "timer_ui.h"

#define TAG "CustomerUI"

static uint8_t dummy_count = 0;

static void screen_touch_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    lv_display_t *disp = lv_display_get_default();
    if (disp == nullptr) {
        return;
    }

    ESP_LOGI(TAG, "Clicked, %s", dummy_count % 2 ? "true" : "false");

    esp_lv_adapter_set_dummy_draw(disp, dummy_count % 2);
    dummy_count++;
}

void create_customer_ui()
{
    lv_display_t *disp = lv_display_get_default();
    if (disp == nullptr) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }

    lv_obj_t *scr = lv_disp_get_scr_act(disp);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);

    create_timer_tabview();

    lv_obj_t *dummy_touch_layer = lv_obj_create(scr);
    lv_obj_remove_style_all(dummy_touch_layer);
    lv_obj_set_size(dummy_touch_layer, LV_PCT(100), LV_PCT(30));
    lv_obj_align(dummy_touch_layer, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(dummy_touch_layer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(dummy_touch_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(dummy_touch_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(dummy_touch_layer, screen_touch_event_cb, LV_EVENT_CLICKED, NULL);

    ESP_LOGI(TAG, "UI created for %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);
}
