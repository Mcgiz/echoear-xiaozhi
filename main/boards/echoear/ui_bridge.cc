#include "config.h"
#include "ui_bridge.h"
#include "board.h"
#include "display/emote_display.h"
#include "customer_ui/alarm_api.h"
#include <esp_log.h>
#include <lvgl.h>
#include <esp_lv_adapter.h>
// #include <stdlib.h>  /* For malloc/free */
// #include <stdbool.h>  /* For bool type */
// #include <string.h>   /* For strcmp */

#define TAG "ui_bridge"

/* Gesture detection state structure */
typedef struct {
    bool active;
    bool handled;
    lv_coord_t start_x;
    lv_coord_t start_y;
    uint32_t press_start_time;
} lvgl_bridge_gesture_state_t;

/* Gesture detection state */
static lvgl_bridge_gesture_state_t s_gesture_state = {
    .active = false,
    .handled = false,
    .start_x = 0,
    .start_y = 0,
    .press_start_time = 0,
};

/* Page management using linked list */
typedef struct lvgl_bridge_page_node {
    const char *page_id;
    lv_obj_t **container;
    struct lvgl_bridge_page_node *next;
} lvgl_bridge_page_node_t;

static lvgl_bridge_page_node_t *s_page_list = NULL;  /* Linked list head */
static const char *s_current_page = NULL;
static lvgl_bridge_page_switch_cb_t s_page_switch_cb = NULL;
static void *s_page_switch_user_data = NULL;

/* Base emote UI container */
static lv_obj_t *s_base_container = NULL;

/* Cached display pointer for emote refresh */
static emote::EmoteDisplay *s_cached_emote_display = nullptr;

/* Forward declarations */
static void lvgl_bridge_handle_gesture_navigation(lvgl_bridge_gesture_type_t gesture_type);
static void lvgl_bridge_refresh_emote_display(void);

/* Touch gesture event callback */
static void lvgl_bridge_gesture_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    lvgl_bridge_gesture_state_t *state = &s_gesture_state;

    /* Suppress warning for unhandled enum values - we only care about touch events */
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wswitch"
    switch (code) {
    case LV_EVENT_PRESSED:
        if (indev) {
            lv_point_t p;
            lv_indev_get_point(indev, &p);
            state->active = true;
            state->handled = false;
            state->start_x = p.x;
            state->start_y = p.y;
            state->press_start_time = lv_tick_get();
            ESP_LOGD(TAG, "press at (%ld, %ld)", (long)p.x, (long)p.y);
        } else {
            ESP_LOGW(TAG, "press but indev is NULL");
        }
        break;

    case LV_EVENT_PRESSING: {
        if (!state->active || state->handled || !indev) {
            break;
        }
        
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        lv_coord_t dx = p.x - state->start_x;
        lv_coord_t dy = p.y - state->start_y;
        
        /* Log movement for debugging */
        static lv_coord_t last_dx = 0, last_dy = 0;
        if (LV_ABS(dx - last_dx) > 5 || LV_ABS(dy - last_dy) > 5) {
            ESP_LOGD(TAG, "PRESSING: dx=%ld, dy=%ld, threshold=%d", 
                     (long)dx, (long)dy, UI_BRIDGE_GESTURE_SWIPE_THRESHOLD);
            last_dx = dx;
            last_dy = dy;
        }

        /* Detect swipe gestures during pressing */
        if (LV_ABS(dx) >= UI_BRIDGE_GESTURE_SWIPE_THRESHOLD || 
            LV_ABS(dy) >= UI_BRIDGE_GESTURE_SWIPE_THRESHOLD) {
            lvgl_bridge_gesture_type_t gesture = UI_BRIDGE_GESTURE_NONE;
            
            /* Determine swipe direction based on dominant axis */
            if (LV_ABS(dx) > LV_ABS(dy)) {
                /* Horizontal swipe */
                if (dx < 0) {
                    gesture = UI_BRIDGE_GESTURE_SWIPE_LEFT;
                } else {
                    gesture = UI_BRIDGE_GESTURE_SWIPE_RIGHT;
                }
            } else {
                /* Vertical swipe */
                if (dy < 0) {
                    gesture = UI_BRIDGE_GESTURE_SWIPE_UP;
                } else {
                    gesture = UI_BRIDGE_GESTURE_SWIPE_DOWN;
                }
            }
            
            if (gesture != UI_BRIDGE_GESTURE_NONE) {
                ESP_LOGD(TAG, "swipe detected: %d", gesture);
                /* Handle page navigation directly */
                lvgl_bridge_handle_gesture_navigation(gesture);
                state->handled = true;
            }
        }
        break;
    }

    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST: {
        if (!state->active || !indev) {
            state->active = false;
            state->handled = false;
            break;
        }
        
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        lv_coord_t dx = p.x - state->start_x;
        lv_coord_t dy = p.y - state->start_y;
        uint32_t press_duration = lv_tick_elaps(state->press_start_time);
        
        ESP_LOGD(TAG, "release: dx=%ld, dy=%ld, duration=%lu ms, handled=%d", 
                 (long)dx, (long)dy, press_duration, state->handled);
        
        /* Check for swipe gesture on release (if not already handled) */
        if (!state->handled) {
            if (LV_ABS(dx) >= UI_BRIDGE_GESTURE_SWIPE_THRESHOLD || 
                LV_ABS(dy) >= UI_BRIDGE_GESTURE_SWIPE_THRESHOLD) {
                lvgl_bridge_gesture_type_t gesture = UI_BRIDGE_GESTURE_NONE;
                
                /* Determine swipe direction based on dominant axis */
                if (LV_ABS(dx) > LV_ABS(dy)) {
                    /* Horizontal swipe */
                    if (dx < 0) {
                        gesture = UI_BRIDGE_GESTURE_SWIPE_LEFT;
                    } else {
                        gesture = UI_BRIDGE_GESTURE_SWIPE_RIGHT;
                    }
                } else {
                    /* Vertical swipe */
                    if (dy < 0) {
                        gesture = UI_BRIDGE_GESTURE_SWIPE_UP;
                    } else {
                        gesture = UI_BRIDGE_GESTURE_SWIPE_DOWN;
                    }
                }
                
                if (gesture != UI_BRIDGE_GESTURE_NONE) {
                    ESP_LOGD(TAG, "swipe detected: %d", gesture);
                    /* Handle page navigation directly */
                    lvgl_bridge_handle_gesture_navigation(gesture);
                    state->handled = true;
                }
            } else {
                /* It's a press (not a swipe) */
                lvgl_bridge_gesture_type_t gesture;
                if (press_duration >= UI_BRIDGE_GESTURE_LONG_PRESS_TIME_MS) {
                    gesture = UI_BRIDGE_GESTURE_LONG_PRESS;
                } else {
                    gesture = UI_BRIDGE_GESTURE_SHORT_PRESS;
                }
                ESP_LOGD(TAG, "press detected: %d (duration: %lu ms)", gesture, press_duration);
            }
        }
        
        state->active = false;
        state->handled = false;
        break;
    }

    default:
        /* Ignore all other events (LV_EVENT_FLUSH_WAIT_START, LV_EVENT_VSYNC, etc.) */
        break;
    }
    #pragma GCC diagnostic pop
}

