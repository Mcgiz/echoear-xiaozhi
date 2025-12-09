/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create alarm alert UI when countdown reaches zero
 *
 * 倒计时到达界面，包含：
 * - 稍后提醒圆角按钮（Snooze 5 min）
 * - 滑动 slider "滑动以停止"
 */
void create_alarm_alert_ui(void);

/**
 * @brief Destroy alarm alert UI and free resources
 */
void destroy_alarm_alert_ui(void);

#ifdef __cplusplus
}
#endif

