#include "base_control.h"
#include "EchoEar.h"
#include "echo_base_control.h"
#include "display/display.h"
#include "display/emote_display.h"
#include "config.h"
#include "application.h"
#include "device_state.h"
#include <esp_log.h>
#include <esp_timer.h>

#define TAG "BaseControl"

extern gpio_num_t UART1_TX;
extern gpio_num_t UART1_RX;

BaseControl::BaseControl(EspS3Cat* board) : board_(board)
{
    echo_base_online_ = false;
    last_heartbeat_time_ = 0;
    heartbeat_check_timer_ = nullptr;
}

BaseControl::~BaseControl()
{
    if (heartbeat_check_timer_ != nullptr) {
        esp_timer_stop(heartbeat_check_timer_);
        esp_timer_delete(heartbeat_check_timer_);
    }
}

void BaseControl::Initialize()
{
    echo_base_control_config_t base_config = {
        .uart_num = UART_NUM_1,
        .tx_pin = UART1_TX,
        .rx_pin = UART1_RX,
        .baud_rate = 115200,
        .rx_buffer_size = 1024,
        .cmd_cb = CmdCallback,
        .user_ctx = this,
    };
    esp_err_t ret = echo_base_control_init(&base_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize echo base control");
        return;
    }

    // Initialize heartbeat monitoring timer
    const esp_timer_create_args_t heartbeat_timer_args = {
        .callback = &HeartbeatCheckTimerCallback,
        .arg = this,
        .name = "heartbeat_check"
    };
    ret = esp_timer_create(&heartbeat_timer_args, &heartbeat_check_timer_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create heartbeat check timer");
        return;
    }

    // Start periodic timer to check heartbeat every 500ms
    ret = esp_timer_start_periodic(heartbeat_check_timer_, 500 * 1000);  // 500ms in microseconds
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start heartbeat check timer");
        esp_timer_delete(heartbeat_check_timer_);
        heartbeat_check_timer_ = nullptr;
        return;
    }

    // Initialize to offline state
    echo_base_online_ = false;
    last_heartbeat_time_ = 0;
}

void BaseControl::HandleCommand(uint8_t cmd, uint8_t *data, int data_len)
{
    Display* display = board_->GetDisplay();
    if (display == nullptr) {
        return;
    }

    switch (cmd) {
    case ECHO_BASE_CMD_RECV_SLIDE_SWITCH: {
        if (data_len >= 2) {
            auto &app = Application::GetInstance();

            uint16_t event = (data[0] << 8) | data[1];
            switch (event) {
            case ECHO_BASE_CMD_RECV_SWITCH_SLIDE_DOWN:
                // display->SetStatus("status_IDLE");
                // display->SetEmotion("happy");
                // app.SetDeviceState(kDeviceStateListening);
                ESP_LOGI(TAG, "Slide switch down");
                break;
            case ECHO_BASE_CMD_RECV_SWITCH_SLIDE_UP:
                // display->SetStatus("status_IDLE");
                // display->SetEmotion("sleepy");
                // app.SetDeviceState(kDeviceStateIdle);
                ESP_LOGI(TAG, "Slide switch up");
                break;
            default:
                ESP_LOGI(TAG, "Slide switch event: %d", event);
                break;
            }
        }
        break;
    }
    case ECHO_BASE_CMD_RECV_PERCEPTION: {
        ESP_LOGI(TAG, "Perception mode response, data_len=%d", data_len);
        break;
    }
    case ECHO_BASE_CMD_RECV_ACTION: {
        ESP_LOGI(TAG, "Action response, data_len=%d", data_len);
        uint16_t event = (data[0] << 8) | data[1];
        switch (event) {
        case ECHO_BASE_CMD_RECV_ACTION_DONE:
            ESP_LOGI(TAG, "Action done");
            break;
        default:
            ESP_LOGI(TAG, "Unknown action event: %d", event);
            break;
        }
        break;
    }
    case ECHO_BASE_CMD_RECV_HEARTBEAT: {
        uint16_t event = (data[0] << 8) | data[1];
        // ESP_LOGI(TAG, "Heartbeat event: %d", event);
        switch (event) {
        case ECHO_BASE_CMD_RECV_HEARTBEAT_ALIVE: {
            int64_t current_time = esp_timer_get_time() / 1000;  // Convert to milliseconds
            bool was_offline = !echo_base_online_;

            last_heartbeat_time_ = current_time;
            echo_base_online_ = true;

            if (was_offline) {
                ESP_LOGI(TAG, "Echo base connected (reinserted)");

                emote::EmoteDisplay* emote_display = dynamic_cast<emote::EmoteDisplay*>(display);
                if (emote_display != nullptr) {
                    emote_display->InsertAnimDialog("insert", 3000);
                }
            }
            break;
        }
        }
        break;
    }
    default: {
        ESP_LOGI(TAG, "Unknown command: 0x%02X", cmd);
        break;
    }
    }
}

void BaseControl::HeartbeatCheckTimerCallback(void* arg)
{
    BaseControl* self = static_cast<BaseControl*>(arg);
    if (self == nullptr) {
        return;
    }

    // Skip check if we haven't received any heartbeat yet
    if (self->last_heartbeat_time_ == 0) {
        return;
    }

    int64_t current_time = esp_timer_get_time() / 1000;  // Convert to milliseconds
    int64_t time_since_last_heartbeat = current_time - self->last_heartbeat_time_;

    // Check if heartbeat timeout (2 seconds = 4 missed heartbeats at 500ms interval)
    if (self->echo_base_online_ && time_since_last_heartbeat > BaseControl::HEARTBEAT_TIMEOUT_MS) {
        self->echo_base_online_ = false;
        ESP_LOGW(TAG, "Echo base disconnected (timeout: %lld ms)", time_since_last_heartbeat);
    }
}

void BaseControl::CmdCallback(uint8_t cmd, uint8_t *data, int data_len, void *user_ctx)
{
    BaseControl* self = static_cast<BaseControl*>(user_ctx);
    if (self != nullptr) {
        self->HandleCommand(cmd, data, data_len);
    }
}