/* Internal function to refresh emote display */
static void lvgl_bridge_refresh_emote_display(void)
{
    if (s_cached_emote_display == nullptr) {
        Display *base_display = Board::GetInstance().GetDisplay();
        if (!base_display) {
            ESP_LOGI(TAG, "Refresh all: base_display is nullptr");
            return;
        }
        s_cached_emote_display = dynamic_cast<emote::EmoteDisplay *>(base_display);
        if (!s_cached_emote_display) {
            ESP_LOGI(TAG, "Refresh all: emote_display is nullptr");
            return;
        }
    }
    
    if (s_cached_emote_display) {
        s_cached_emote_display->RefreshAll();
    }
}

/* Internal function to handle gesture-based page navigation */
static void lvgl_bridge_handle_gesture_navigation(lvgl_bridge_gesture_type_t gesture_type)
{
    /* Map gesture to page direction */
    int direction = 0;
    const char *gesture_name = NULL;
    
    switch (gesture_type) {
    case UI_BRIDGE_GESTURE_SWIPE_LEFT:
    case UI_BRIDGE_GESTURE_SWIPE_DOWN:
        direction = -1;  /* Previous page */
        gesture_name = (gesture_type == UI_BRIDGE_GESTURE_SWIPE_LEFT) ? "LEFT" : "DOWN";
        break;
    case UI_BRIDGE_GESTURE_SWIPE_RIGHT:
    case UI_BRIDGE_GESTURE_SWIPE_UP:
        direction = 1;   /* Next page */
        gesture_name = (gesture_type == UI_BRIDGE_GESTURE_SWIPE_RIGHT) ? "RIGHT" : "UP";
        break;
    default:
        return;
    }

    /* Check if page containers are registered */
    if (s_page_list == NULL) {
        ESP_LOGW(TAG, "No page containers registered");
        return;
    }

    /* Count total pages and find current page index */
    unsigned int total_count = 0;
    int current_index = -1;
    lvgl_bridge_page_node_t *node = s_page_list;
    while (node != NULL) {
        if (node->page_id == s_current_page) {
            current_index = (int)total_count;
        }
        total_count++;
        node = node->next;
    }

    /* If current page is not in registered list, ignore gesture */
    if (current_index < 0 || total_count == 0) {
        return;
    }

    /* Calculate next page index with round-robin */
    int next_index = (current_index + direction + (int)total_count) % (int)total_count;
    
    /* Find target page by index */
    const char *target_page = NULL;
    const char *current_name = "UNKNOWN";
    const char *next_name = "UNKNOWN";
    node = s_page_list;
    int index = 0;
    while (node != NULL) {
        if (index == current_index) {
            current_name = node->page_id ? node->page_id : "UNKNOWN";
        }
        if (index == next_index) {
            target_page = node->page_id;
            next_name = node->page_id ? node->page_id : "UNKNOWN";
        }
        index++;
        node = node->next;
    }

    ESP_LOGI(TAG, "SWIPE_%s: %s (%d) -> %s (%d)",
             gesture_name, current_name, current_index, next_name, next_index);

    /* Call custom callback if set */
    if (s_page_switch_cb && s_page_switch_cb(target_page, s_page_switch_user_data)) {
        return;
    }

    /* Default page switching */
    lvgl_bridge_switch_page(target_page);
}

