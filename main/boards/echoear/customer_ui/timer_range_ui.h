/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create 24h range timer / interval alarm UI with circular arc
 *
 * 参考 `create_timer_ui` 的样式，实现一个"定时区间闹钟"界面，但不倒计时：
 * - 和倒计时界面一样的圆形 UI、背景圆环和纹理进度条
 * - 整个圆表示 24 小时（0°=00:00，顺时针时间增加）
 * - 中央文本显示：开始时间和结束时间（24 小时制，例如 `08:00 - 22:00`）
 * - start / end 两个 knob 都可以拖动，表示一个 24h 区间
 */
void create_timer_range_ui(void);

/**
 * @brief Create 24h range timer UI with parent container
 *
 * @param parent  Parent object to create the UI in
 */
void create_timer_range_ui_with_parent(lv_obj_t *parent);

/**
 * @brief Destroy range timer UI and free resources
 */
void destroy_timer_range_ui(void);

/**
 * @brief Trigger center button click event
 *
 * Simulates a click on the center button to toggle duration display
 */
void timer_range_ui_trigger_center_btn(void);

#ifdef __cplusplus
}
#endif


