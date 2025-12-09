/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create timer UI with circular progress arc
 *
 * Creates a countdown timer UI with:
 * - Central time display
 * - Gray background arc ring
 * - Progress arc that decreases as time counts down
 * - Knob images at start and end points of the arc
 */
void create_timer_ui(void);

/**
 * @brief Create timer UI with parent container
 *
 * @param parent  Parent object to create the UI in
 */
void create_timer_ui_with_parent(lv_obj_t *parent);

/**
 * @brief Create tabview with timer UI and timer range UI
 *
 * Creates a tabview that contains two tabs:
 * - Tab 1: Timer UI (countdown timer)
 * - Tab 2: Timer Range UI (24h range selector)
 * Users can swipe horizontally to switch between tabs.
 */
void create_timer_tabview(void);

/**
 * @brief Destroy timer UI and free resources
 */
void destroy_timer_ui(void);

/**
 * @brief Adjust end point by delta minutes
 *
 * @param delta_minutes  Positive value to increase time, negative to decrease (unit: minutes)
 */
void timer_ui_adjust_end_point(int32_t delta_minutes);

/**
 * @brief Toggle start/pause timer
 *
 * If timer is running, pause it. If paused, resume it (if there's remaining time).
 */
void timer_ui_toggle_start_pause(void);

/**
 * @brief Get current timer mode
 *
 * @return true if in countdown mode, false if in range mode
 */
bool timer_ui_is_countdown_mode(void);

/**
 * @brief Reset timer to 0:00
 */
void timer_ui_reset_to_zero(void);

/**
 * @brief Switch to countdown mode
 */
void timer_ui_switch_to_countdown(void);

/**
 * @brief Switch to range mode
 */
void timer_ui_switch_to_range(void);

/**
 * @brief Show timer UI (make it visible)
 */
void timer_ui_show(void);

/**
 * @brief Hide timer UI (make it invisible)
 */
void timer_ui_hide(void);

#ifdef __cplusplus
}
#endif
