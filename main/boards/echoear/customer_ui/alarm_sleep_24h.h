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

#ifdef __cplusplus
}
#endif
