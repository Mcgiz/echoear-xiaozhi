#ifndef ECHOEAR_H
#define ECHOEAR_H

#include "wifi_board.h"
#include "display/display.h"
#include "backlight.h"
#include "button.h"
#include "cst816s_touch.h"
#include "charge.h"
#include "base_control.h"
#include "audio_analysis.h"
#include <functional>

class EspS3Cat : public WifiBoard {
public:
    EspS3Cat();

    virtual AudioCodec* GetAudioCodec() override;
    virtual Display* GetDisplay() override;
    virtual Backlight* GetBacklight() override;
    Cst816s* GetTouchpad();

    virtual void SetAfeDataProcessCallback(std::function<void(const int16_t* audio_data, size_t total_bytes)> callback) override;
    virtual void SetVadStateChangeCallback(std::function<void(bool speaking)> callback) override;
    virtual void SetAudioDataProcessedCallback(std::function<void(const int16_t* audio_data, size_t bytes_per_channel, size_t channels)> callback) override;

    beat_detection_handle_t GetBeatDetectionHandle() const;
    void SetAudioAnalysisMode(AudioAnalysisMode mode);

private:
    i2c_master_bus_handle_t i2c_bus_;
    Cst816s* cst816s_;
    Charge* charge_;
    Button boot_button_;
    Display* display_;
    PwmBacklight* backlight_;

    BaseControl* base_control_;
    AudioAnalysis* audio_analysis_;

    void InitializeI2c();
    uint8_t DetectPcbVersion();
    void InitializeSpi();
    void Initializest77916Display(uint8_t pcb_version);
    void InitializeButtons();
    void InitializeCharge();
    void InitializeCst816sTouchPad();

    static void touch_isr_callback(void* arg);
    static void touch_event_task(void* arg);
};

#endif // ECHOEAR_H
