#include "customer_ui.h"
#include "main_ui.h"
#include "../config.h"
#include <esp_log.h>
#include <lvgl.h>

#define TAG "CustomerUI"

void create_customer_ui(Display *display)
{
    create_main_ui(display);
    ESP_LOGI(TAG, "Customer UI created for %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);
}
