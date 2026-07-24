#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"

#include "core/app_state.h"
#include "core/camera.h"
#include "ui/editor_ui.h"
#include "ui/input_handler.h"
#include "editor/edit_tools.h"
#include "editor/sculpt_tools.h"
#include "scene_graph/scene.h"
#include "geometry/bmesh.h"

#include <vector>
#include <map>
#include <string>
#include <algorithm>

namespace ocp {

// ---------------------------------------------------------------------------
// Global pointers set by setup_input() -- used by all GLFW callbacks
// ---------------------------------------------------------------------------
static AppState*      g_app       = nullptr;
static EditModeState* g_edit      = nullptr;
static TransformState* g_transform = nullptr;
static UIState*       g_ui        = nullptr;

// ---------------------------------------------------------------------------
// Mouse interaction state
// ---------------------------------------------------------------------------
static bool mouse_orbiting  = false;
static bool mouse_panning   = false;
static bool mouse_zooming   = false;
static bool alt_held_for_orbit  = false;
static bool right_held_for_orbit = false;
static double last_mx = 0.0;
static double last_my = 0.0;

// ---------------------------------------------------------------------------
// Sculpt state  (referenced by app/main.cpp and ui/editor_ui.cpp)
// ---------------------------------------------------------------------------
vec3                sculpt_hit_pos;
bool                sculpt_hit_valid = false;
sculpt::BrushSettings brush_settings;

vec3&                g_sculpt_hit_pos    = sculpt_hit_pos;
bool&                g_sculpt_hit_valid  = sculpt_hit_valid;
bool                 g_sculpt_stroke_active = false;
sculpt::BrushSettings& g_brush_settings   = brush_settings;

// ---------------------------------------------------------------------------
// Viewport hover flag  (satisfies editor_ui.h extern)
// ---------------------------------------------------------------------------
bool viewport_hovered = false;

// ---------------------------------------------------------------------------
// Undo / Redo
// ---------------------------------------------------------------------------
static std::vector<UndoState> undo_stack;
static std::vector<UndoState> redo_stack;
static const int MAX_UNDO = 50;

// ---------------------------------------------------------------------------
// Helper: sync BMesh back to Mesh on a node
// ---------------------------------------------------------------------------
void sync_bmesh_to_mesh(SceneNode* node, EditModeState& edit, Renderer& renderer) {
    if (!node || !node->mesh) return;
    auto it = edit.bmeshes.find(node);
    if (it != edit.bmeshes.end()) {
        node->mesh->mesh_from_bmesh(it->second);
        renderer.invalidate_mesh(node->mesh.get());
    }
}

// ---------------------------------------------------------------------------
// Undo helpers
// ---------------------------------------------------------------------------
static void save_undo(const std::string& description) {
    if (!g_app) return;
    UndoState s;
    s.data        = g_app->scene.save_to_string();
    s.description = description;
    undo_stack.push_back(std::move(s));
    if ((int)undo_stack.size() > MAX_UNDO)
        undo_stack.erase(undo_stack.begin());
    redo_stack.clear();
}

static void undo() {
    if (!g_app || undo_stack.empty()) return;

    UndoState cur;
    cur.data        = g_app->scene.save_to_string();
    cur.description = "redo";
    redo_stack.push_back(std::move(cur));

    UndoState& prev = undo_stack.back();
    g_app->scene.load_from_string(prev.data);
    g_app->status_text = "Undo: " + prev.description;
    undo_stack.pop_back();
    g_app->log_message("Undo performed");
}

static void redo() {
    if (!g_app || redo_stack.empty()) return;

    UndoState cur;
    cur.data        = g_app->scene.save_to_string();
    cur.description = "undo";
    undo_stack.push_back(std::move(cur));

    UndoState& next = redo_stack.back();
    g_app->scene.load_from_string(next.data);
    g_app->status_text = "Redo: " + next.description;
    redo_stack.pop_back();
    g_app->log_message("Redo performed");
}

// ---------------------------------------------------------------------------
// Mode management  (exit functions before enter functions for call order)
// ---------------------------------------------------------------------------
void exit_edit_mode(AppState& app, EditModeState& edit) {
    if (app.current_mode != MODE_EDIT || !edit.edit_node) return;

    save_undo("Exit Edit Mode");
    sync_bmesh_to_mesh(edit.edit_node, edit, app.renderer);

    edit.bmeshes.erase(edit.edit_node);
    edit.edit_node  = nullptr;
    edit.select_mode = 0;
    edit.tool        = 0;

    app.current_mode = MODE_OBJECT;
    app.status_text  = "Object Mode";
    app.log_message("Exited edit mode");
    app.events.emit(ModeChangedEvent{ MODE_OBJECT });
}

void exit_sculpt_mode(AppState& app, EditModeState& edit) {
    if (app.current_mode != MODE_SCULPT || !edit.edit_node) return;

    save_undo("Exit Sculpt Mode");
    sync_bmesh_to_mesh(edit.edit_node, edit, app.renderer);

    edit.bmeshes.erase(edit.edit_node);
    edit.edit_node = nullptr;

    g_sculpt_stroke_active = false;
    g_sculpt_hit_valid     = false;
    sculpt::clear_mask_weights();

    app.current_mode = MODE_OBJECT;
    app.status_text  = "Object Mode";
    app.log_message("Exited sculpt mode");
    app.events.emit(ModeChangedEvent{ MODE_OBJECT });
}

void enter_edit_mode(AppState& app, EditModeState& edit, SceneNode* node) {
    if (!node || !node->mesh) return;
    if (app.current_mode == MODE_EDIT) exit_edit_mode(app, edit);
    if (app.current_mode == MODE_SCULPT) exit_sculpt_mode(app, edit);

    g_app  = &app;
    g_edit = &edit;
    save_undo("Enter Edit Mode");

    edit.edit_node  = node;
    edit.select_mode = 0;
    edit.tool        = 0;

    BMesh bm;
    node->mesh->bmesh_from_mesh(bm);
    edit.bmeshes[node] = std::move(bm);

    app.current_mode = MODE_EDIT;
    app.status_text  = "Edit Mode - " + node->name;
    app.log_message("Entered edit mode: " + node->name);
    app.events.emit(ModeChangedEvent{ MODE_EDIT });
}

void enter_sculpt_mode(AppState& app, EditModeState& edit, SceneNode* node) {
    if (!node || !node->mesh) return;
    if (app.current_mode == MODE_EDIT) exit_edit_mode(app, edit);
    if (app.current_mode == MODE_SCULPT) exit_sculpt_mode(app, edit);

    g_app  = &app;
    g_edit = &edit;
    save_undo("Enter Sculpt Mode");

    edit.edit_node = node;

    BMesh bm;
    node->mesh->bmesh_from_mesh(bm);
    edit.bmeshes[node] = std::move(bm);

    g_sculpt_stroke_active = false;
    g_sculpt_hit_valid     = false;

    app.current_mode = MODE_SCULPT;
    app.status_text  = "Sculpt Mode - " + node->name;
    app.log_message("Entered sculpt mode: " + node->name);
    app.events.emit(ModeChangedEvent{ MODE_SCULPT });
}

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------
static bool in_viewport(double mx, double my, int win_w, int win_h) {
    return mx > LEFT_PANEL_W && mx < win_w - RIGHT_PANEL_W &&
           my > TOP_OFFSET   && my < win_h - BOTTOM_OFFSET;
}

static void viewport_raycast(GLFWwindow* window, double mx, double my,
                              vec3& out_origin, vec3& out_dir) {
    int win_w, win_h;
    glfwGetWindowSize(window, &win_w, &win_h);
    g_app->camera.screen_to_ray((float)mx, (float)my,
                                 (float)win_w, (float)win_h,
                                 out_origin, out_dir);
}

// ---------------------------------------------------------------------------
// GLFW mouse-button callback
// ---------------------------------------------------------------------------
static void mouse_button_callback(GLFWwindow* window, int button,
                                   int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (!g_app) return;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    last_mx = mx;
    last_my = my;

    bool shift = (mods & GLFW_MOD_SHIFT)   != 0;
    bool ctrl  = (mods & GLFW_MOD_CONTROL)  != 0;
    bool alt   = (mods & GLFW_MOD_ALT)      != 0;

    int win_w, win_h;
    glfwGetWindowSize(window, &win_w, &win_h);
    bool vph = in_viewport(mx, my, win_w, win_h);

    // ---- PRESS ----------------------------------------------------------------
    if (action == GLFW_PRESS) {

        // --- Middle button: orbit / pan ---
        if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            if (shift || ctrl) {
                mouse_panning = true;
            } else {
                mouse_orbiting = true;
            }
            return;
        }

        // --- Right button: camera pan (or orbit when alt-held) ---
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            right_held_for_orbit = true;
            mouse_panning = true;
            return;
        }

