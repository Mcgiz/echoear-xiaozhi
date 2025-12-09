/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "timer_range_ui.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include <math.h>
// #include "esp_lvgl_port.h"

#define SCREEN_WIDTH    360
#define SCREEN_HEIGHT   360

/* Declare fonts */
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_32);

/* 24h 一圈：360° -> 24 小时 -> 0° = 00:00（顶部，12 点方向），顺时针时间变大 */
#define DAY_SECONDS           (24 * 60 * 60)
#define DEFAULT_START_HOUR    22   // 默认开始时间 22:00
#define DEFAULT_END_HOUR      8  // 默认结束时间 08:00

static const char *TAG_RANGE = "timer_range_ui";

LV_IMG_DECLARE(time_start);
LV_IMG_DECLARE(time_sleep);
LV_IMG_DECLARE(watch_bg);
LV_IMG_DECLARE(time_arc_texture);

typedef struct {
    lv_obj_t *container;
    lv_obj_t *bg_arc;             // Background gray arc
    lv_obj_t *range_arc;          // Selected range arc
    lv_obj_t *time_container;     // Container for time labels (3 行 x 4 列)
    lv_obj_t *start_hour_label;   // 起始时间：小时
    lv_obj_t *start_colon_label;  // 起始时间：冒号
    lv_obj_t *start_min_label;    // 起始时间：分钟
    lv_obj_t *start_ampm_label;   // 起始时间：AM/PM
    lv_obj_t *center_sep_label;   // 中间分隔符 "|"
    lv_obj_t *end_hour_label;     // 结束时间：小时
    lv_obj_t *end_colon_label;    // 结束时间：冒号
    lv_obj_t *end_min_label;      // 结束时间：分钟
    lv_obj_t *end_ampm_label;     // 结束时间：AM/PM
    lv_obj_t *duration_label;     // Duration label (initially hidden, shown when user clicks center button)
    lv_obj_t *center_btn;         // Transparent center button to toggle duration display
    lv_obj_t *start_knob_img;     // Start point knob image (draggable)
    lv_obj_t *end_knob_img;       // End point knob image (draggable)
    int32_t start_angle;          // 0~360, LVGL 角度，旋转 270 后 0° 在 12 点方向，顺时针
    int32_t end_angle;            // 0~360, LVGL 角度
    bool show_duration;           // Toggle between time display and duration display
} timer_range_ui_t;

static timer_range_ui_t s_range_ui;

/* 计算从圆心到触摸点的角度，返回 LVGL 坐标系下的角度（0°=顶部，顺时针） */
static float calculate_angle_from_center(lv_coord_t center_x, lv_coord_t center_y,
        lv_coord_t point_x, lv_coord_t point_y)
{
    float dx = point_x - center_x;
    float dy = center_y - point_y;  // Invert Y because screen Y increases downward
    float angle_rad = atan2f(dy, dx);
    float angle_deg = angle_rad * 180.0f / (float)M_PI;

    /* 数学坐标系 0° 在右侧，逆时针；这里转换到 LVGL 弧线使用的（旋转 270 后）坐标 */
    angle_deg = 90.0f - angle_deg;

    if (angle_deg < 0) {
        angle_deg += 360.0f;
    } else if (angle_deg >= 360.0f) {
        angle_deg -= 360.0f;
    }

    return angle_deg;
}

/* 根据当前 start / end 角度更新 knob 位置 */
static void update_knob_positions(timer_range_ui_t *ui)
{
    if (!ui->range_arc || !ui->start_knob_img || !ui->end_knob_img) {
        return;
    }

    lv_coord_t arc_size = lv_obj_get_width(ui->range_arc);
    lv_coord_t arc_width = 30;
    lv_coord_t radius = arc_size / 2 - arc_width / 2;
    lv_coord_t center_x = arc_size / 2;
    lv_coord_t center_y = arc_size / 2;

    lv_coord_t arc_x = lv_obj_get_x(ui->range_arc);
    lv_coord_t arc_y = lv_obj_get_y(ui->range_arc);

    lv_coord_t start_w = lv_obj_get_width(ui->start_knob_img);
    lv_coord_t start_h = lv_obj_get_height(ui->start_knob_img);
    lv_coord_t end_w   = lv_obj_get_width(ui->end_knob_img);
    lv_coord_t end_h   = lv_obj_get_height(ui->end_knob_img);

    /* LVGL 角度 -> 数学坐标角度 */
    float start_rad = (90.0f - (float)ui->start_angle) * (float)M_PI / 180.0f;
    float end_rad   = (90.0f - (float)ui->end_angle)   * (float)M_PI / 180.0f;

    lv_coord_t start_x = arc_x + center_x + (lv_coord_t)(radius * cosf(start_rad)) - start_w / 2;
    lv_coord_t start_y = arc_y + center_y - (lv_coord_t)(radius * sinf(start_rad)) - start_h / 2;
    lv_coord_t end_x   = arc_x + center_x + (lv_coord_t)(radius * cosf(end_rad))   - end_w / 2;
    lv_coord_t end_y   = arc_y + center_y - (lv_coord_t)(radius * sinf(end_rad))   - end_h / 2;

    lv_obj_set_pos(ui->start_knob_img, start_x, start_y);
    lv_obj_set_pos(ui->end_knob_img, end_x, end_y);
}

