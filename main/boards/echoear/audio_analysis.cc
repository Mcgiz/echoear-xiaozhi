#include "audio_analysis.h"
#include "audio_doa.h"
#include "application.h"
#include "board.h"
#include "audio_codec.h"
#include "display/emote_display.h"
#include "device_state.h"
#include <esp_log.h>
#include <cstring>
#include "echo_base_control.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <wifi_station.h>

#define TAG "AudioAnalysis"

AudioAnalysis::AudioAnalysis() : beat_detection_handle_(nullptr)
{
    memset(&doa_app_, 0, sizeof(doa_app_));
    // UDP socket will be initialized lazily when WiFi is connected
}

AudioAnalysis::~AudioAnalysis()
{
    // Handles will be cleaned up by their respective components
}

void AudioAnalysis::Initialize()
{
    // AudioCodec* codec = Board::GetInstance().GetAudioCodec();
    // int channel_count = codec != nullptr ? codec->input_channels() : 1;
    // ESP_LOGI(TAG, "Initializing audio analysis with %d channels", channel_count);

    int channel_count = 2;

    // Initialize beat detection
    beat_detection_cfg_t beat_cfg = {
        .sample_rate = BEAT_DETECTION_DEFAULT_SAMPLE_RATE,
        .channel = static_cast<uint8_t>(channel_count),
        .fft_size = BEAT_DETECTION_DEFAULT_FFT_SIZE,
        .bass_freq_start = static_cast<uint16_t>(BEAT_DETECTION_DEFAULT_BASS_FREQ_MIN),
        .bass_freq_end = static_cast<uint16_t>(BEAT_DETECTION_DEFAULT_BASS_FREQ_MAX),
        .bass_surge_threshold = BEAT_DETECTION_DEFAULT_BASS_SURGE_THRESHOLD,
        .task_priority = BEAT_DETECTION_DEFAULT_TASK_PRIORITY,
        .task_stack_size = BEAT_DETECTION_DEFAULT_TASK_STACK_SIZE,
        .task_core_id = BEAT_DETECTION_DEFAULT_TASK_CORE_ID,
        .result_callback = reinterpret_cast<void*>(BeatDetectionResultCallback),
        .result_callback_ctx = this,
        .flags = {
            .enable_psram = false,
        }
    };

    esp_err_t ret = beat_detection_init(&beat_cfg, &beat_detection_handle_);
    if (ret != ESP_OK || beat_detection_handle_ == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize beat detection: %d", ret);
    } else {
        ESP_LOGI(TAG, "Beat detection initialized successfully (channels: %d)", channel_count);
    }

    // Initialize audio DOA
    audio_doa_app_config_t doa_app_cfg = {
        .doa_tracker_result_callback = DoaTrackerResultCallback,
        .doa_tracker_result_callback_ctx = NULL,
    };
    ret = audio_doa_app_create(&doa_app_, &doa_app_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create audio DOA app");
    } else {
        ESP_LOGI(TAG, "Audio DOA app created successfully");
    }
}

void AudioAnalysis::BeatDetectionResultCallback(beat_detection_result_t result, void *ctx)
{
    AudioAnalysis* self = static_cast<AudioAnalysis*>(ctx);
    if (self == nullptr) {
        return;
    }

    if (self->mode_ == AudioAnalysisMode::BEAT_DETECTION && result == BEAT_DETECTED) {
        ESP_LOGI(TAG, "Beat detected (result=%d), triggering beat swing action", result);
        echo_base_control_set_action(ECHO_BASE_CMD_SET_ACTION_BEAT_SWING);

        Display* display = Board::GetInstance().GetDisplay();
        if (display != nullptr) {
            emote::EmoteDisplay* emote_display = dynamic_cast<emote::EmoteDisplay*>(display);
            if (emote_display != nullptr) {
                emote_display->InsertAnimDialog("beat_swing", 8 * 1000); // 8 seconds animation
            }
        }
    }
}

