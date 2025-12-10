#include "main_ui.h"
#include "alarm_pomodoro.h"
#include "alarm_sleep_24h.h"
#include "alarm_end.h"
#include "../config.h"
#include "board.h"
#include "display/emote_display.h"
#include <esp_log.h>
#include <lvgl.h>
#include <esp_lv_adapter.h>
#include <mmap_generate_storage.h>

#define TAG "MainUI"

// ============================================================================
// Type Definitions
// ============================================================================

typedef struct {
    main_ui_page_t page;
    const char *name;
} main_ui_page_entry_t;

// ============================================================================
// Constants
// ============================================================================

/* Pages that participate in left/right swipe loop (with names for logging) */
static const main_ui_page_entry_t s_swipe_pages[] = {
    { MAIN_UI_PAGE_DUMMY,    "DUMMY"    },
    { MAIN_UI_PAGE_POMODORO, "POMODORO" },
    { MAIN_UI_PAGE_SLEEP,    "SLEEP"    },
};

#define MAIN_UI_SWIPE_PAGE_COUNT (sizeof(s_swipe_pages) / sizeof(s_swipe_pages[0]))

// ============================================================================
// Static Variables
// ============================================================================

/* UI container objects */
static lv_obj_t *container_emote_ui = NULL;
static lv_obj_t *container_pomodoro = NULL;
static lv_obj_t *container_sleep = NULL;
static lv_obj_t *container_time_up = NULL;

/* Current page state */
static main_ui_page_t s_current_page = MAIN_UI_PAGE_DUMMY;

/* Swipe detection state */
static bool s_swipe_active = false;
static bool s_swipe_handled = false;
static lv_coord_t s_swipe_start_x = 0;
static lv_coord_t s_swipe_start_y = 0;

// ============================================================================
// Global Variables
// ============================================================================

mmap_assets_handle_t asset_handle;

// ============================================================================
// Forward Declarations
// ============================================================================

static void page_gesture_event_cb(lv_event_t *e);

void main_ui_init_assets(void)
{
    const mmap_assets_config_t config = {
        .partition_label = "storage",
        .max_files = MMAP_STORAGE_FILES,
        .checksum = MMAP_STORAGE_CHECKSUM,
        .flags = {
            .mmap_enable = true,
            .app_bin_check = false,
            .full_check = true,
            .metadata_check = true,
        },
    };

    mmap_assets_new(&config, &asset_handle);

    int stored_files = mmap_assets_get_stored_files(asset_handle);
    ESP_LOGI(TAG, "stored_files:%d", stored_files);

    for (int i = 0; i < stored_files; i++) {
        const char *file_name = mmap_assets_get_name(asset_handle, i);
        ESP_LOGI(TAG, "file_name:%s, size:%d", file_name, mmap_assets_get_size(asset_handle, i));
    }
}

void main_ui_swipe_event_cb(lv_event_t *e)
{
    page_gesture_event_cb(e);
}

static void main_ui_trigger_emote_all_refresh(void)
{
    Display *base_display = Board::GetInstance().GetDisplay();
    if (!base_display) {
        ESP_LOGI(TAG, "Refresh all: base_display is nullptr");
        return;
    }

    auto *emote_display = dynamic_cast<emote::EmoteDisplay *>(base_display);
    if (!emote_display) {
        ESP_LOGI(TAG, "Refresh all: emote_display is nullptr");
        return;
    }

    ESP_LOGI(TAG, "Refresh all: emote_display is not nullptr");
    emote_display->RefreshAll();
    ESP_LOGI(TAG, "Refresh done");
}

