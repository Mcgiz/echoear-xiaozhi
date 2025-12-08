#include "touch_cst816s.h"
#include <esp_log.h>

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
