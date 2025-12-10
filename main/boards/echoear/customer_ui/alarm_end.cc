/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include "esp_lv_adapter.h"
#include "main_ui.h"
#include "alarm_end.h"
#include "alarm_pomodoro.h"

static const char *TAG = "alarm_time_up";

typedef struct {
    lv_obj_t *container;
    lv_obj_t *remind_later_btn;
    lv_obj_t *slider_container;
    lv_obj_t *slider;
    lv_obj_t *slider_label;
} alarm_time_up_ui_t;

static alarm_time_up_ui_t s_time_up_ui;

static void remind_later_btn_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Snooze 5 min button clicked");
        alarm_time_up_hide();
        alarm_pomodoro_adjust_end_point(5);
        alarm_pomodoro_show();
    }
}

static void slider_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_slider_get_value(slider);
        if (value >= 100) {
            ESP_LOGI(TAG, "Slider reached end - stop alarm");
            lv_slider_set_value(slider, 0, LV_ANIM_ON);
            main_ui_show_dummy_page();
        }
    }
}

void alarm_time_up_create_with_parent(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating alarm time up UI");

    s_time_up_ui.container = lv_obj_create(parent);
    lv_obj_set_size(s_time_up_ui.container, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_center(s_time_up_ui.container);
    lv_obj_set_style_bg_color(s_time_up_ui.container, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_time_up_ui.container, 0, 0);
    lv_obj_set_style_pad_all(s_time_up_ui.container, 0, 0);

    s_time_up_ui.remind_later_btn = lv_btn_create(s_time_up_ui.container);
    lv_obj_set_size(s_time_up_ui.remind_later_btn, 200, 50);
    lv_obj_align(s_time_up_ui.remind_later_btn, LV_ALIGN_CENTER, 0, -50);
    lv_obj_set_style_radius(s_time_up_ui.remind_later_btn, 25, 0);
    lv_obj_set_style_bg_color(s_time_up_ui.remind_later_btn, lv_color_make(0x98, 0x98, 0x98), 0);
    lv_obj_set_style_bg_opa(s_time_up_ui.remind_later_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_time_up_ui.remind_later_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(s_time_up_ui.remind_later_btn, 0, 0);
    lv_obj_set_style_shadow_opa(s_time_up_ui.remind_later_btn, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(s_time_up_ui.remind_later_btn, remind_later_btn_event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t *remind_label = lv_label_create(s_time_up_ui.remind_later_btn);
    lv_label_set_text(remind_label, "Snooze 5 min");
    lv_obj_set_style_text_font(remind_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(remind_label, lv_color_white(), 0);
    lv_obj_center(remind_label);

    s_time_up_ui.slider_container = lv_obj_create(s_time_up_ui.container);
    lv_obj_set_size(s_time_up_ui.slider_container, 260, 80);
    lv_obj_align(s_time_up_ui.slider_container, LV_ALIGN_CENTER, 0, 50);
    lv_obj_set_style_bg_opa(s_time_up_ui.slider_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(s_time_up_ui.slider_container, 0, 0);
    lv_obj_set_style_border_width(s_time_up_ui.slider_container, 0, 0);
    lv_obj_set_style_pad_all(s_time_up_ui.slider_container, 0, 0);

    s_time_up_ui.slider_label = lv_label_create(s_time_up_ui.slider_container);
    lv_label_set_text(s_time_up_ui.slider_label, "Slide to Stop");
    lv_obj_set_style_text_font(s_time_up_ui.slider_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_time_up_ui.slider_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_time_up_ui.slider_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_time_up_ui.slider_label, LV_ALIGN_CENTER, 0, 0);

    s_time_up_ui.slider = lv_slider_create(s_time_up_ui.slider_container);
    lv_obj_set_width(s_time_up_ui.slider, 200);
    lv_obj_set_height(s_time_up_ui.slider, 50);
    lv_obj_clear_flag(s_time_up_ui.slider, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_time_up_ui.slider, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_width(s_time_up_ui.slider, 40, LV_PART_KNOB);
    lv_obj_set_style_height(s_time_up_ui.slider, 40, LV_PART_KNOB);
    lv_obj_set_style_radius(s_time_up_ui.slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(s_time_up_ui.slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_time_up_ui.slider, 0, LV_PART_KNOB);
    lv_obj_set_style_bg_img_src(s_time_up_ui.slider, &time_sleep, LV_PART_KNOB);
    lv_slider_set_range(s_time_up_ui.slider, 0, 100);
    lv_slider_set_value(s_time_up_ui.slider, 0, LV_ANIM_OFF);

    lv_obj_set_style_radius(s_time_up_ui.slider, 25, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_time_up_ui.slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_time_up_ui.slider, lv_color_make(0x3F, 0x3F, 0x3F), LV_PART_MAIN);

    lv_obj_set_style_radius(s_time_up_ui.slider, 25, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_time_up_ui.slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_time_up_ui.slider, lv_color_make(0x98, 0x98, 0x98), LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(s_time_up_ui.slider, lv_color_make(0x41, 0x41, 0x41), LV_PART_KNOB);
    lv_obj_add_event_cb(s_time_up_ui.slider, slider_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_move_foreground(s_time_up_ui.slider_label);
}