/* 根据 start / end 角度更新时间显示（24 小时制）和弧线 */
static void update_range_from_angles(timer_range_ui_t *ui)
{
    /* 角度 -> 一天中的时间（秒），0°=00:00，360° 接近 24:00/00:00 */
    int32_t start_total_sec = (ui->start_angle * DAY_SECONDS) / 360;
    int32_t end_total_sec   = (ui->end_angle   * DAY_SECONDS) / 360;

    int32_t start_hour_24 = start_total_sec / 3600;
    int32_t start_min     = (start_total_sec % 3600) / 60;
    int32_t end_hour_24   = end_total_sec / 3600;
    int32_t end_min       = (end_total_sec % 3600) / 60;


    // Calculate duration in hours (handle wrap-around for overnight periods)
    int32_t duration_sec;
    if (end_total_sec >= start_total_sec) {
        duration_sec = end_total_sec - start_total_sec;
    } else {
        // Overnight: e.g., 22:00 -> 07:30 = (24:00 - 22:00) + 07:30
        duration_sec = (DAY_SECONDS - start_total_sec) + end_total_sec;
    }
    float duration_hours = duration_sec / 3600.0f;

    /* 转为 12 小时制 + AM/PM，仅用于显示 */
    int32_t start_hour_12 = start_hour_24 % 12;
    if (start_hour_12 == 0) {
        start_hour_12 = 12;
    }
    const char *start_ampm = (start_hour_24 < 12) ? "AM" : "PM";

    int32_t end_hour_12 = end_hour_24 % 12;
    if (end_hour_12 == 0) {
        end_hour_12 = 12;
    }
    const char *end_ampm = (end_hour_24 < 12) ? "AM" : "PM";

    // Update display based on show_duration flag
    if (ui->show_duration) {
        // Show duration in center
        if (ui->time_container) {
            lv_obj_add_flag(ui->time_container, LV_OBJ_FLAG_HIDDEN);
        }
        if (ui->duration_label) {
            lv_obj_clear_flag(ui->duration_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(ui->duration_label, "%.1f hr", duration_hours);
        }
    } else {
        // Show time range
        if (ui->duration_label) {
            lv_obj_add_flag(ui->duration_label, LV_OBJ_FLAG_HIDDEN);
        }
        if (ui->time_container) {
            lv_obj_clear_flag(ui->time_container, LV_OBJ_FLAG_HIDDEN);
        }

        if (ui->start_hour_label && ui->start_min_label && ui->start_ampm_label &&
                ui->center_sep_label && ui->end_hour_label && ui->end_min_label && ui->end_ampm_label) {
            // Top row: HH : MM AM/PM
            lv_label_set_text_fmt(ui->start_hour_label, "%ld", (long)start_hour_12);
            lv_label_set_text_fmt(ui->start_min_label, "%02ld", (long)start_min);
            lv_label_set_text(ui->start_ampm_label, start_ampm);

            // Center separator
            lv_label_set_text(ui->center_sep_label, "|");

            // Bottom row: HH : MM AM/PM
            lv_label_set_text_fmt(ui->end_hour_label, "%ld", (long)end_hour_12);
            lv_label_set_text_fmt(ui->end_min_label, "%02ld", (long)end_min);
            lv_label_set_text(ui->end_ampm_label, end_ampm);
        }
    }

    /* 更新弧线显示区间 */
    lv_arc_set_angles(ui->range_arc, (int16_t)ui->start_angle, (int16_t)ui->end_angle);

    /* 更新两个 knob 的位置 */
    update_knob_positions(ui);

    ESP_LOGD(TAG_RANGE, "range: start=%ld° -> %02ld:%02ld, end=%ld° -> %02ld:%02ld",
             (long)ui->start_angle, (long)start_hour_24, (long)start_min,
             (long)ui->end_angle, (long)end_hour_24, (long)end_min);
}

/* 计算弧线中心在屏幕绝对坐标中的位置 */
static void get_arc_center_abs(lv_obj_t *arc, lv_coord_t *center_x, lv_coord_t *center_y)
{
    lv_coord_t x = 0;
    lv_coord_t y = 0;
    lv_obj_t *obj = arc;

    while (obj != NULL) {
        x += lv_obj_get_x(obj);
        y += lv_obj_get_y(obj);
        obj = lv_obj_get_parent(obj);
        if (obj == lv_scr_act()) {
            break;
        }
    }

    x += lv_obj_get_width(arc) / 2;
    y += lv_obj_get_height(arc) / 2;

    *center_x = x;
    *center_y = y;
}

/* start knob 事件：拖动改变 start_angle */
static void start_knob_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    timer_range_ui_t *ui = (timer_range_ui_t *)lv_event_get_user_data(e);

    if (code == LV_EVENT_PRESSED) {
        lv_obj_t *knob = (lv_obj_t *)lv_event_get_target(e);
        lv_obj_set_style_transform_scale(knob, 280, 0);  // 110% scale
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_t *knob = (lv_obj_t *)lv_event_get_target(e);
        lv_obj_set_style_transform_scale(knob, 256, 0);  // 100% scale
    }

    if (code == LV_EVENT_PRESSING || code == LV_EVENT_PRESSED) {
        lv_indev_t *indev = lv_indev_get_act();
        if (indev == NULL) {
            return;
        }

        lv_point_t point;
        lv_indev_get_point(indev, &point);

        lv_coord_t cx = 0;
        lv_coord_t cy = 0;
        get_arc_center_abs(ui->range_arc, &cx, &cy);

        float angle = calculate_angle_from_center(cx, cy, point.x, point.y);
        if (angle < 0.0f) {
            angle = 0.0f;
        } else if (angle > 360.0f) {
            angle = 360.0f;
        }

        ui->start_angle = (int32_t)angle;
        update_range_from_angles(ui);
    }
}

