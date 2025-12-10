#include "echoear_tools.h"
#include "EchoEar.h"
#include "echo_base_control.h"
#include "audio_analysis.h"
#include "mcp_server.h"
#include <esp_log.h>

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
                       "cat_nuzzle: 蹭头撒娇动作",
    PropertyList({
        Property("action", kPropertyTypeString),
    }), [board](const PropertyList & properties) -> ReturnValue {
        const std::string &action = properties["action"].value<std::string>();
        int action_value = -1;

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
        } else
        {
            ESP_LOGE(TAG, "Unknown action: %s", action.c_str());
            return false;
        }

        esp_err_t ret = echo_base_control_set_action(action_value);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set action: %d", ret);
            return false;
        }
        return true;
    });

    // Audio analysis mode control
    mcp_server.AddTool("self.echo_base.set_audio_mode", "Set audio analysis mode. Available modes:\n"
                       "beat_detection: 鼓点检测模式，跟着音乐跳舞\n"
                       "doa_follow: DOA 人声音跟随模式\n"
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
}