static void main_ui_switch_page(main_ui_page_t page)
{
    /* Update current page state first so gesture logic sees the latest value on next swipe */
    s_current_page = page;

    esp_lv_adapter_lock(-1);

    lv_display_t *disp = lv_display_get_default();
    if (disp != nullptr) {
        bool enable_dummy = (page == MAIN_UI_PAGE_DUMMY);
        esp_lv_adapter_set_dummy_draw(disp, enable_dummy);
        // if (enable_dummy) {
        //     ESP_LOGI(TAG, "Emote all refresh");
        //     main_ui_trigger_emote_all_refresh();
        //     ESP_LOGI(TAG, "Refresh done");
        // }
    }

    /* Control visibility of each full-screen container */
    if (container_emote_ui) {
        if (page == MAIN_UI_PAGE_DUMMY) {
            lv_obj_clear_flag(container_emote_ui, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(container_emote_ui, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (container_pomodoro) {
        if (page == MAIN_UI_PAGE_POMODORO) {
            lv_obj_clear_flag(container_pomodoro, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(container_pomodoro, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (container_sleep) {
        if (page == MAIN_UI_PAGE_SLEEP) {
            lv_obj_clear_flag(container_sleep, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(container_sleep, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (container_time_up) {
        if (page == MAIN_UI_PAGE_TIME_UP) {
            lv_obj_clear_flag(container_time_up, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(container_time_up, LV_OBJ_FLAG_HIDDEN);
        }
    }

    esp_lv_adapter_unlock();
}

static void create_emote_ui_layer(void)
{
    lv_obj_t *scr = lv_scr_act();

    // Create full screen container_emote_ui on top
    container_emote_ui = lv_obj_create(scr);
    lv_obj_remove_style_all(container_emote_ui);
    lv_obj_set_size(container_emote_ui, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_align(container_emote_ui, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(container_emote_ui, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(container_emote_ui, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(container_emote_ui, LV_OBJ_FLAG_CLICKABLE);

    /* Enable swipe-based page switching on dummy page */
    lv_obj_add_flag(container_emote_ui, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(container_emote_ui, page_gesture_event_cb, LV_EVENT_ALL, NULL);

    ESP_LOGI(TAG, "Dummy monitor layer created");
}

static void create_alarm_containers(void)
{
    lv_obj_t *scr = lv_scr_act();
    container_pomodoro = lv_obj_create(scr);
    lv_obj_set_size(container_pomodoro, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(container_pomodoro, lv_color_black(), 0);
    lv_obj_set_style_border_width(container_pomodoro, 0, 0);
    lv_obj_set_style_pad_all(container_pomodoro, 0, 0);
    alarm_pomodoro_create_with_parent(container_pomodoro);

    /* Enable swipe-based page switching on pomodoro page */
    lv_obj_add_flag(container_pomodoro, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(container_pomodoro, page_gesture_event_cb, LV_EVENT_ALL, NULL);

    container_sleep = lv_obj_create(scr);
    lv_obj_set_size(container_sleep, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(container_sleep, lv_color_black(), 0);
    lv_obj_set_style_border_width(container_sleep, 0, 0);
    lv_obj_set_style_pad_all(container_sleep, 0, 0);
    alarm_sleep_24h_create_with_parent(container_sleep);

    /* Enable swipe-based page switching on sleep page */
    lv_obj_add_flag(container_sleep, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(container_sleep, page_gesture_event_cb, LV_EVENT_ALL, NULL);

    // Create alarm alert container (hidden by default)
    container_time_up = lv_obj_create(scr);
    lv_obj_set_size(container_time_up, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(container_time_up, lv_color_black(), 0);
    lv_obj_set_style_border_width(container_time_up, 0, 0);
    lv_obj_set_style_pad_all(container_time_up, 0, 0);
    alarm_time_up_create_with_parent(container_time_up);
}

static void page_gesture_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();

    switch (code) {
    case LV_EVENT_PRESSED:
        if (indev) {
            lv_point_t p;
            lv_indev_get_point(indev, &p);
            s_swipe_active = true;
            s_swipe_handled = false;
            s_swipe_start_x = p.x;
            s_swipe_start_y = p.y;
        }
        break;

    case LV_EVENT_PRESSING:
        if (!s_swipe_active || s_swipe_handled || !indev) {
            break;
        } else {
            lv_point_t p;
            lv_indev_get_point(indev, &p);
            lv_coord_t dx = p.x - s_swipe_start_x;
            lv_coord_t dy = p.y - s_swipe_start_y;

            /* Require mostly horizontal movement and a minimum distance */
            const lv_coord_t threshold = 40;
            if (LV_ABS(dx) < threshold || LV_ABS(dx) < LV_ABS(dy)) {
                break;
            }

            lv_dir_t dir = (dx < 0) ? LV_DIR_LEFT : LV_DIR_RIGHT;

            /* Use array round-robin to find next/prev page */
            int current_index = -1;
            for (unsigned int i = 0; i < MAIN_UI_SWIPE_PAGE_COUNT; ++i) {
                if (s_swipe_pages[i].page == s_current_page) {
                    current_index = (int)i;
                    break;
                }
            }

            /* If current page is not in swipe list (e.g. TIME_UP), ignore swipe */
            if (current_index < 0) {
                break;
            }

            int next_index = current_index;
            if (dir == LV_DIR_LEFT) {
                next_index = (current_index + 1) % MAIN_UI_SWIPE_PAGE_COUNT;
            } else if (dir == LV_DIR_RIGHT) {
                next_index = (current_index + MAIN_UI_SWIPE_PAGE_COUNT - 1) % MAIN_UI_SWIPE_PAGE_COUNT;
            }

            main_ui_page_t target_page = s_swipe_pages[next_index].page;

            ESP_LOGI(TAG, "dir=%s, current=%s (%d) -> next=%s (%d)",
                     (dir == LV_DIR_LEFT) ? "LEFT" : "RIGHT",
                     s_swipe_pages[current_index].name, current_index,
                     s_swipe_pages[next_index].name, next_index);

            s_swipe_handled = true;

            /* If swipe targets pomodoro page, use 5-minute default entry */
            if (target_page == MAIN_UI_PAGE_POMODORO && s_current_page != MAIN_UI_PAGE_POMODORO) {
                main_ui_show_pomodoro_with_minutes(5);
            } else {
                main_ui_switch_page(target_page);
            }
        }
        break;

    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        s_swipe_active = false;
        s_swipe_handled = false;
        break;

    default:
        break;
    }
}

void create_main_ui(void)
{
    main_ui_init_assets();

    lv_display_t *disp = lv_display_get_default();
    if (disp == nullptr) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);

    create_alarm_containers();
    create_emote_ui_layer();

    /* Default entry page: dummy monitor */
    main_ui_switch_page(MAIN_UI_PAGE_DUMMY);

    ESP_LOGI(TAG, "Main UI created");
}

void alarm_pomodoro_show(void)
{
    ESP_LOGI(TAG, "Pomodoro timer UI shown");
    main_ui_switch_page(MAIN_UI_PAGE_POMODORO);
}

void alarm_pomodoro_hide(void)
{
    ESP_LOGI(TAG, "Pomodoro timer UI hidden");
    esp_lv_adapter_lock(-1);
    if (container_pomodoro) {
        lv_obj_add_flag(container_pomodoro, LV_OBJ_FLAG_HIDDEN);
    }
    esp_lv_adapter_unlock();
}

void alarm_time_up_show(void)
{
    ESP_LOGI(TAG, "Alarm time up UI shown");
    main_ui_switch_page(MAIN_UI_PAGE_TIME_UP);
}

void alarm_time_up_hide(void)
{
    ESP_LOGI(TAG, "Alarm time up UI hidden");
    esp_lv_adapter_lock(-1);
    if (container_time_up) {
        lv_obj_add_flag(container_time_up, LV_OBJ_FLAG_HIDDEN);
    }
    esp_lv_adapter_unlock();
}

void main_ui_show_pomodoro_with_minutes(int32_t minutes)
{
    if (minutes <= 0) {
        minutes = 5;
    }

    ESP_LOGI(TAG, "Show pomodoro page with %ld minutes", (long)minutes);

    /* Configure pomodoro timer first, then switch page */
    alarm_pomodoro_reset_to_zero();
    alarm_pomodoro_adjust_end_point(minutes);
    main_ui_switch_page(MAIN_UI_PAGE_POMODORO);
}

void main_ui_show_dummy_page(void)
{
    ESP_LOGI(TAG, "Switch to dummy page");
    main_ui_switch_page(MAIN_UI_PAGE_DUMMY);
}

size_t main_ui_get_assets_size(void)
{
    return mmap_assets_get_size(asset_handle, MMAP_STORAGE_CLOCK_LOOP_EAF);
}

const uint8_t* main_ui_get_assets_data(void)
{
    return (const uint8_t*)mmap_assets_get_mem(asset_handle, MMAP_STORAGE_CLOCK_LOOP_EAF);
}

main_ui_page_t main_ui_get_current_page(void)
{
    return s_current_page;
}
