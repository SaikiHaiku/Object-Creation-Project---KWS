#pragma once
#include "core/app_state.h"
#include "editor/sculpt_tools.h"

namespace ocp {

static const int LEFT_PANEL_W = 220;
static const int TOP_OFFSET = 54;
static const int RIGHT_PANEL_W = 350;
static const int BOTTOM_OFFSET = 24;

extern bool viewport_hovered;
extern bool sculpt_hit_valid;
extern vec3 sculpt_hit_pos;
extern sculpt::BrushSettings brush_settings;

struct UIState {
    bool show_resource_browser = false;
    bool show_about = false;
    bool show_console = false;
    bool show_settings = false;
    bool show_light_panel = false;

    char scene_search[128] = "";
    char rename_buf[128] = "";
    bool renaming = false;
    int tree_force_state = 0;

    void discover_resources();
};

void setup_default_scene(AppState& app);
void draw_editor_ui(AppState& app, EditModeState& edit, TransformState& transform,
                    UIState& ui, int win_w, int win_h, bool viewport_hov, float fps);

void enter_edit_mode(AppState& app, EditModeState& edit, SceneNode* node);
void exit_edit_mode(AppState& app, EditModeState& edit);
void enter_sculpt_mode(AppState& app, EditModeState& edit, SceneNode* node);
void exit_sculpt_mode(AppState& app, EditModeState& edit);
void sync_bmesh_to_mesh(SceneNode* node, EditModeState& edit, Renderer& renderer);

} // namespace ocp
