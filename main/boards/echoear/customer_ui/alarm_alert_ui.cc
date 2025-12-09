/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "alarm_alert_ui.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include "timer_ui.h"

#define SCREEN_WIDTH    360
#define SCREEN_HEIGHT   360

static const char *TAG_ALERT = "alarm_alert_ui";

/* Declare fonts */
LV_FONT_DECLARE(lv_font_montserrat_20);

/* 使用闹钟相关的图片资源作为 slider knob 图标 */
LV_IMG_DECLARE(time_sleep);

typedef struct {
    lv_obj_t *container;
    lv_obj_t *remind_later_btn;    // 稍后提醒按钮
    lv_obj_t *slider_container;    // 滑动容器
    lv_obj_t *slider;               // 滑动条
    lv_obj_t *slider_label;         // "滑动以停止" 标签
} alarm_alert_ui_t;

static alarm_alert_ui_t s_alert_ui;

/* 稍后提醒按钮事件处理 */
static void remind_later_btn_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG_ALERT, "Snooze 5 min button clicked - set timer to 5 minutes and return to timer UI");
        // Destroy alarm alert UI
        destroy_alarm_alert_ui();
        // Set timer to 5 minutes and start countdown
        timer_ui_adjust_end_point(5);  // Add 5 minutes
        // Show timer UI
        timer_ui_show();
    }
}

/* 滑动条事件处理 */
static void slider_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_slider_get_value(slider);
        // ESP_LOGI(TAG_ALERT, "Slider value: %ld", (long)value);
        if (value >= 100) {
            ESP_LOGI(TAG_ALERT, "Slider reached end - stop alarm and return to timer UI");
            lv_slider_set_value(slider, 0, LV_ANIM_ON);
            // Destroy alarm alert UI and show timer UI
            destroy_alarm_alert_ui();
            timer_ui_show();
        }
    }
}

void create_alarm_alert_ui(void)
{
    /* 主容器 */
    s_alert_ui.container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_alert_ui.container, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_center(s_alert_ui.container);
    lv_obj_set_style_bg_color(s_alert_ui.container, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_alert_ui.container, 0, 0);
    lv_obj_set_style_pad_all(s_alert_ui.container, 0, 0);

    /* 稍后提醒按钮 - 居中显示 */
    s_alert_ui.remind_later_btn = lv_btn_create(s_alert_ui.container);
    lv_obj_set_size(s_alert_ui.remind_later_btn, 200, 50);
    lv_obj_align(s_alert_ui.remind_later_btn, LV_ALIGN_CENTER, 0, -50);  // 居中，向上偏移50像素
    lv_obj_set_style_radius(s_alert_ui.remind_later_btn, 25, 0);  // 圆角
    lv_obj_set_style_bg_color(s_alert_ui.remind_later_btn, lv_color_make(0x98, 0x98, 0x98), 0);
    lv_obj_set_style_bg_opa(s_alert_ui.remind_later_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_alert_ui.remind_later_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(s_alert_ui.remind_later_btn, 0, 0);
    lv_obj_set_style_shadow_opa(s_alert_ui.remind_later_btn, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(s_alert_ui.remind_later_btn, remind_later_btn_event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t *remind_label = lv_label_create(s_alert_ui.remind_later_btn);
    lv_label_set_text(remind_label, "Snooze 5 min");
    lv_obj_set_style_text_font(remind_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(remind_label, lv_color_white(), 0);
    lv_obj_center(remind_label);

    /* 滑动容器 - 居中显示，在按钮下方 */
    s_alert_ui.slider_container = lv_obj_create(s_alert_ui.container);
    lv_obj_set_size(s_alert_ui.slider_container, 260, 80);
    lv_obj_align(s_alert_ui.slider_container, LV_ALIGN_CENTER, 0, 50);  // 居中，向下偏移50像素
    lv_obj_set_style_bg_opa(s_alert_ui.slider_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(s_alert_ui.slider_container, 0, 0);
    lv_obj_set_style_border_width(s_alert_ui.slider_container, 0, 0);
    lv_obj_set_style_pad_all(s_alert_ui.slider_container, 0, 0);

    /* "滑动以停止" 标签 */
    s_alert_ui.slider_label = lv_label_create(s_alert_ui.slider_container);
    lv_label_set_text(s_alert_ui.slider_label, "Slide to Stop");
    lv_obj_set_style_text_font(s_alert_ui.slider_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_alert_ui.slider_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_alert_ui.slider_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_alert_ui.slider_label, LV_ALIGN_CENTER, 0, 0);

    /* 滑动条（用于检测滑动），大小固定为 200x50 */
    s_alert_ui.slider = lv_slider_create(s_alert_ui.slider_container);
    lv_obj_set_width(s_alert_ui.slider, 200);   // 固定宽度 200，与按钮一致
    lv_obj_set_height(s_alert_ui.slider, 50);   // 固定高度 50
    lv_obj_clear_flag(s_alert_ui.slider, LV_OBJ_FLAG_SCROLLABLE);  // 禁用滚动
    lv_obj_align(s_alert_ui.slider, LV_ALIGN_CENTER, 0, 0);
    /* 使用图片作为 knob（例如 time_sleep），并让 knob 在轨道中居中 */
    lv_obj_set_style_width(s_alert_ui.slider, 40, LV_PART_KNOB);
    lv_obj_set_style_height(s_alert_ui.slider, 40, LV_PART_KNOB);
    lv_obj_set_style_radius(s_alert_ui.slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(s_alert_ui.slider, LV_OPA_TRANSP, LV_PART_KNOB);    // 不用纯色背景
    lv_obj_set_style_pad_all(s_alert_ui.slider, 0, LV_PART_KNOB);
    lv_obj_set_style_bg_img_src(s_alert_ui.slider, &time_sleep, LV_PART_KNOB);
    lv_slider_set_range(s_alert_ui.slider, 0, 100);
    lv_slider_set_value(s_alert_ui.slider, 0, LV_ANIM_OFF);

    /* 仅 slider 自己画出 200x50 的圆角条 */
    lv_obj_set_style_radius(s_alert_ui.slider, 25, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_alert_ui.slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_alert_ui.slider, lv_color_make(0x3F, 0x3F, 0x3F), LV_PART_MAIN);

    lv_obj_set_style_radius(s_alert_ui.slider, 25, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_alert_ui.slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_alert_ui.slider, lv_color_make(0x98, 0x98, 0x98), LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(s_alert_ui.slider, lv_color_make(0x41, 0x41, 0x41), LV_PART_KNOB);
    lv_obj_add_event_cb(s_alert_ui.slider, slider_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

    /* 将标签移到滑动条上方 */
    lv_obj_move_foreground(s_alert_ui.slider_label);

    ESP_LOGI(TAG_ALERT, "Alarm alert UI created");
}

void destroy_alarm_alert_ui(void)
{
    if (s_alert_ui.container) {
        lv_obj_del(s_alert_ui.container);
        s_alert_ui.container = NULL;
    }
}


