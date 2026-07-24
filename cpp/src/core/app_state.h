#pragma once
#include "core/camera.h"
#include "core/events.h"
#include "scene_graph/scene.h"
#include "renderer/renderer.h"
#include "ai/ai_engine.h"

#include <map>
#include <deque>
#include <string>
#include <memory>

namespace ocp {

enum EditMode { MODE_OBJECT = 0, MODE_EDIT = 1, MODE_SCULPT = 2 };

struct AppState {
    Scene scene;
    Camera camera;
    Renderer renderer;
    ObjectGenerator ai_generator;
    EventBus events;

    int selected_tool = 0;
    int current_mode = MODE_OBJECT;
    bool local_space_mode = true;

    float snap_value = 0.0f;
    std::string status_text = "Ready";
    std::string last_ai_result;

    bool show_about = false;
    bool show_console = false;
    bool show_settings = false;
    bool show_light_panel = false;
    bool show_resource_browser = false;

    char ai_prompt[1024] = "";
    char scene_search[128] = "";
    char rename_buf[128] = "";
    bool renaming = false;

    std::deque<std::string> console_log;
    static const int MAX_LOG = 200;

    std::deque<std::string> ai_prompt_history;
    static const int MAX_AI_HISTORY = 20;

    void log_message(const std::string& msg) {
        console_log.push_back(msg);
        if ((int)console_log.size() > MAX_LOG) console_log.pop_front();
    }
};

struct UndoState {
    std::string data;
    std::string description;
};

struct EditModeState {
    SceneNode* edit_node = nullptr;
    int select_mode = 0;
    int tool = 0;
    std::map<SceneNode*, BMesh> bmeshes;
    std::map<SceneNode*, std::vector<Modifier>> modifier_stacks;
};

struct TransformState {
    bool active = false;
    vec3 start_pos;
    vec3 start_rot;
    vec3 start_scl;
    double start_mx = 0, start_my = 0;
};

} // namespace ocp
