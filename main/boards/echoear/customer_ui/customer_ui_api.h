/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file customer_ui_api.h
 * @brief Public API for Customer UI module
 * 
 * This header file contains only the interfaces that are needed by external modules.
 * Internal interfaces are kept in their respective header files.
 */

#ifndef CUSTOMER_UI_API_H
#define CUSTOMER_UI_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Pomodoro Timer API
 * ============================================================================ */

/**
 * @brief Show pomodoro page with specified minutes (default 5 min if <= 0)
 *
 * This will:
 *  - reset pomodoro timer to 0
 *  - set remaining time to `minutes` (or 5 if minutes <= 0)
 *  - show pomodoro page
 *
 * @param minutes Timer duration in minutes (1-60, default 5)
 */
void main_ui_show_pomodoro_with_minutes(int32_t minutes);

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

/**
 * @brief Check if timer is currently paused
 * @return true if timer is paused, false if running or not initialized
 */
bool alarm_pomodoro_is_paused(void);

#ifdef __cplusplus
}
#endif

#endif // CUSTOMER_UI_API_H

