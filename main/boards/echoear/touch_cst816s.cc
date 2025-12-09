#include "touch_cst816s.h"
#include "config.h"
#include <esp_log.h>
#include <esp_lcd_touch_cst816s.h>
#include <esp_lcd_panel_io.h>

#define TAG "Cst816s"

Cst816s::Cst816s(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr)
{
    read_buffer_ = new uint8_t[6];
    was_touched_ = false;
    press_count_ = 0;

    // Create touch interrupt semaphore
    touch_isr_mux_ = xSemaphoreCreateBinary();
    if (touch_isr_mux_ == NULL) {
        ESP_LOGE(TAG, "Failed to create touch semaphore");
    }
}

Cst816s::~Cst816s()
{
    delete[] read_buffer_;

    // Delete semaphore if it exists
    if (touch_isr_mux_ != NULL) {
        vSemaphoreDelete(touch_isr_mux_);
        touch_isr_mux_ = NULL;
    }
}

void Cst816s::UpdateTouchPoint()
{
    ReadRegs(0x02, read_buffer_, 6);
    tp_.num = read_buffer_[0] & 0x0F;
    tp_.x = ((read_buffer_[1] & 0x0F) << 8) | read_buffer_[2];
    tp_.y = ((read_buffer_[3] & 0x0F) << 8) | read_buffer_[4];
}

const Cst816s::TouchPoint_t &Cst816s::GetTouchPoint()
{
    return tp_;
}

Cst816s::TouchEvent Cst816s::CheckTouchEvent()
{
    bool is_touched = (tp_.num > 0);
    TouchEvent event = TOUCH_NONE;

    if (is_touched && !was_touched_) {
        // Press event (transition from not touched to touched)
        press_count_++;
        event = TOUCH_PRESS;
        ESP_LOGI(TAG, "PRESS");
        ESP_LOGD(TAG, "count: %d, x: %d, y: %d", press_count_, tp_.x, tp_.y);
    } else if (!is_touched && was_touched_) {
        // Release event (transition from touched to not touched)
        event = TOUCH_RELEASE;
        ESP_LOGI(TAG, "RELEASE");
    } else if (is_touched && was_touched_) {
        // Continuous touch (hold)
        event = TOUCH_HOLD;
        ESP_LOGD(TAG, "HOLD - x: %d, y: %d", tp_.x, tp_.y);
    }

    // Update previous state
    was_touched_ = is_touched;
    return event;
}

int Cst816s::GetPressCount() const
{
    return press_count_;
}

void Cst816s::ResetPressCount()
{
    press_count_ = 0;
}

SemaphoreHandle_t Cst816s::GetTouchSemaphore()
{
    return touch_isr_mux_;
}

bool Cst816s::WaitForTouchEvent(TickType_t timeout)
{
    if (touch_isr_mux_ != NULL) {
        return xSemaphoreTake(touch_isr_mux_, timeout) == pdTRUE;
    }
    return false;
}

void Cst816s::NotifyTouchEvent()
{
    if (touch_isr_mux_ != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(touch_isr_mux_, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

esp_err_t cst816s_touch_init(i2c_master_bus_handle_t i2c_bus, 
                              int width, 
                              int height,
                              bool swap_xy,
                              bool mirror_x,
                              bool mirror_y,
                              esp_lcd_touch_handle_t *ret_touch)
{
    ESP_LOGI(TAG, "Initializing CST816S touch controller");

    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "I2C bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (ret_touch == NULL) {
        ESP_LOGE(TAG, "Touch handle pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Configure touch panel parameters
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = static_cast<uint16_t>(width),
        .y_max = static_cast<uint16_t>(height),
        .rst_gpio_num = TP_PIN_NUM_RST,
        .int_gpio_num = TP_PIN_NUM_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
    };

    // Create panel IO handle for touch controller
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    tp_io_config.scl_speed_hz = 400000;  // 400kHz I2C speed

    ESP_LOGI(TAG, "clk:%d", tp_io_config.scl_speed_hz);
    ESP_LOGI(TAG, "swap_xy:%d", swap_xy);
    ESP_LOGI(TAG, "mirror_x:%d", mirror_x);
    ESP_LOGI(TAG, "mirror_y:%d", mirror_y);
    ESP_LOGI(TAG, "x_max:%d", tp_cfg.x_max);
    ESP_LOGI(TAG, "y_max:%d", tp_cfg.y_max);
    ESP_LOGI(TAG, "rst_gpio_num:%d", tp_cfg.rst_gpio_num);
    ESP_LOGI(TAG, "int_gpio_num:%d", tp_cfg.int_gpio_num);
    ESP_LOGI(TAG, "levels.reset:%d", tp_cfg.levels.reset);
    ESP_LOGI(TAG, "levels.interrupt:%d", tp_cfg.levels.interrupt);
    ESP_LOGI(TAG, "flags.swap_xy:%d", tp_cfg.flags.swap_xy);
    ESP_LOGI(TAG, "flags.mirror_x:%d", tp_cfg.flags.mirror_x);
    ESP_LOGI(TAG, "flags.mirror_y:%d", tp_cfg.flags.mirror_y);

    esp_err_t ret = esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO for touch controller: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize CST816S touch controller
    ret = esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, ret_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create CST816S touch driver: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "CST816S touch controller initialized successfully");
    return ESP_OK;
}
