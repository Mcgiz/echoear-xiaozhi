#include <string.h>  /* For strcmp */
#include <esp_log.h>
#include <esp_lv_adapter.h>
#include <mmap_generate_storage.h>
#include "alarm_manager.h"
#include "alarm_api.h"

#define TAG "alarm_manager"

// ============================================================================
// Type Definitions
// ============================================================================

// ============================================================================
// Static Variables
// ============================================================================

/* UI container objects (main_ui specific) */
static lv_obj_t *container_pomodoro = NULL;
static lv_obj_t *container_sleep = NULL;
static lv_obj_t *container_time_up = NULL;

// ============================================================================
// Global Variables
// ============================================================================

void main_ui_switch_page(const char *page_name)
{
    ESP_LOGD(TAG, "Switching to page: %s", page_name);
    ui_bridge_switch_page(page_name);
}

/* Page switch callback to handle pomodoro special case */
static bool main_ui_page_switch_callback(const char *target_page, void *user_data)
{
    ESP_LOGD(TAG, "Page switch callback: %s", target_page);
    const char *current_page = ui_bridge_get_current_page();

    /* Special handling for pomodoro page */
    if (target_page != NULL && strcmp(target_page, PAGE_POMODORO) == 0 &&
            (current_page == NULL || strcmp(current_page, PAGE_POMODORO) != 0)) {
        alarm_start_pomodoro(5);
        return true;  /* Handled, skip default switch */
    }

    return false;  /* Use default switch */
}

void alarm_create_ui()
{
    lv_obj_t *scr = lv_scr_act();
    /* Create and register pomodoro container */
    container_pomodoro = alarm_pomodoro_create_with_parent(scr);
    ui_bridge_register_page_with_cycle(PAGE_POMODORO, &container_pomodoro, true);

    /* Create and register sleep container */
    container_sleep = alarm_sleep_24h_create_with_parent(scr);
    ui_bridge_register_page_with_cycle(PAGE_SLEEP, &container_sleep, true);

    /* Create and register time up container */
    container_time_up = alarm_time_up_create_with_parent(scr);
    ui_bridge_register_page_with_cycle(PAGE_TIME_UP, &container_time_up, false);

    /* Register page switch callback for custom handling (e.g., pomodoro) */
    ui_bridge_set_page_switch_callback(main_ui_page_switch_callback, NULL);
}

void alarm_start_pomodoro(int32_t minutes)
{
    if (minutes <= 0) {
        minutes = 5;
    }

    ESP_LOGI(TAG, "Show pomodoro page with %ld minutes", (long)minutes);

    /* Configure pomodoro timer first, then switch page */
    alarm_pomodoro_reset_to_zero();
    alarm_pomodoro_adjust_end_point(minutes);
    main_ui_switch_page(PAGE_POMODORO);
}

bool alarm_resume_pomodoro(void)
{
    ESP_LOGI(TAG, "Start pomodoro timer");
    const char *current_page = ui_bridge_get_current_page();
    if (current_page == NULL || strcmp(current_page, PAGE_POMODORO) != 0) {
        ESP_LOGI(TAG, "Not on pomodoro page (current=%s)",
                 current_page ? current_page : "NULL");
        return false;
    } else {
        alarm_pomodoro_start();
    }
    return true;
}

bool alarm_pause_pomodoro(void)
{
    ESP_LOGI(TAG, "Pause pomodoro timer");
    const char *current_page = ui_bridge_get_current_page();
    if (current_page == NULL || strcmp(current_page, PAGE_POMODORO) != 0) {
        ESP_LOGI(TAG, "Not on pomodoro page (current=%s)",
                 current_page ? current_page : "NULL");
        return false;
    } else {
        alarm_pomodoro_pause();
    }
    return true;
}
