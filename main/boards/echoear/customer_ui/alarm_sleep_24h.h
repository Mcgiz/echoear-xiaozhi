/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "lvgl.h"

#define PAGE_SLEEP      "SLEEP"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create 24h sleep timer UI with parent container
 *
 * @param parent  Parent object to create the UI in
 * @return Created container object
 */
lv_obj_t *alarm_sleep_24h_create_with_parent(lv_obj_t *parent);

/**
 * @brief Trigger center button click event
 *
 * Simulates a click on the center button to toggle duration display
 */
void alarm_sleep_24h_trigger_center_btn(void);

/**
 * @brief Set sleep time range
 *
 * Sets the start and end time for the sleep timer, precise to minutes.
 * Start time is set to current time, end time is specified.
 *
 * @param end_hour End hour (0-23)
 * @param end_min  End minute (0-59)
 */
void alarm_sleep_24h_set_time_range(int32_t end_hour, int32_t end_min);

/**
 * @brief Set end time only
 *
 * Sets only the end time for the sleep timer, precise to minutes.
 * The start time is automatically set to current time.
 *
 * @param end_hour End hour (0-23)
 * @param end_min  End minute (0-59)
 */
void alarm_sleep_24h_set_end_time(int32_t end_hour, int32_t end_min);

#ifdef __cplusplus
}
#endif
