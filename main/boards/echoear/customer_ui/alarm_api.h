#ifndef MAIN_UI_API_H
#define MAIN_UI_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file alarm_api.h.h
 * @brief Public API for main UI operations
 *
 * This file provides the public interface for external modules to interact
 * with the main UI system, including pomodoro timer control.
 */

/**
 * @brief Initialize main UI
 *
 * Creates and registers all UI containers (pomodoro, sleep, time up).
 * This function should be called after the display is initialized.
 */
void alarm_create_ui(void);

/**
 * @brief Show pomodoro timer page with specified minutes
 *
 * Configures the pomodoro timer to the specified duration and switches to the pomodoro page.
 *
 * @param minutes Timer duration in minutes (defaults to 5 if <= 0)
 */
void alarm_start_pomodoro(int32_t minutes);

/**
 * @brief Start pomodoro timer
 *
 * Starts the pomodoro timer if it's paused or stopped.
 */
bool alarm_resume_pomodoro(void);

/**
 * @brief Pause pomodoro timer
 *
 * Pauses the pomodoro timer if it's currently running.
 */
bool alarm_pause_pomodoro(void);

#ifdef __cplusplus
}
#endif

#endif // MAIN_UI_API_H