        // --- Left button ---
        if (button == GLFW_MOUSE_BUTTON_LEFT) {

            // Alt + LMB = orbit (Maya-style)
            if (alt) {
                alt_held_for_orbit = true;
                mouse_orbiting = true;
                return;
            }

            if (!vph) return;

            // ---- Object mode selection ----
            if (g_app->current_mode == MODE_OBJECT) {
                vec3 ro, rd;
                viewport_raycast(window, mx, my, ro, rd);

                SceneNode* hit_node = nullptr;
                float hit_dist = 0.0f;
                if (g_app->scene.raycast(ro, rd, hit_node, hit_dist)) {
                    if (shift)
                        g_app->scene.toggle_multi_select(hit_node);
                    else
                        g_app->scene.select(hit_node);
                } else {
                    g_app->scene.deselect();
                }
                return;
            }

            // ---- Edit mode selection ----
            if (g_app->current_mode == MODE_EDIT && g_edit->edit_node) {
                auto it = g_edit->bmeshes.find(g_edit->edit_node);
                if (it == g_edit->bmeshes.end()) return;

                BMesh& bm = it->second;

                vec3 ro, rd;
                viewport_raycast(window, mx, my, ro, rd);

                if (g_edit->select_mode == 0) { // vertex
                    BMVert* v = bm.vert_find_nearest(ro + rd * 5.0f, 0.5f);
                    if (v) {
                        if (!shift) bm.select_all(false);
                        v->select = !v->select || shift;
                    } else if (!shift) {
                        bm.select_all(false);
                    }
                } else if (g_edit->select_mode == 1) { // edge
                    float best = 1e10f;
                    BMEdge* e = bm.edge_find_nearest(ro + rd * 5.0f, best);
                    if (e) {
                        if (!shift) bm.select_all(false);
                        e->select = !e->select || shift;
                    } else if (!shift) {
                        bm.select_all(false);
                    }
                } else { // face
                    BMFace* f = bm.face_pick(ro, rd);
                    if (f) {
                        if (!shift) bm.select_all(false);
                        f->select = !f->select || shift;
                    } else if (!shift) {
                        bm.select_all(false);
                    }
                }
                bm.select_flush();
                return;
            }

            // ---- Sculpt mode: start stroke ----
            if (g_app->current_mode == MODE_SCULPT && g_edit->edit_node) {
                g_sculpt_stroke_active = true;
                save_undo("Sculpt Stroke");

                // Compute initial hit position for immediate feedback
                vec3 ro, rd;
                viewport_raycast(window, mx, my, ro, rd);

                SceneNode* hit_node = nullptr;
                float hit_dist = 0.0f;
                if (g_app->scene.raycast(ro, rd, hit_node, hit_dist) &&
                    hit_node == g_edit->edit_node)
                {
                    g_sculpt_hit_pos    = ro + rd * hit_dist;
                    g_sculpt_hit_valid  = true;
                }
                return;
            }
        }
        return;
    }

    // ---- RELEASE --------------------------------------------------------------
    if (action == GLFW_RELEASE) {

        if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            mouse_orbiting = false;
            mouse_panning  = false;
            mouse_zooming  = false;
            return;
        }

        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            right_held_for_orbit = false;
            mouse_panning = false;
            return;
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (alt_held_for_orbit) {
                alt_held_for_orbit = false;
                mouse_orbiting = false;
            }

            // End sculpt stroke -- sync back to mesh
            if (g_sculpt_stroke_active) {
                g_sculpt_stroke_active = false;
                if (g_edit && g_edit->edit_node)
                    sync_bmesh_to_mesh(g_edit->edit_node, *g_edit, g_app->renderer);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// GLFW cursor-position callback
// ---------------------------------------------------------------------------
static void cursor_position_callback(GLFWwindow* window, double mx, double my) {
    ImGui_ImplGlfw_CursorPosCallback(window, mx, my);

    double dx = mx - last_mx;
    double dy = my - last_my;
    last_mx = mx;
    last_my = my;

    if (!g_app) return;

    // --- Camera navigation while mouse button held ---
    if (mouse_orbiting) {
        g_app->camera.orbit((float)dx * 0.3f, (float)dy * 0.3f);
    } else if (mouse_panning) {
        g_app->camera.pan((float)dx * 0.01f, (float)dy * 0.01f);
    } else if (mouse_zooming) {
        g_app->camera.zoom((float)dy * 0.1f);
    }

    // --- Sculpt brush stroke in progress ---
    if (g_sculpt_stroke_active &&
        g_app->current_mode == MODE_SCULPT &&
        g_edit && g_edit->edit_node)
    {
        vec3 ro, rd;
        viewport_raycast(window, mx, my, ro, rd);

        SceneNode* hit_node = nullptr;
        float hit_dist = 0.0f;
        if (g_app->scene.raycast(ro, rd, hit_node, hit_dist) &&
            hit_node == g_edit->edit_node)
        {
            g_sculpt_hit_pos   = ro + rd * hit_dist;
            g_sculpt_hit_valid = true;

            auto it = g_edit->bmeshes.find(g_edit->edit_node);
            if (it != g_edit->bmeshes.end()) {
                vec3 hit_normal(0.0f, 1.0f, 0.0f);
                BMVert* nearest = it->second.vert_find_nearest(
                    g_sculpt_hit_pos, g_brush_settings.radius);
                if (nearest) hit_normal = nearest->no;

                sculpt::apply_brush(it->second, g_sculpt_hit_pos,
                                    hit_normal, g_brush_settings);
            }
        } else {
            g_sculpt_hit_valid = false;
        }
        return;
    }

    // --- Sculpt hover (no active stroke) ---
    if (g_app->current_mode == MODE_SCULPT && g_edit && g_edit->edit_node) {
        vec3 ro, rd;
        viewport_raycast(window, mx, my, ro, rd);

        SceneNode* hit_node = nullptr;
        float hit_dist = 0.0f;
        if (g_app->scene.raycast(ro, rd, hit_node, hit_dist) &&
            hit_node == g_edit->edit_node)
        {
            g_sculpt_hit_pos   = ro + rd * hit_dist;
            g_sculpt_hit_valid = true;
        } else {
            g_sculpt_hit_valid = false;
        }
    }

    // Update viewport hover for UI
    int win_w, win_h;
    glfwGetWindowSize(window, &win_w, &win_h);
    viewport_hovered = in_viewport(mx, my, win_w, win_h);
}

// ---------------------------------------------------------------------------
// GLFW scroll callback
// ---------------------------------------------------------------------------
static void scroll_callback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    ImGui_ImplGlfw_ScrollCallback(window, 0.0, yoffset);
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (!g_app) return;

    g_app->camera.zoom((float)yoffset * 0.5f);
}