void AudioAnalysis::DoaTrackerResultCallback(float angle, void *ctx)
{
    audio_doa_app_t *app = (audio_doa_app_t *)ctx;
    if (app == NULL) {
        ESP_LOGE(TAG, "audio_doa_tracker_result_callback: app is NULL");
        return;
    }
    if (app->last_callback_time == 0) {
        app->last_callback_time = xTaskGetTickCount();
    } else {
        if (xTaskGetTickCount() - app->last_callback_time > pdMS_TO_TICKS(500)) {
            ESP_LOGI(TAG, "Estimated direction: %.2f", angle);
            echo_base_control_set_angle(angle);
            app->last_callback_time = xTaskGetTickCount();
        }
    }
    // ESP_LOGI(TAG, "Estimated direction: %.2f", angle);
}

void AudioAnalysis::SetAfeDataProcessCallback()
{
    auto &app = Application::GetInstance();
    app.GetAudioService().SetAfeDataProcessedCallback([this](const int16_t* audio_data, size_t total_bytes) {
        OnAfeDataProcessed(audio_data, total_bytes);
    });
}

void AudioAnalysis::SetVadStateChangeCallback()
{
    auto &app = Application::GetInstance();
    app.GetAudioService().SetVadStateChangeCallback([this](bool speaking) {
        OnVadStateChange(speaking);
    });
}

void AudioAnalysis::SetAudioDataProcessedCallback()
{
    auto &app = Application::GetInstance();
    app.GetAudioService().SetAudioDataProcessedCallback([this](const int16_t* audio_data, size_t bytes_per_channel, size_t channels) {
        OnAudioDataProcessed(audio_data, bytes_per_channel, channels);
    });
}

void AudioAnalysis::OnAfeDataProcessed(const int16_t* audio_data, size_t total_bytes)
{
}

void AudioAnalysis::OnVadStateChange(bool speaking)
{
    if (speaking) {
        ESP_LOGD(TAG, "active");
        audio_doa_app_set_vad_detect(&doa_app_, true);
    } else {
        ESP_LOGD(TAG, "silence");
        audio_doa_app_set_vad_detect(&doa_app_, false);
    }
}

void AudioAnalysis::SetMode(AudioAnalysisMode mode)
{
    mode_ = mode;

    // Stop any existing animation dialog when mode changes
    Display* display = Board::GetInstance().GetDisplay();
    if (display != nullptr) {
        emote::EmoteDisplay* emote_display = dynamic_cast<emote::EmoteDisplay*>(display);
        if (emote_display != nullptr) {
            emote_display->StopAnimDialog();
        }
    }
    ESP_LOGI(TAG, "Audio analysis mode set to: %d", static_cast<int>(mode));
}

void AudioAnalysis::OnAudioDataProcessed(const int16_t* audio_data, size_t bytes_per_channel, size_t channels)
{
    //Incoming: bytes=1024, channels=2
    // ESP_LOGI(TAG, "Incoming: bytes=%d, channels=%d", bytes_per_channel, channels);

    switch (mode_) {
    case AudioAnalysisMode::BEAT_DETECTION:
        // Feed to beat detection
        if (beat_detection_handle_ != nullptr && beat_detection_handle_->feed_audio != nullptr) {
            beat_detection_audio_buffer_t buffer = {
                .audio_buffer = (uint8_t*)audio_data,
                .bytes_size = bytes_per_channel * channels
            };
            beat_detection_handle_->feed_audio(buffer, beat_detection_handle_->feed_audio_ctx);
        }
        break;

    case AudioAnalysisMode::DOA_FOLLOW: {
        auto &app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateSpeaking) {
            break;
        }
        // Feed to audio DOA
        if (doa_app_.doa_handle != nullptr) {
            audio_doa_app_data_write(&doa_app_, (uint8_t *)audio_data, bytes_per_channel * channels);
        }
        break;
    }

    case AudioAnalysisMode::DISABLED:
    default:
        break;
    }
}
