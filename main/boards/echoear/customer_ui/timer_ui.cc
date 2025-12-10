/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "lvgl.h"
#include "lv_eaf.h"
#include "esp_log.h"
#include <stdio.h>
#include <math.h>
#include "esp_lv_adapter.h"
#include "mmap_generate_storage.h"
#include "timer_range_ui.h"
#include "timer_ui.h"
#include "alarm_alert_ui.h"
#include "main_ui.h"

#define SCREEN_WIDTH    360
#define SCREEN_HEIGHT   360

#define TIMER_MAX_SECONDS       (60 * 60)  // 60 minutes max (one full circle)
// Set this to 0 if you want start_point == end_point and countdown == 0 on first create
#define TIMER_INITIAL_SECONDS   (0 * 60)   // initial countdown time
#define TIMER_UPDATE_PERIOD_MS  1000       // Update every second

static const char *TAG = "timer_ui";

/* Declare fonts */
LV_FONT_DECLARE(lv_font_montserrat_40);

/* Define asset_handle if not defined elsewhere */
mmap_assets_handle_t asset_handle = nullptr;

LV_IMG_DECLARE(time_start);
LV_IMG_DECLARE(time_end);
LV_IMG_DECLARE(time_sleep);
LV_IMG_DECLARE(time_arc_texture);
LV_IMG_DECLARE(clock_loop);

// Timer state
typedef enum {
    TIMER_STATE_RUNNING = 0,
    TIMER_STATE_END,
    TIMER_STATE_PAUSED,
} timer_state_t;

// UI components
typedef struct {
    lv_obj_t *container;
    lv_obj_t *bg_arc;           // Background gray arc
    lv_obj_t *progress_arc;     // Progress arc
    lv_obj_t *time_container;   // Container for time labels
    lv_obj_t *time_min_label;   // "MM"
    lv_obj_t *time_colon_label; // ":"
    lv_obj_t *time_sec_label;   // "SS"
    lv_obj_t *start_knob_img;   // Start point knob image
    lv_obj_t *end_knob_img;     // End point knob image
    lv_obj_t *eaf_anim;         // EAF animation object
    lv_timer_t *timer;          // LVGL timer
    int32_t remaining_seconds;  // Remaining time in seconds
    timer_state_t state;        // Timer state: running or end
    bool was_running_before_zero; // Flag to track if timer was running when it reached zero
} timer_ui_t;

static timer_ui_t timer_ui;

// For tabview mode: track which UI is currently shown
typedef enum {
    TIMER_MODE_COUNTDOWN = 0,
    TIMER_MODE_RANGE,
} timer_mode_t;

static timer_mode_t current_timer_mode = TIMER_MODE_COUNTDOWN;
// timer_container and range_container are now created in main_ui.cc
// Access them via main_ui_get_timer_container() and main_ui_get_range_container()


static lv_image_dsc_t s_test_eaf_dsc;

// Helper to change state and sync EAF animation
static void timer_set_state(timer_ui_t *ui, timer_state_t new_state)
{
    if (ui->state == new_state) {
        return;
    }

    ui->state = new_state;

    if (ui->eaf_anim) {
        if (new_state == TIMER_STATE_END || new_state == TIMER_STATE_PAUSED) {
            lv_eaf_pause(ui->eaf_anim);
            ESP_LOGI(TAG, "Timer state -> END, pause EAF");
        } else {
            lv_eaf_resume(ui->eaf_anim);
            ESP_LOGI(TAG, "Timer state -> RUNNING, resume EAF");
        }
    }
}

// We'll create simple circular knob objects instead of using images
// This is simpler and more compatible with LVGL 9

// Calculate angle from center to point
static float calculate_angle_from_center(lv_coord_t center_x, lv_coord_t center_y, lv_coord_t point_x, lv_coord_t point_y)
{
    float dx = point_x - center_x;
    float dy = center_y - point_y;  // Invert Y because screen Y increases downward
    float angle_rad = atan2(dy, dx);
    float angle_deg = angle_rad * 180.0f / M_PI;

    // Convert to LVGL's coordinate system (0° = top, clockwise)
    // Math: 0° = right, counter-clockwise
    // LVGL arc with rotation=90: 0° = top (12 o'clock), clockwise
    angle_deg = 90.0f - angle_deg;

    // Normalize to 0-360
    if (angle_deg < 0) {
        angle_deg += 360.0f;
    }

    return angle_deg;
}