// ---------------------------------------------------------------------------
// GLFW key callback
// ---------------------------------------------------------------------------
static void key_callback(GLFWwindow* window, int key, int /*scancode*/,
                          int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(window, key, 0, action, mods);
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    if (!g_app) return;

    bool ctrl  = (mods & GLFW_MOD_CONTROL) != 0;
    bool shift = (mods & GLFW_MOD_SHIFT)   != 0;

    if (action == GLFW_PRESS) {

        // ====================================================================
        // Ctrl + key shortcuts
        // ====================================================================
        if (ctrl) {
            switch (key) {
                case GLFW_KEY_Z:
                    if (shift) redo(); else undo();
                    return;
                case GLFW_KEY_Y:
                    redo();
                    return;
                case GLFW_KEY_D:
                    save_undo("Duplicate");
                    g_app->scene.duplicate_selected();
                    return;
                case GLFW_KEY_V:
                    save_undo("Paste");
                    g_app->scene.paste_clipboard();
                    return;
                case GLFW_KEY_G:
                    if (shift) {
                        save_undo("Ungroup");
                        g_app->scene.ungroup_selected();
                    } else {
                        save_undo("Group");
                        g_app->scene.group_selected();
                    }
                    return;
                case GLFW_KEY_N:
                    undo_stack.clear();
                    redo_stack.clear();
                    g_app->scene.clear();
                    setup_default_scene(*g_app);
                    g_app->log_message("New scene created");
                    return;
                case GLFW_KEY_S:
                    g_app->status_text = "Scene saved";
                    g_app->log_message("Scene saved (placeholder)");
                    return;
                case GLFW_KEY_O:
                    g_app->status_text = "Open (placeholder)";
                    g_app->log_message("Open scene (placeholder)");
                    return;
                case GLFW_KEY_A:
                    if (g_app->current_mode == MODE_EDIT && g_edit->edit_node) {
                        auto it = g_edit->bmeshes.find(g_edit->edit_node);
                        if (it != g_edit->bmeshes.end()) {
                            it->second.select_all(true);
                            g_app->status_text = "All selected";
                        }
                    }
                    return;
                default:
                    return;   // ignore unrecognised Ctrl combos
            }
        }

        // ====================================================================
        // Unmodified / Shift-only key shortcuts
        // ====================================================================
        switch (key) {

            // --- Mode toggling ---
            case GLFW_KEY_TAB:
                if (g_app->current_mode == MODE_OBJECT) {
                    if (g_app->scene.selected_node)
                        enter_edit_mode(*g_app, *g_edit, g_app->scene.selected_node);
                } else if (g_app->current_mode == MODE_EDIT) {
                    exit_edit_mode(*g_app, *g_edit);
                } else if (g_app->current_mode == MODE_SCULPT) {
                    exit_sculpt_mode(*g_app, *g_edit);
                }
                return;

            // --- Transform / modelling tools ---
            case GLFW_KEY_G:
                g_app->selected_tool = 0;
                g_app->status_text   = "Grab Tool";
                g_app->events.emit(ToolChangedEvent{ 0 });
                return;
            case GLFW_KEY_R:
                g_app->selected_tool = 1;
                g_app->status_text   = "Rotate Tool";
                g_app->events.emit(ToolChangedEvent{ 1 });
                return;
            case GLFW_KEY_S:
                g_app->selected_tool = 2;
                g_app->status_text   = "Scale Tool";
                g_app->events.emit(ToolChangedEvent{ 2 });
                return;

            // --- Display toggles ---
            case GLFW_KEY_W:
                g_app->scene.global_wireframe = !g_app->scene.global_wireframe;
                g_app->status_text = g_app->scene.global_wireframe
                    ? "Wireframe On" : "Wireframe Off";
                return;
            case GLFW_KEY_X:
                g_app->scene.xray_mode = !g_app->scene.xray_mode;
                g_app->status_text = g_app->scene.xray_mode
                    ? "X-Ray On" : "X-Ray Off";
                return;

            // --- Focus on selection ---
            case GLFW_KEY_F:
                if (g_app->scene.selected_node) {
                    g_app->camera.focus_on(
                        g_app->scene.selected_node->get_world_position(), 2.0f);
                }
                return;

            // --- Toggle visibility ---
            case GLFW_KEY_H:
                if (g_app->scene.selected_node) {
                    g_app->scene.selected_node->visible =
                        !g_app->scene.selected_node->visible;
                    g_app->status_text =
                        g_app->scene.selected_node->visible ? "Visible" : "Hidden";
                }
                return;

            // --- Console toggle ---
            case GLFW_KEY_GRAVE_ACCENT:
                g_app->show_console = !g_app->show_console;
                return;

            // --- About dialog ---
            case GLFW_KEY_F1:
                g_app->show_about = !g_app->show_about;
                return;

            // --- Delete ---
            case GLFW_KEY_DELETE:
            case GLFW_KEY_BACKSPACE:
                if (g_app->current_mode == MODE_EDIT && g_edit->edit_node) {
                    auto it = g_edit->bmeshes.find(g_edit->edit_node);
                    if (it != g_edit->bmeshes.end()) {
                        save_undo("Delete Geometry");
                        BMesh& bm = it->second;
                        for (int i = bm.vert_count - 1; i >= 0; --i) {
                            if (bm.verts[i]->select)
                                bm.vert_kill(bm.verts[i]);
                        }
                        sync_bmesh_to_mesh(g_edit->edit_node, *g_edit, g_app->renderer);
                    }
                } else {
                    save_undo("Delete");
                    g_app->scene.delete_selected();
                }
                return;

            // --- Escape: cancel / exit mode / deselect ---
            case GLFW_KEY_ESCAPE:
                if (g_transform && g_transform->active) {
                    g_transform->active = false;
                    g_app->status_text  = "Transform cancelled";
                } else if (g_app->current_mode == MODE_EDIT) {
                    exit_edit_mode(*g_app, *g_edit);
                } else if (g_app->current_mode == MODE_SCULPT) {
                    exit_sculpt_mode(*g_app, *g_edit);
                } else {
                    g_app->scene.deselect();
                }
                return;

            // --- Edit-mode select mode switching ---
            case GLFW_KEY_Z:
                if (g_app->current_mode == MODE_EDIT) {
                    g_edit->select_mode = 0;
                    g_app->status_text  = "Vertex Select";
                }
                return;
            case GLFW_KEY_E:
                if (g_app->current_mode == MODE_EDIT) {
                    g_edit->select_mode = 1;
                    g_app->status_text  = "Edge Select";
                }
                return;
            case GLFW_KEY_Q:
                if (g_app->current_mode == MODE_EDIT) {
                    g_edit->select_mode = 2;
                    g_app->status_text  = "Face Select";
                }
                return;

            // --- Standard views (number row) ---
            case GLFW_KEY_1:
                g_app->camera.set_view("Front");
                return;
            case GLFW_KEY_2:
                g_app->camera.set_view("Right");
                return;
            case GLFW_KEY_3:
                g_app->camera.set_view("Top");
                return;

            // --- Numpad views ---
            case GLFW_KEY_KP_1:
                g_app->camera.set_view("Front");
                return;
            case GLFW_KEY_KP_3:
                g_app->camera.set_view("Right");
                return;
            case GLFW_KEY_KP_7:
                g_app->camera.set_view("Top");
                return;
            case GLFW_KEY_KP_5:
                g_app->camera.ortho = !g_app->camera.ortho;
                g_app->status_text  = g_app->camera.ortho
                    ? "Orthographic" : "Perspective";
                return;
            case GLFW_KEY_KP_0:
                g_app->camera.set_view("Perspective");
                return;
            case GLFW_KEY_KP_4:
                g_app->camera.set_view("Left");
                return;
            case GLFW_KEY_KP_6:
                g_app->camera.set_view("Right");
                return;
            case GLFW_KEY_KP_8:
                g_app->camera.set_view("Back");
                return;
            case GLFW_KEY_KP_2:
                g_app->camera.set_view("Front");
                return;

            default:
                return;
        }
    }
}

// ---------------------------------------------------------------------------
// Public: register all GLFW callbacks
// ---------------------------------------------------------------------------
void setup_input(GLFWwindow* window, AppState& app, EditModeState& edit,
                 TransformState& transform, UIState& ui)
{
    g_app       = &app;
    g_edit      = &edit;
    g_transform = &transform;
    g_ui        = &ui;

    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window,   cursor_position_callback);
    glfwSetScrollCallback(window,      scroll_callback);
    glfwSetKeyCallback(window,         key_callback);

    app.log_message("Input callbacks registered");
}

// ---------------------------------------------------------------------------
// Public: per-frame WASD camera movement (right-click held)
// ---------------------------------------------------------------------------
void process_camera_input(GLFWwindow* window, Camera& camera, float dt) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) return;

    float speed = 5.0f * dt;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        speed *= 3.0f;

    float fwd = 0.0f, right = 0.0f, up = 0.0f;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) fwd += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) fwd -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) right += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) right -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) up += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) up -= 1.0f;

    if (fwd != 0.0f || right != 0.0f || up != 0.0f)
        camera.move(fwd, right, up, speed);
}

} // namespace ocp
