#include "echoear_tools.h"
#include "EchoEar.h"
#include "echo_base_control.h"
#include "audio_analysis.h"
#include "mcp_server.h"
#include "board.h"
#include "assets/lang_config.h"
#include <esp_log.h>
#include "customer_ui/alarm_api.h"

#define TAG "EchoEarTools"

void EchoEarTools::Initialize(EspS3Cat* board)
{
    auto &mcp_server = McpServer::GetInstance();

    // Echo base action control
    mcp_server.AddTool("self.echo_base.set_action", "Echo base action control. Available actions:\n"
                       "shark_head: 摇头动作\n"
                       "shark_head_decay: 缓慢摇头动作\n"
                       "look_around: 环顾四周动作\n"
                       "beat_swing: 节拍摇摆动作\n"
                       "cat_nuzzle: 蹭头撒娇动作\n"
                       "calibrate: 校准底座\n",
    PropertyList({
        Property("action", kPropertyTypeString),
    }), [board](const PropertyList & properties) -> ReturnValue {
        const std::string &action = properties["action"].value<std::string>();
        int action_value = -1;

        ESP_LOGI(TAG, "&&& Do Action: %s", action.c_str());
        if (action == "shark_head") {
            action_value = ECHO_BASE_CMD_SET_ACTION_SHARK_HEAD;
        } else if (action == "shark_head_decay") {
            action_value = ECHO_BASE_CMD_SET_ACTION_SHARK_HEAD_DECAY;
        } else if (action == "look_around")
        {
            action_value = ECHO_BASE_CMD_SET_ACTION_LOOK_AROUND;
        } else if (action == "beat_swing")
        {
            action_value = ECHO_BASE_CMD_SET_ACTION_BEAT_SWING;
        } else if (action == "cat_nuzzle")
        {
            action_value = ECHO_BASE_CMD_SET_ACTION_CAT_NUZZLE;
        } else if (action == "calibrate")
        {
            echo_base_control_set_calibrate();
            BaseControl* base_control = board->GetBaseControl();
            if (base_control != nullptr) {
                bool completed = base_control->WaitForCalibrationComplete(30000);
                if (!completed) {
                    ESP_LOGW(TAG, "Calibration wait timeout");
                    return false;
                }
            }
        } else
        {
            return false;
        }

        if (action_value != -1)
        {
            esp_err_t ret = echo_base_control_set_action(action_value);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set action: %d", ret);
                return false;
            }
        }
        return true;
    });

    // Audio analysis mode control
    mcp_server.AddTool("self.echo_base.set_audio_mode", "Set audio analysis mode. Available modes:\n"
                       "beat_detection: 鼓点检测模式，跟着音乐跳舞\n"
                       "doa_follow: DOA 声音方向跟随模式\n"
                       "disabled: 禁用音频分析",
    PropertyList({
        Property("mode", kPropertyTypeString),
    }), [board](const PropertyList & properties) -> ReturnValue {
        const std::string &mode = properties["mode"].value<std::string>();
        AudioAnalysisMode analysis_mode = AudioAnalysisMode::DISABLED;

        if (mode == "beat_detection") {
            analysis_mode = AudioAnalysisMode::BEAT_DETECTION;
        } else if (mode == "doa_follow") {
            analysis_mode = AudioAnalysisMode::DOA_FOLLOW;
        } else if (mode == "disabled")
        {
            analysis_mode = AudioAnalysisMode::DISABLED;
        } else
        {
            ESP_LOGE(TAG, "Unknown audio analysis mode: %s", mode.c_str());
            return false;
        }

        board->SetAudioAnalysisMode(analysis_mode);
        ESP_LOGI(TAG, "Audio analysis mode set to: %s", mode.c_str());
        return true;
    });

    // Pomodoro timer control
    mcp_server.AddTool("self.pomodoro.start", "开启番茄钟定时器，设置倒计时时间（1-60分钟，默认5分钟）",
    PropertyList({
        Property("minutes", kPropertyTypeInteger, 5, 1, 60),
    }), [](const PropertyList& properties) -> ReturnValue {
        int minutes = properties["minutes"].value<int>();
        ESP_LOGI(TAG, "Starting pomodoro timer with %d minutes", minutes);
        alarm_start_pomodoro(minutes);
        return true;
    });

    // Pomodoro timer control (start/pause)
    mcp_server.AddTool("self.pomodoro.control", "控制番茄钟定时器的运行状态。参数：start-启动定时器，pause-暂停定时器",
    PropertyList({
        Property("action", kPropertyTypeString),
    }), [](const PropertyList& properties) -> ReturnValue {
        const std::string &action = properties["action"].value<std::string>();
        
        bool success = false;
        if (action == "start") {
            ESP_LOGI(TAG, "Starting pomodoro timer");
            success = alarm_resume_pomodoro();
        } else if (action == "pause") {
            ESP_LOGI(TAG, "Pausing pomodoro timer");
            success = alarm_pause_pomodoro();
        } else {
            ESP_LOGE(TAG, "Unknown pomodoro action: %s (expected 'start' or 'pause')", action.c_str());
            return false;
        }
        return success;
    });
}