/* Public API implementation */
void lvgl_bridge_init(Display *display)
{
    /* Cache display pointer */
    if (display) {
        s_cached_emote_display = dynamic_cast<emote::EmoteDisplay *>(display);
        if (s_cached_emote_display) {
            ESP_LOGI(TAG, "Cached emote display pointer: %p", (void*)s_cached_emote_display);
        }
    }

    /* Create base emote UI container */
    lv_obj_t *scr = lv_scr_act();
    s_base_container = lv_obj_create(scr);
    lv_obj_remove_style_all(s_base_container);
    lv_obj_set_size(s_base_container, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_align(s_base_container, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(s_base_container, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_base_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_base_container, LV_OBJ_FLAG_CLICKABLE);
    
    /* Register base container as default page */
    lvgl_bridge_register_page(UI_BRIDGE_PAGE_HOME, &s_base_container);
    lvgl_bridge_switch_page(UI_BRIDGE_PAGE_HOME);  /* Set as default page */

    /* Create main UI (which will register its own pages) */
    alarm_create_ui();
    
    ESP_LOGI(TAG, "LVGL display bridge initialized for %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

void lvgl_bridge_attach_gesture_handler(lv_indev_t *indev)
{
    ESP_LOGI(TAG, "Attaching gesture handler to input device: %p", (void*)indev);
    lv_indev_add_event_cb(indev, lvgl_bridge_gesture_event_cb, LV_EVENT_ALL, NULL);
}

bool lvgl_bridge_register_page(const char *page_id, lv_obj_t **container)
{
    if (page_id == NULL) {
        ESP_LOGE(TAG, "Page ID cannot be NULL");
        return false;
    }
    
    /* Check if already registered */
    lvgl_bridge_page_node_t *node = s_page_list;
    while (node != NULL) {
        if (node->page_id != NULL && strcmp(node->page_id, page_id) == 0) {
            ESP_LOGW(TAG, "Page container '%s' already registered, updating", page_id);
            node->container = container;
            return true;
        }
        node = node->next;
    }
    
    /* Create new node */
    lvgl_bridge_page_node_t *new_node = (lvgl_bridge_page_node_t *)malloc(sizeof(lvgl_bridge_page_node_t));
    if (new_node == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for page container node");
        return false;
    }
    
    new_node->page_id = page_id;
    new_node->container = container;
    new_node->next = s_page_list;  /* Insert at head */
    s_page_list = new_node;
    
    /* Count nodes for logging */
    unsigned int count = 0;
    node = s_page_list;
    while (node != NULL) {
        count++;
        node = node->next;
    }
    ESP_LOGD(TAG, "Registered page container for page '%s' (total: %u)", page_id, count);
    return true;
}

void lvgl_bridge_switch_page(const char *page_id)
{
    if (page_id == NULL) {
        ESP_LOGW(TAG, "Cannot switch to NULL page");
        return;
    }
    
    /* Update current page state */
    s_current_page = page_id;

    esp_lv_adapter_lock(-1);

    /* Set dummy draw mode for home page */
    lv_display_t *disp = lv_display_get_default();
    bool enable_dummy = (strcmp(page_id, UI_BRIDGE_PAGE_HOME) == 0);
    if (disp != nullptr) {
        esp_lv_adapter_set_dummy_draw(disp, enable_dummy);
    }

    /* Control visibility of all registered containers */
    lvgl_bridge_page_node_t *node = s_page_list;
    while (node != NULL) {
        lv_obj_t *container = *node->container;
        if (container != NULL) {
            if (node->page_id != NULL && strcmp(node->page_id, page_id) == 0) {
                lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
            }
        }
        node = node->next;
    }

    esp_lv_adapter_unlock();

    /* Refresh emote display if switching to home page */
    if (enable_dummy) {
        lvgl_bridge_refresh_emote_display();
    }
}

const char *lvgl_bridge_get_current_page(void)
{
    return s_current_page;
}

void lvgl_bridge_set_page_switch_callback(lvgl_bridge_page_switch_cb_t cb, void *user_data)
{
    s_page_switch_cb = cb;
    s_page_switch_user_data = user_data;
}