// Update knob positions based on arc angles
static void update_knob_positions(timer_ui_t *ui)
{
    if (!ui->progress_arc || !ui->start_knob_img || !ui->end_knob_img) {
        return;
    }

    // Get arc parameters
    lv_coord_t arc_size = lv_obj_get_width(ui->progress_arc);
    lv_coord_t arc_width = 30;  // Arc line width
    lv_coord_t radius = arc_size / 2 - arc_width / 2;  // Radius to center of arc line
    lv_coord_t center_x = arc_size / 2;
    lv_coord_t center_y = arc_size / 2;

    // Get arc position in container
    lv_coord_t arc_x = lv_obj_get_x(ui->progress_arc);
    lv_coord_t arc_y = lv_obj_get_y(ui->progress_arc);

    // Get knob sizes from their actual images
    lv_coord_t start_w = lv_obj_get_width(ui->start_knob_img);
    lv_coord_t start_h = lv_obj_get_height(ui->start_knob_img);
    lv_coord_t end_w   = lv_obj_get_width(ui->end_knob_img);
    lv_coord_t end_h   = lv_obj_get_height(ui->end_knob_img);

    // Start knob is always at top (0° in LVGL arc with rotation=270)
    // LVGL arc: 0° is at top (12 o'clock) after rotation=270, goes clockwise
    // With rotation=270: 0° LVGL = 90° math (top in standard coordinates)
    float start_angle_lvgl = 0.0f;  // Always at top
    float start_rad = (90.0f - start_angle_lvgl) * M_PI / 180.0f;  // 90° in math = top
    lv_coord_t start_x = arc_x + center_x + (lv_coord_t)(radius * cos(start_rad)) - start_w / 2;
    lv_coord_t start_y = arc_y + center_y - (lv_coord_t)(radius * sin(start_rad)) - start_h / 2;

    // End knob position based on current remaining time mapped to angle
    // (keep it consistent with how the arc itself is updated)
    int16_t end_angle_lvgl = (360 * ui->remaining_seconds) / TIMER_MAX_SECONDS;
    // Convert from LVGL angle to math angle
    float end_rad = (90.0f - end_angle_lvgl) * M_PI / 180.0f;
    lv_coord_t end_x = arc_x + center_x + (lv_coord_t)(radius * cos(end_rad)) - end_w / 2;
    lv_coord_t end_y = arc_y + center_y - (lv_coord_t)(radius * sin(end_rad)) - end_h / 2;

    // Update knob positions
    lv_obj_set_pos(ui->start_knob_img, start_x, start_y);
    lv_obj_set_pos(ui->end_knob_img, end_x, end_y);

    // ESP_LOGI(TAG, "start_knob_img, %d, %d", start_x, start_y);
    // ESP_LOGI(TAG, "end_knob_img: %d, %d", end_x, end_y);

    // ESP_LOGI(TAG, "Knob positions - End angle: %d°, End pos: (%d, %d)", end_angle_lvgl, end_x, end_y);
}

// Update time from angle (for drag operations)
static void update_time_from_angle(timer_ui_t *ui, int32_t angle_deg)
{
    // Limit angle to 0-360 (0-60 minutes)
    if (angle_deg < 0) {
        angle_deg = 0;
    }
    if (angle_deg > 360) {
        angle_deg = 360;
    }

    // Calculate remaining seconds from angle (360° = 60 minutes, 0° = 0 minutes)
    ui->remaining_seconds = (angle_deg * TIMER_MAX_SECONDS) / 360;

    // Update state by remaining time:
    // start point == end point -> angle == 0 -> remaining_seconds == 0 -> END state, animation stopped
    if (ui->remaining_seconds <= 0) {
        ui->remaining_seconds = 0;
        timer_set_state(ui, TIMER_STATE_END);
        // Clear flag when manually set to 0, so it won't trigger jump
        ui->was_running_before_zero = false;
    } else {
        timer_set_state(ui, TIMER_STATE_RUNNING);
    }

    // Update time display
    int minutes = ui->remaining_seconds / 60;
    int seconds = ui->remaining_seconds % 60;
    if (ui->time_min_label && ui->time_sec_label) {
        lv_label_set_text_fmt(ui->time_min_label, "%02d", minutes);
        lv_label_set_text_fmt(ui->time_sec_label, "%02d", seconds);
    }

    lv_arc_set_angles(ui->progress_arc, 0, angle_deg);

    // Update knob positions
    update_knob_positions(ui);
}

