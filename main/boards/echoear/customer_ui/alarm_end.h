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
 * @brief Create alarm time up UI with parent container
 *
 * @param parent  Parent object to create the UI in
 */
void alarm_time_up_create_with_parent(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
