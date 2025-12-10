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
 * @brief Create pomodoro timer UI with parent container
 *
 * @param parent  Parent object to create the UI in
 */
void alarm_pomodoro_create_with_parent(lv_obj_t *parent);

/**
 * @brief Adjust end point by delta minutes
 *
 * @param delta_minutes  Positive value to increase time, negative to decrease (unit: minutes)
 */
void alarm_pomodoro_adjust_end_point(int32_t delta_minutes);

/**
 * @brief Toggle start/pause timer
 *
 * If timer is running, pause it. If paused, resume it (if there's remaining time).
 */
void alarm_pomodoro_toggle_start_pause(void);

/**
 * @brief Reset timer to 0:00
 */
void alarm_pomodoro_reset_to_zero(void);

/**
 * @brief Check if timer is currently paused
 * @return true if timer is paused, false if running or not initialized
 */
bool alarm_pomodoro_is_paused(void);

/**
 * @brief Start or resume the pomodoro timer
 *
 * If not on pomodoro page, switches to it first.
 * If timer is paused, resumes it. If already running, does nothing.
 */
void alarm_pomodoro_start(void);

/**
 * @brief Pause the pomodoro timer
 *
 * If not on pomodoro page, switches to it first.
 * If timer is running, pauses it. If already paused, does nothing.
 */
void alarm_pomodoro_pause(void);

#ifdef __cplusplus
}
#endif