// Event handler for end knob dragging
static void end_knob_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    timer_ui_t *ui = (timer_ui_t *)lv_event_get_user_data(e);

    if (code == LV_EVENT_PRESSING || code == LV_EVENT_PRESSED) {
        // Pause timer during drag
        if (code == LV_EVENT_PRESSED) {
            if (ui->timer) {
                lv_timer_pause(ui->timer);
            }
        }

        // Get touch point (absolute screen coordinates)
        lv_indev_t *indev = lv_indev_get_act();
        if (indev == NULL) {
            return;
        }

        lv_point_t point;
        lv_indev_get_point(indev, &point);

        // Calculate center of arc in absolute screen coordinates
        lv_obj_t *arc = ui->progress_arc;
        lv_coord_t center_x = 0;
        lv_coord_t center_y = 0;

        // Get absolute position by traversing parent hierarchy
        lv_obj_t *obj = arc;
        while (obj != NULL) {
            center_x += lv_obj_get_x(obj);
            center_y += lv_obj_get_y(obj);
            obj = lv_obj_get_parent(obj);
            if (obj == lv_scr_act()) {
                break;
            }
        }
        center_x += lv_obj_get_width(arc) / 2;
        center_y += lv_obj_get_height(arc) / 2;

        // Calculate angle from touch point
        float angle = calculate_angle_from_center(center_x, center_y, point.x, point.y);

        // Limit angle to 0-360
        if (angle < 0) {
            angle = 0;
        }
        if (angle > 360) {
            angle = 360;
        }

        // ESP_LOGI(TAG, "Dragging end knob to angle: %.1f°", angle);

        // Update time and UI - this will update knob positions
        update_time_from_angle(ui, (int32_t)angle);

    } else if (code == LV_EVENT_RELEASED) {
        // Resume timer when drag ends (if time remaining and was running)
        if (ui->timer && ui->remaining_seconds > 0) {
            lv_timer_resume(ui->timer);
        }
    }
}

// Start knob is fixed, no dragging allowed
// This event handler is kept for potential future use (e.g., visual feedback on touch)
static void start_knob_event_handler(lv_event_t *e)
{
    // Start knob is fixed at top position, no dragging
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        // Visual feedback: slightly scale up
        lv_obj_t *knob = (lv_obj_t *)lv_event_get_target(e);
        lv_obj_set_style_transform_scale(knob, 280, 0);  // 110% scale
    } else if (code == LV_EVENT_RELEASED) {
        // Return to normal size
        lv_obj_t *knob = (lv_obj_t *)lv_event_get_target(e);
        lv_obj_set_style_transform_scale(knob, 256, 0);  // 100% scale
    }
}

