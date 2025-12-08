#ifndef CST816S_TOUCH_H
#define CST816S_TOUCH_H

#include "i2c_device.h"
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class Cst816s : public I2cDevice {
public:
    struct TouchPoint_t {
        int num = 0;
        int x = -1;
        int y = -1;
    };

    enum TouchEvent {
        TOUCH_NONE,
        TOUCH_PRESS,
        TOUCH_RELEASE,
        TOUCH_HOLD
    };

    Cst816s(i2c_master_bus_handle_t i2c_bus, uint8_t addr);
    ~Cst816s();

    void UpdateTouchPoint();
    const TouchPoint_t &GetTouchPoint();
    TouchEvent CheckTouchEvent();
    int GetPressCount() const;
    void ResetPressCount();
    SemaphoreHandle_t GetTouchSemaphore();
    bool WaitForTouchEvent(TickType_t timeout = portMAX_DELAY);
    void NotifyTouchEvent();

private:
    uint8_t* read_buffer_ = nullptr;
    TouchPoint_t tp_;
    bool was_touched_;
    int press_count_;
    SemaphoreHandle_t touch_isr_mux_;
};

#endif // CST816S_TOUCH_H