/**
 * @brief Center button event handler - toggle duration display
 */
static void center_btn_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    timer_range_ui_t *ui = (timer_range_ui_t *)lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED) {
        // Toggle duration display
        ui->show_duration = !ui->show_duration;
        ESP_LOGI(TAG_RANGE, "Toggled duration display: %s", ui->show_duration ? "ON" : "OFF");

        // Update display
        update_range_from_angles(ui);
    }
}

/* end knob 事件：拖动改变 end_angle */
static void end_knob_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    timer_range_ui_t *ui = (timer_range_ui_t *)lv_event_get_user_data(e);

    if (code == LV_EVENT_PRESSED) {
        lv_obj_t *knob = (lv_obj_t *)lv_event_get_target(e);
        lv_obj_set_style_transform_scale(knob, 280, 0);  // 110% scale
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_t *knob = (lv_obj_t *)lv_event_get_target(e);
        lv_obj_set_style_transform_scale(knob, 256, 0);  // 100% scale
    }

    if (code == LV_EVENT_PRESSING || code == LV_EVENT_PRESSED) {
        lv_indev_t *indev = lv_indev_get_act();
        if (indev == NULL) {
            return;
        }

        lv_point_t point;
        lv_indev_get_point(indev, &point);

        lv_coord_t cx = 0;
        lv_coord_t cy = 0;
        get_arc_center_abs(ui->range_arc, &cx, &cy);

        float angle = calculate_angle_from_center(cx, cy, point.x, point.y);
        if (angle < 0.0f) {
            angle = 0.0f;
        } else if (angle > 360.0f) {
            angle = 360.0f;
        }

        ui->end_angle = (int32_t)angle;
        update_range_from_angles(ui);
    }
}