// Timer callback to update countdown
static void timer_tick_cb(lv_timer_t *timer)
{
    timer_ui_t *ui = (timer_ui_t *)lv_timer_get_user_data(timer);

    if (ui->remaining_seconds > 0) {
        ui->remaining_seconds--;
        // Mark that timer was running (for detecting natural countdown finish)
        ui->was_running_before_zero = true;

        // Update time display
        int minutes = ui->remaining_seconds / 60;
        int seconds = ui->remaining_seconds % 60;
        if (ui->time_min_label && ui->time_sec_label) {
            lv_label_set_text_fmt(ui->time_min_label, "%02d", minutes);
            lv_label_set_text_fmt(ui->time_sec_label, "%02d", seconds);
        }

        // Update arc progress (0-360 degrees, based on 60 minute max)
        int32_t progress = (360 * ui->remaining_seconds) / TIMER_MAX_SECONDS;
        // Use (progress, 0) so that the visible arc is clockwise from 12 o'clock
        // lv_arc_set_angles(ui->progress_arc, progress, 0);
        lv_arc_set_angles(ui->progress_arc, 0, progress);
        // ESP_LOGI(TAG, "Progress: %d°, remaining seconds: %d:%d", progress, ui->remaining_seconds / 60, ui->remaining_seconds % 60);

        // We are in running state while remaining time > 0
        timer_set_state(ui, TIMER_STATE_RUNNING);

        // Update knob positions
        update_knob_positions(ui);

    } else {
        // Timer finished
        if (ui->time_min_label && ui->time_sec_label) {
            lv_label_set_text(ui->time_min_label, "00");
            lv_label_set_text(ui->time_sec_label, "00");
        }
        // Set END state when countdown reaches zero
        timer_set_state(ui, TIMER_STATE_END);
        ESP_LOGI(TAG, "Timer finished!");
        lv_timer_pause(timer);
        
        // Only jump to alarm alert UI if timer was running and naturally counted down to zero
        // Don't jump if timer was manually set to 0 or initialized to 0
        if (ui->was_running_before_zero) {
            // Timer was running and naturally counted down to zero
            ESP_LOGI(TAG, "Timer countdown finished naturally - jump to alarm alert UI");
            timer_ui_hide();
            create_alarm_alert_ui();
            // Reset flag after jumping
            ui->was_running_before_zero = false;
        } else {
            // Timer was manually set to 0 or initialized to 0, don't jump
            ESP_LOGI(TAG, "Timer set to 0 manually or initialized to 0 - no jump");
        }
    }
}