// Internal function that accepts parent
static void create_timer_range_ui_internal(lv_obj_t *parent)
{
    /* 默认区间：按照 24 小时制，从 DEFAULT_START_HOUR 到 DEFAULT_END_HOUR */
    s_range_ui.start_angle = (DEFAULT_START_HOUR * 360) / 24;
    s_range_ui.end_angle   = (DEFAULT_END_HOUR   * 360) / 24;

    /* 容器 */
    s_range_ui.container = lv_obj_create(parent);
    // Use percentage size for tabview compatibility, or fixed size if parent is screen
    if (parent == lv_scr_act()) {
        lv_obj_set_size(s_range_ui.container, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_center(s_range_ui.container);
    } else {
        // For tabview, use full size and let parent manage layout
        lv_obj_set_size(s_range_ui.container, LV_PCT(100), LV_PCT(100));
    }
    lv_obj_set_style_bg_color(s_range_ui.container, lv_color_black(), 0);
    lv_obj_set_style_border_width(s_range_ui.container, 0, 0);
    lv_obj_set_style_pad_all(s_range_ui.container, 0, 0);
    // Don't block parent scrolling
    lv_obj_clear_flag(s_range_ui.container, LV_OBJ_FLAG_SCROLLABLE);

    lv_coord_t arc_size = LV_MIN(SCREEN_WIDTH, SCREEN_HEIGHT) - 20;

    /* 背景圆环（灰色） */
    s_range_ui.bg_arc = lv_arc_create(s_range_ui.container);
    lv_obj_set_size(s_range_ui.bg_arc, arc_size + 20, arc_size + 20);
    lv_obj_center(s_range_ui.bg_arc);
    lv_arc_set_rotation(s_range_ui.bg_arc, 270);
    lv_arc_set_bg_angles(s_range_ui.bg_arc, 0, 360);
    lv_obj_remove_style(s_range_ui.bg_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_range_ui.bg_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_range_ui.bg_arc, 50, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_range_ui.bg_arc, lv_color_make(0x56, 0x56, 0x56), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_range_ui.bg_arc, 0, LV_PART_INDICATOR);
    lv_arc_set_value(s_range_ui.bg_arc, 0);

    /* 区间弧线（带纹理） */
    s_range_ui.range_arc = lv_arc_create(s_range_ui.container);
    lv_obj_set_size(s_range_ui.range_arc, arc_size, arc_size);
    lv_obj_center(s_range_ui.range_arc);
    lv_arc_set_rotation(s_range_ui.range_arc, 270);
    lv_arc_set_mode(s_range_ui.range_arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_bg_angles(s_range_ui.range_arc, 0, 360);
    lv_arc_set_range(s_range_ui.range_arc, 0, 360);

    /* 先设置个初始区间角度，后面 update_range_from_angles 会再更新一次 */
    lv_arc_set_angles(s_range_ui.range_arc,
                      (int16_t)s_range_ui.start_angle,
                      (int16_t)s_range_ui.end_angle);

    lv_obj_set_style_arc_image_src(s_range_ui.range_arc, &time_arc_texture, LV_PART_INDICATOR);
    lv_obj_remove_style(s_range_ui.range_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_range_ui.range_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_range_ui.range_arc, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_range_ui.range_arc, 30, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_range_ui.range_arc, false, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_range_ui.range_arc, lv_color_make(0x56, 0x56, 0x56), LV_PART_INDICATOR);

    /* 先在中间贴一张表盘背景图 `watch_bg`，再在其上方叠加时间文字 */
    lv_obj_t *watch_bg_img = lv_img_create(s_range_ui.container);
    lv_img_set_src(watch_bg_img, &watch_bg);
    lv_obj_center(watch_bg_img);
    lv_obj_clear_flag(watch_bg_img, LV_OBJ_FLAG_SCROLLABLE);

    /* 中央时间显示容器：3 行 x 4 列
     *   第 1 行：开始时间  [HH] [:] [MM] [AM/PM]
     *   第 2 行：分隔符    [  ][|][  ][   ]
     *   第 3 行：结束时间  [HH] [:] [MM] [AM/PM]
     */
    s_range_ui.time_container = lv_obj_create(s_range_ui.container);
    lv_obj_set_size(s_range_ui.time_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_range_ui.time_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_range_ui.time_container, 0, 0);
    lv_obj_set_style_pad_all(s_range_ui.time_container, 0, 0);
    lv_obj_center(s_range_ui.time_container);

    /* 使用固定宽度的列，避免数字变化时布局移动，减小小时和冒号之间的间距 */
    static const int32_t col_dsc[] = {
        20,  /* 小时列：固定宽度，能容纳2位数字，进一步减小宽度 */
        4,   /* 冒号列：固定宽度，进一步减小间距 */
        30,  /* 分钟列：固定宽度，增加宽度以避免换行 */
        35,  /* AM/PM列：固定宽度，增加宽度以避免换行 */
        LV_GRID_TEMPLATE_LAST
    };
    static const int32_t row_dsc[] = {
        LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST
    };
    lv_obj_set_grid_dsc_array(s_range_ui.time_container, col_dsc, row_dsc);

    /* 第一行：开始时间，4 列 [HH] [:] [MM] [AM/PM] */
    s_range_ui.start_hour_label = lv_label_create(s_range_ui.time_container);
    lv_label_set_text(s_range_ui.start_hour_label, "10");
    lv_obj_set_size(s_range_ui.start_hour_label, 20, LV_SIZE_CONTENT);
    lv_obj_set_grid_cell(s_range_ui.start_hour_label,
                         LV_GRID_ALIGN_END, 0, 1,
                         LV_GRID_ALIGN_CENTER, 0, 1);

    s_range_ui.start_colon_label = lv_label_create(s_range_ui.time_container);
    lv_label_set_text(s_range_ui.start_colon_label, ":");
    lv_obj_set_grid_cell(s_range_ui.start_colon_label,
                         LV_GRID_ALIGN_CENTER, 1, 1,
                         LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_style_text_font(s_range_ui.start_colon_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_range_ui.start_colon_label, lv_color_white(), 0);

    s_range_ui.start_min_label = lv_label_create(s_range_ui.time_container);
    lv_label_set_text(s_range_ui.start_min_label, "00");
    lv_obj_set_size(s_range_ui.start_min_label, 30, LV_SIZE_CONTENT);
    lv_obj_set_grid_cell(s_range_ui.start_min_label,
                         LV_GRID_ALIGN_START, 2, 1,
                         LV_GRID_ALIGN_CENTER, 0, 1);

    s_range_ui.start_ampm_label = lv_label_create(s_range_ui.time_container);
    lv_label_set_text(s_range_ui.start_ampm_label, "PM");
    lv_obj_set_size(s_range_ui.start_ampm_label, 35, LV_SIZE_CONTENT);
    lv_label_set_long_mode(s_range_ui.start_ampm_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_grid_cell(s_range_ui.start_ampm_label,
                         LV_GRID_ALIGN_START, 3, 1,
                         LV_GRID_ALIGN_CENTER, 0, 1);

    /* 第二行：中间分隔符 |，放在中间一列 */
    s_range_ui.center_sep_label = lv_label_create(s_range_ui.time_container);
    lv_label_set_text(s_range_ui.center_sep_label, "|");
    lv_obj_set_grid_cell(s_range_ui.center_sep_label,
                         LV_GRID_ALIGN_CENTER, 1, 1,
                         LV_GRID_ALIGN_CENTER, 1, 1);

    /* 第三行：结束时间，4 列 [HH] [:] [MM] [AM/PM] */
    s_range_ui.end_hour_label = lv_label_create(s_range_ui.time_container);
    lv_label_set_text(s_range_ui.end_hour_label, "7");
    lv_obj_set_size(s_range_ui.end_hour_label, 20, LV_SIZE_CONTENT);
    lv_obj_set_grid_cell(s_range_ui.end_hour_label,
                         LV_GRID_ALIGN_END, 0, 1,
                         LV_GRID_ALIGN_CENTER, 2, 1);

    s_range_ui.end_colon_label = lv_label_create(s_range_ui.time_container);
    lv_label_set_text(s_range_ui.end_colon_label, ":");
    lv_obj_set_grid_cell(s_range_ui.end_colon_label,
                         LV_GRID_ALIGN_CENTER, 1, 1,
                         LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_style_text_font(s_range_ui.end_colon_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_range_ui.end_colon_label, lv_color_white(), 0);

    s_range_ui.end_min_label = lv_label_create(s_range_ui.time_container);
    lv_label_set_text(s_range_ui.end_min_label, "30");
    lv_obj_set_size(s_range_ui.end_min_label, 30, LV_SIZE_CONTENT);
    lv_obj_set_grid_cell(s_range_ui.end_min_label,
                         LV_GRID_ALIGN_START, 2, 1,
                         LV_GRID_ALIGN_CENTER, 2, 1);

    s_range_ui.end_ampm_label = lv_label_create(s_range_ui.time_container);
    lv_label_set_text(s_range_ui.end_ampm_label, "AM");
    lv_obj_set_size(s_range_ui.end_ampm_label, 35, LV_SIZE_CONTENT);
    lv_label_set_long_mode(s_range_ui.end_ampm_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_grid_cell(s_range_ui.end_ampm_label,
                         LV_GRID_ALIGN_START, 3, 1,
                         LV_GRID_ALIGN_CENTER, 2, 1);

    /* 统一设置字体和颜色 */
    lv_obj_set_style_text_font(s_range_ui.start_hour_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(s_range_ui.start_min_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(s_range_ui.start_ampm_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(s_range_ui.center_sep_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(s_range_ui.end_hour_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(s_range_ui.end_min_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(s_range_ui.end_ampm_label, &lv_font_montserrat_20, 0);

    lv_obj_set_style_text_color(s_range_ui.start_hour_label, lv_color_white(), 0);
    lv_obj_set_style_text_color(s_range_ui.start_min_label, lv_color_white(), 0);
    lv_obj_set_style_text_color(s_range_ui.start_ampm_label, lv_color_white(), 0);
    lv_obj_set_style_text_color(s_range_ui.center_sep_label, lv_color_white(), 0);
    lv_obj_set_style_text_color(s_range_ui.end_hour_label, lv_color_white(), 0);
    lv_obj_set_style_text_color(s_range_ui.end_min_label, lv_color_white(), 0);
    lv_obj_set_style_text_color(s_range_ui.end_ampm_label, lv_color_white(), 0);

    /* 设置文本对齐方式 */
    lv_obj_set_style_text_align(s_range_ui.start_ampm_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_align(s_range_ui.end_ampm_label, LV_TEXT_ALIGN_LEFT, 0);

    // Duration label (initially hidden, shown when user clicks center button)
    s_range_ui.duration_label = lv_label_create(s_range_ui.container);
    lv_label_set_text(s_range_ui.duration_label, "9.5 hr");
    lv_obj_set_style_text_font(s_range_ui.duration_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s_range_ui.duration_label, lv_color_white(), 0);
    lv_obj_center(s_range_ui.duration_label);
    lv_obj_add_flag(s_range_ui.duration_label, LV_OBJ_FLAG_HIDDEN);  // Initially hidden

    // Transparent center button to toggle duration display
    s_range_ui.center_btn = lv_btn_create(s_range_ui.container);
    lv_obj_set_size(s_range_ui.center_btn, 120, 120);  // Large clickable area
    lv_obj_center(s_range_ui.center_btn);
    lv_obj_set_style_bg_opa(s_range_ui.center_btn, LV_OPA_TRANSP, 0);  // Transparent
    lv_obj_set_style_border_opa(s_range_ui.center_btn, LV_OPA_TRANSP, 0);  // No border
    lv_obj_set_style_shadow_opa(s_range_ui.center_btn, LV_OPA_TRANSP, 0);  // No shadow
    lv_obj_add_event_cb(s_range_ui.center_btn, center_btn_event_handler, LV_EVENT_CLICKED, &s_range_ui);

    // Initialize show_duration flag to false (show time range by default)
    s_range_ui.show_duration = false;

    /* knob 图标：start / end 都可拖动 */
    s_range_ui.start_knob_img = lv_img_create(s_range_ui.container);
    lv_img_set_src(s_range_ui.start_knob_img, &time_start);
    lv_obj_clear_flag(s_range_ui.start_knob_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_range_ui.start_knob_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_range_ui.start_knob_img, start_knob_event_handler, LV_EVENT_ALL, &s_range_ui);

    s_range_ui.end_knob_img = lv_img_create(s_range_ui.container);
    lv_img_set_src(s_range_ui.end_knob_img, &time_sleep);
    lv_obj_clear_flag(s_range_ui.end_knob_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_range_ui.end_knob_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_range_ui.end_knob_img, end_knob_event_handler, LV_EVENT_ALL, &s_range_ui);

    /* 先强制做一次布局计算，确保 x/y/宽高有效，再初始化 knob 位置和时间显示 */
    lv_obj_update_layout(s_range_ui.container);

    /* 初始化 knob 位置和时间显示 */
    update_range_from_angles(&s_range_ui);

    ESP_LOGI(TAG_RANGE, "Timer range UI created: start=%ld°, end=%ld°",
             (long)s_range_ui.start_angle, (long)s_range_ui.end_angle);
}

#ifdef __cplusplus
extern "C" {
#endif

// Public function for backward compatibility
void create_timer_range_ui(void)
{
    create_timer_range_ui_internal(lv_scr_act());
}

// Public function with parent parameter
void create_timer_range_ui_with_parent(lv_obj_t *parent)
{
    create_timer_range_ui_internal(parent);
}

void destroy_timer_range_ui(void)
{
    if (s_range_ui.container) {
        lv_obj_del(s_range_ui.container);
        s_range_ui.container = NULL;
    }
}

#ifdef __cplusplus
}
#endif