// Internal function that accepts parent
static void create_timer_ui_internal(lv_obj_t *parent)
{
    // Initialize timer UI structure
    timer_ui.remaining_seconds = TIMER_INITIAL_SECONDS;
    timer_ui.state = (timer_ui.remaining_seconds > 0) ? TIMER_STATE_RUNNING : TIMER_STATE_END;
    timer_ui.eaf_anim = NULL;
    timer_ui.was_running_before_zero = false;  // Initialize flag

    // Create container
    timer_ui.container = lv_obj_create(parent);
    // Use percentage size for tabview compatibility, or fixed size if parent is screen
    if (parent == lv_scr_act()) {
        lv_obj_set_size(timer_ui.container, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_center(timer_ui.container);
    } else {
        // For tabview, use full size and let parent manage layout
        lv_obj_set_size(timer_ui.container, LV_PCT(100), LV_PCT(100));
    }
    lv_obj_set_style_bg_color(timer_ui.container, lv_color_black(), 0);
    lv_obj_set_style_border_width(timer_ui.container, 0, 0);
    lv_obj_set_style_pad_all(timer_ui.container, 0, 0);
    // Don't block parent scrolling
    lv_obj_clear_flag(timer_ui.container, LV_OBJ_FLAG_SCROLLABLE);

#if 1
    // Background image
    // lv_obj_t *bg_img = lv_img_create(timer_ui.container);
    // lv_img_set_src(bg_img, &clock_loop);
    // lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 45);
#else
    timer_ui.eaf_anim = lv_eaf_create(timer_ui.container);
    lv_obj_align(timer_ui.eaf_anim, LV_ALIGN_CENTER, 0, 50);

    s_test_eaf_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_test_eaf_dsc.header.cf = LV_COLOR_FORMAT_RAW;
    s_test_eaf_dsc.header.flags = 0;
    s_test_eaf_dsc.header.w = 1;
    s_test_eaf_dsc.header.h = 1;
    s_test_eaf_dsc.header.stride = 0;
    s_test_eaf_dsc.header.reserved_2 = 0;
    s_test_eaf_dsc.data_size = mmap_assets_get_size(asset_handle, MMAP_STORAGE_CLOCK_LOOP_EAF);
    s_test_eaf_dsc.data = mmap_assets_get_mem(asset_handle, MMAP_STORAGE_CLOCK_LOOP_EAF);

    lv_eaf_set_src(timer_ui.eaf_anim, &s_test_eaf_dsc);
    lv_eaf_set_frame_delay(timer_ui.eaf_anim, 30);
    lv_eaf_pause(timer_ui.eaf_anim);

    int32_t total_frames = lv_eaf_get_total_frames(timer_ui.eaf_anim);
    ESP_LOGI(TAG, "Total frames: %d", total_frames);
#endif

    // Calculate arc size and position (centered)
    lv_coord_t arc_size = LV_MIN(SCREEN_WIDTH, SCREEN_HEIGHT) - 20;

    // Create background arc (outer white border ring - full circle)
    timer_ui.bg_arc = lv_arc_create(timer_ui.container);
    // Slightly larger than the main arc so the border is just outside it
    lv_obj_set_size(timer_ui.bg_arc, arc_size + 20, arc_size + 20);
    lv_obj_center(timer_ui.bg_arc);
    lv_arc_set_rotation(timer_ui.bg_arc, 270);  // Rotate so 0° is at top (12 o'clock)
    lv_arc_set_bg_angles(timer_ui.bg_arc, 0, 360);  // Full circle background
    lv_obj_remove_style(timer_ui.bg_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(timer_ui.bg_arc, LV_OBJ_FLAG_CLICKABLE);
    // Thicker white ring as outer border
    lv_obj_set_style_arc_width(timer_ui.bg_arc, 50, LV_PART_MAIN);
    lv_obj_set_style_arc_color(timer_ui.bg_arc, lv_color_make(0xc6, 0xc6, 0xc6), LV_PART_MAIN);
    lv_obj_set_style_arc_width(timer_ui.bg_arc, 0, LV_PART_INDICATOR);  // No indicator
    lv_arc_set_value(timer_ui.bg_arc, 0);

    // Create progress arc (colored/textured part from start to end)
    timer_ui.progress_arc = lv_arc_create(timer_ui.container);
    lv_obj_set_size(timer_ui.progress_arc, arc_size, arc_size);
    lv_obj_center(timer_ui.progress_arc);
    lv_arc_set_rotation(timer_ui.progress_arc, 270);  // Rotate so 0° is at top (12 o'clock)

    // Set mode to NORMAL (indicator goes from start angle to value angle)
    lv_arc_set_mode(timer_ui.progress_arc, LV_ARC_MODE_NORMAL);

    // Set background angles (not shown, but defines the valid range)
    lv_arc_set_bg_angles(timer_ui.progress_arc, 0, 360);

    // Set the value range (0-360 degrees)
    lv_arc_set_range(timer_ui.progress_arc, 0, 360);

    // Calculate initial angle based on initial time
    int32_t initial_angle = (360 * TIMER_INITIAL_SECONDS) / TIMER_MAX_SECONDS;

    // Set start and end angles for the indicator
    // Use (initial_angle, 0) so the covered area goes clockwise from 12 o'clock
    // (0° after rotation) to the current end angle.
    // lv_arc_set_angles(timer_ui.progress_arc, initial_angle, 0);
    lv_arc_set_angles(timer_ui.progress_arc, 0, initial_angle);

    // ESP_LOGI(TAG, "Initial angle set to: %ld° (from 0° to %ld°) for %ld seconds",
    //          initial_angle, initial_angle, TIMER_INITIAL_SECONDS);

    // Set image texture for the arc indicator: use time_bg as the textured arc
    lv_obj_set_style_arc_image_src(timer_ui.progress_arc, &time_arc_texture, LV_PART_INDICATOR);

    lv_obj_remove_style(timer_ui.progress_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(timer_ui.progress_arc, LV_OBJ_FLAG_CLICKABLE);

    // Hide the background part (we only want the indicator to show)
    lv_obj_set_style_arc_width(timer_ui.progress_arc, 0, LV_PART_MAIN);

    // Style the indicator (progress bar from 0° to current angle)
    lv_obj_set_style_arc_width(timer_ui.progress_arc, 30, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(timer_ui.progress_arc, false, LV_PART_INDICATOR);

    // Use white color so the texture shows its original colors
    lv_obj_set_style_arc_color(timer_ui.progress_arc, lv_color_make(0xc6, 0xc6, 0xc6), LV_PART_INDICATOR);

    // Create time labels in center: "MM:SS" as three labels using a 3-column grid.
    // Column layout:
    //   [ col 0: minutes ][ col 1: ":" ][ col 2: seconds ]
    // Middle column is always centered, so ":" stays fixed.
    timer_ui.time_container = lv_obj_create(timer_ui.container);
    lv_obj_set_size(timer_ui.time_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(timer_ui.time_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(timer_ui.time_container, 0, 0);
    lv_obj_set_style_pad_all(timer_ui.time_container, 0, 0);
    lv_obj_align(timer_ui.time_container, LV_ALIGN_TOP_MID, 0, 87);
    lv_obj_set_size(timer_ui.time_container, 160, 70);
    // Grid: 3 columns (FR, CONTENT, FR) and 1 row
    static const int32_t col_dsc[] = {
        LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
    };
    static const int32_t row_dsc[] = {
        LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST
    };
    lv_obj_set_grid_dsc_array(timer_ui.time_container, col_dsc, row_dsc);

    // Minutes label in column 0
    timer_ui.time_min_label = lv_label_create(timer_ui.time_container);
    int init_minutes = timer_ui.remaining_seconds / 60;
    int init_seconds = timer_ui.remaining_seconds % 60;
    lv_label_set_text_fmt(timer_ui.time_min_label, "%02d", init_minutes);
    lv_obj_set_grid_cell(timer_ui.time_min_label,
                         LV_GRID_ALIGN_END, 0, 1,
                         LV_GRID_ALIGN_CENTER, 0, 1);

    // Colon label in column 1 (center)
    timer_ui.time_colon_label = lv_label_create(timer_ui.time_container);
    lv_label_set_text(timer_ui.time_colon_label, ":");
    lv_obj_set_grid_cell(timer_ui.time_colon_label,
                         LV_GRID_ALIGN_CENTER, 1, 1,
                         LV_GRID_ALIGN_CENTER, 0, 1);

    // Seconds label in column 2
    timer_ui.time_sec_label = lv_label_create(timer_ui.time_container);
    lv_label_set_text_fmt(timer_ui.time_sec_label, "%02d", init_seconds);
    lv_obj_set_grid_cell(timer_ui.time_sec_label,
                         LV_GRID_ALIGN_START, 2, 1,
                         LV_GRID_ALIGN_CENTER, 0, 1);


    lv_obj_set_style_text_font(timer_ui.time_min_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_font(timer_ui.time_colon_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_font(timer_ui.time_sec_label, &lv_font_montserrat_40, 0);

    lv_obj_set_style_text_color(timer_ui.time_min_label, lv_color_white(), 0);
    lv_obj_set_style_text_color(timer_ui.time_colon_label, lv_color_white(), 0);
    lv_obj_set_style_text_color(timer_ui.time_sec_label, lv_color_white(), 0);

    // Create knob objects with textures
    // Start knob (at the beginning of the arc) -> time_start
    timer_ui.start_knob_img = lv_img_create(timer_ui.container);
    lv_img_set_src(timer_ui.start_knob_img, &time_start);
    lv_obj_clear_flag(timer_ui.start_knob_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(timer_ui.start_knob_img, LV_OBJ_FLAG_CLICKABLE);  // Not draggable
    // Add subtle visual feedback on touch (optional)
    lv_obj_add_event_cb(timer_ui.start_knob_img, start_knob_event_handler, LV_EVENT_ALL, &timer_ui);

    // End knob (at the current position of the arc) - This is the main draggable knob -> time_end
    timer_ui.end_knob_img = lv_img_create(timer_ui.container);
    lv_img_set_src(timer_ui.end_knob_img, &time_end);
    lv_obj_clear_flag(timer_ui.end_knob_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(timer_ui.end_knob_img, LV_OBJ_FLAG_CLICKABLE);  // Make it clickable
    // Add event handler for dragging
    lv_obj_add_event_cb(timer_ui.end_knob_img, end_knob_event_handler, LV_EVENT_ALL, &timer_ui);

    // Initial knob positions:
    // 先强制更新一次布局，保证 arc 的坐标/尺寸是最终值，
    // 避免第一次计算时拿到的 x/y 都是 0 导致 knob 跑到左上角。
    lv_obj_update_layout(timer_ui.container);
    // use current remaining_seconds to compute angle so that
    // - when INITIAL_SECONDS == 0: start_point == end_point at top, countdown == 0
    // - otherwise: end knob is placed according to initial countdown
    int32_t init_angle = (360 * timer_ui.remaining_seconds) / TIMER_MAX_SECONDS;
    update_time_from_angle(&timer_ui, init_angle);

    // Create timer to update countdown
    timer_ui.timer = lv_timer_create(timer_tick_cb, TIMER_UPDATE_PERIOD_MS, &timer_ui);

    // ESP_LOGI(TAG, "Timer UI created successfully");
}

// Public function for backward compatibility
void create_timer_ui(void)
{
    create_timer_ui_internal(lv_scr_act());
}

// Public function with parent parameter
void create_timer_ui_with_parent(lv_obj_t *parent)
{
    create_timer_ui_internal(parent);
}

// Cleanup function (optional)
void destroy_timer_ui(void)
{
    if (timer_ui.timer) {
        lv_timer_del(timer_ui.timer);
        timer_ui.timer = NULL;
    }
    if (timer_ui.container) {
        lv_obj_del(timer_ui.container);
        timer_ui.container = NULL;
    }
}

// Adjust end point by delta minutes (positive = increase, negative = decrease)
void timer_ui_adjust_end_point(int32_t delta_minutes)
{
    esp_lv_adapter_lock(-1);
    
    // Record old remaining seconds to detect transition from 0 to non-zero
    int32_t old_remaining_seconds = timer_ui.remaining_seconds;
    
    // Convert minutes to seconds
    int32_t delta_seconds = delta_minutes * 60;
    
    // Calculate new remaining seconds
    int32_t new_remaining_seconds = timer_ui.remaining_seconds + delta_seconds;
    
    // Limit to valid range (0 to TIMER_MAX_SECONDS)
    if (new_remaining_seconds < 0) {
        new_remaining_seconds = 0;
    } else if (new_remaining_seconds > TIMER_MAX_SECONDS) {
        new_remaining_seconds = TIMER_MAX_SECONDS;
    }
    
    // Calculate angle from new remaining seconds
    int32_t new_angle = (360 * new_remaining_seconds) / TIMER_MAX_SECONDS;
    
    // Update time and UI
    update_time_from_angle(&timer_ui, new_angle);
    
    // Auto-start timer if time changed from 0 to non-zero
    if (old_remaining_seconds == 0 && new_remaining_seconds > 0) {
        if (timer_ui.timer) {
            bool is_paused = lv_timer_get_paused(timer_ui.timer);
            if (is_paused) {
                lv_timer_resume(timer_ui.timer);
                timer_set_state(&timer_ui, TIMER_STATE_RUNNING);
                ESP_LOGI(TAG, "Timer auto-started (time changed from 0 to %ld seconds)", new_remaining_seconds);
            }
        }
    }

    esp_lv_adapter_unlock();
    
    ESP_LOGI(TAG, "Adjusted end point by %ld minutes, new remaining: %ld seconds", 
             delta_minutes, timer_ui.remaining_seconds);
}

// Toggle start/pause timer
void timer_ui_toggle_start_pause(void)
{
    if (!timer_ui.timer) {
        ESP_LOGW(TAG, "Timer not initialized");
        return;
    }
    
    esp_lv_adapter_lock(-1);
    // Check if timer is paused
    bool is_paused = lv_timer_get_paused(timer_ui.timer);
    
    if (is_paused) {
        // Resume timer if there's remaining time
        if (timer_ui.remaining_seconds > 0) {
            lv_timer_resume(timer_ui.timer);
            timer_set_state(&timer_ui, TIMER_STATE_RUNNING);
            ESP_LOGI(TAG, "Timer resumed");
        } else {
            ESP_LOGI(TAG, "Timer cannot resume: no remaining time");
        }
    } else {
        // Pause timer
        timer_set_state(&timer_ui, TIMER_STATE_PAUSED);
        lv_timer_pause(timer_ui.timer);
        ESP_LOGI(TAG, "Timer paused");
    }
    esp_lv_adapter_unlock();
}

// Get current timer mode
bool timer_ui_is_countdown_mode(void)
{
    return (current_timer_mode == TIMER_MODE_COUNTDOWN);
}

// Reset timer to 0:00
void timer_ui_reset_to_zero(void)
{
    esp_lv_adapter_lock(-1);
    
    // Stop timer if running
    if (timer_ui.timer) {
        lv_timer_pause(timer_ui.timer);
    }
    
    // Reset remaining seconds to 0
    timer_ui.remaining_seconds = 0;
    
    // Clear flag to prevent jump when manually reset to 0
    timer_ui.was_running_before_zero = false;
    
    // Update UI to show 0:00
    update_time_from_angle(&timer_ui, 0);
    
    // Set state to END
    timer_set_state(&timer_ui, TIMER_STATE_END);
    
    esp_lv_adapter_unlock();
    
    ESP_LOGI(TAG, "Timer reset to 0:00");
}

// Switch to countdown mode
void timer_ui_switch_to_countdown(void)
{
    if (current_timer_mode == TIMER_MODE_COUNTDOWN) {
        return;  // Already in countdown mode
    }
    
    esp_lv_adapter_lock(-1);
    current_timer_mode = TIMER_MODE_COUNTDOWN;
    lv_obj_t *range_container = main_ui_get_range_container();
    lv_obj_t *timer_container = main_ui_get_timer_container();
    if (range_container) {
        lv_obj_add_flag(range_container, LV_OBJ_FLAG_HIDDEN);
    }
    if (timer_container) {
        lv_obj_clear_flag(timer_container, LV_OBJ_FLAG_HIDDEN);
    }
    esp_lv_adapter_unlock();
    
    // Reset timer to 0:00 when entering countdown mode (this function has its own lock)
    timer_ui_reset_to_zero();
    
    ESP_LOGI(TAG, "Switched to Countdown mode, timer reset to 0:00");
}

// Switch to range mode
void timer_ui_switch_to_range(void)
{
    if (current_timer_mode == TIMER_MODE_RANGE) {
        return;  // Already in range mode
    }
    
    esp_lv_adapter_lock(-1);
    current_timer_mode = TIMER_MODE_RANGE;
    lv_obj_t *timer_container = main_ui_get_timer_container();
    lv_obj_t *range_container = main_ui_get_range_container();
    if (timer_container) {
        lv_obj_add_flag(timer_container, LV_OBJ_FLAG_HIDDEN);
    }
    if (range_container) {
        lv_obj_clear_flag(range_container, LV_OBJ_FLAG_HIDDEN);
    }
    esp_lv_adapter_unlock();
    ESP_LOGI(TAG, "Switched to Range mode");
}

// Show timer UI
void timer_ui_show(void)
{
    esp_lv_adapter_lock(-1);
    if (timer_ui.container) {
        lv_obj_clear_flag(timer_ui.container, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_t *timer_container = main_ui_get_timer_container();
    if (timer_container) {
        lv_obj_clear_flag(timer_container, LV_OBJ_FLAG_HIDDEN);
    }
    esp_lv_adapter_unlock();
    ESP_LOGI(TAG, "Timer UI shown");
}

// Hide timer UI
void timer_ui_hide(void)
{
    esp_lv_adapter_lock(-1);
    if (timer_ui.container) {
        lv_obj_add_flag(timer_ui.container, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_t *timer_container = main_ui_get_timer_container();
    if (timer_container) {
        lv_obj_add_flag(timer_container, LV_OBJ_FLAG_HIDDEN);
    }
    esp_lv_adapter_unlock();
    ESP_LOGI(TAG, "Timer UI hidden");
}
