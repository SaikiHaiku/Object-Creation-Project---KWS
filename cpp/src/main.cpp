#define GLFW_INCLUDE_NONE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "scene.h"
#include "camera.h"
#include "renderer.h"
#include "primitives.h"
#include "ai_engine.h"
#include "exporters.h"
#include "types.h"
#include "bmesh.h"
#include "edit_tools.h"
#include "sculpt_tools.h"

#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace ocp;

static const char* APP_TITLE = "OCP - Object Creation Project | KitariosWebStudio - KWS";
static int WIN_W = 1600, WIN_H = 900;
static const int LEFT_PANEL_W = 220;
static const int TOP_OFFSET = 54;
static const int RIGHT_PANEL_W = 350;
static const int BOTTOM_OFFSET = 24;

static Scene scene;
static Camera camera;
static Renderer renderer;
static ObjectGenerator ai_generator;
static int selected_tool = 0;
static char ai_prompt[1024] = "";
static std::string status_text = "Ready";
static std::string last_ai_result;
static bool show_about = false;
static bool show_console = false;
static bool show_settings = false;
static bool show_light_panel = false;
static bool show_resource_browser = false;
static char scene_search[128] = "";
static char rename_buf[128] = "";
static bool renaming = false;
static float snap_value = 0.0f;
static std::deque<std::string> console_log;
static const int MAX_LOG = 200;

static bool mouse_orbiting = false, mouse_panning = false, mouse_zooming = false;
static bool alt_held_for_orbit = false;
static bool right_held_for_orbit = false;
static double last_mx = 0, last_my = 0;
static bool viewport_hovered = false;

static bool transform_active = false;
static vec3 transform_start_pos;
static vec3 transform_start_rot;
static vec3 transform_start_scl;
static double transform_start_mx = 0, transform_start_my = 0;

struct UndoState {
    std::string data;
    std::string description;
};
static std::vector<UndoState> undo_stack;
static std::vector<UndoState> redo_stack;
static const int MAX_UNDO = 50;

static std::map<SceneNode*, std::vector<Modifier>> modifier_stacks;

static bool local_space_mode = true;

static std::deque<std::string> ai_prompt_history;
static const int MAX_AI_HISTORY = 20;

enum EditMode { MODE_OBJECT = 0, MODE_EDIT = 1, MODE_SCULPT = 2 };
static EditMode current_mode = MODE_OBJECT;
static std::map<SceneNode*, BMesh> edit_bmeshes;
static SceneNode* edit_node = nullptr;
static int edit_select_mode = 0;
static int edit_tool = 0;
static sculpt::BrushSettings brush_settings;
static vec3 sculpt_hit_pos;
static bool sculpt_hit_valid = false;
static bool sculpt_stroke_active = false;

static double last_frame_time = 0.0;
static float current_fps = 0.0;
static float fps_update_timer = 0.0;
static int frame_counter = 0;

static int tree_force_state = 0;

static std::string to_lower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

static void log_message(const std::string& msg) {
    console_log.push_back(msg);
    if ((int)console_log.size() > MAX_LOG) console_log.pop_front();
}

static void save_undo(const std::string& desc) {
    UndoState s;
    s.data = scene.save_to_string();
    s.description = desc;
    undo_stack.push_back(s);
    if ((int)undo_stack.size() > MAX_UNDO) undo_stack.erase(undo_stack.begin());
    redo_stack.clear();
}

static void sync_bmesh_to_mesh(SceneNode* node) {
    if (!node || !node->mesh) return;
    auto it = edit_bmeshes.find(node);
    if (it != edit_bmeshes.end()) {
        node->mesh->mesh_from_bmesh(it->second);
        renderer.invalidate_mesh(node->mesh.get());
    }
}

static bool undo() {
    if (undo_stack.empty()) return false;
    UndoState current;
    current.data = scene.save_to_string();
    current.description = "Current";
    redo_stack.push_back(current);
    scene.load_from_string(undo_stack.back().data);
    undo_stack.pop_back();
    log_message("Undo: " + (undo_stack.empty() ? "none" : undo_stack.back().description));
    return true;
}

static bool redo() {
    if (redo_stack.empty()) return false;
    UndoState current;
    current.data = scene.save_to_string();
    current.description = "Current";
    undo_stack.push_back(current);
    scene.load_from_string(redo_stack.back().data);
    redo_stack.pop_back();
    log_message("Redo");
    return true;
}

static std::string save_file_dialog(const char* filter) {
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetForegroundWindow();
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn)) return std::string(filename);
    return "";
}

static std::string open_file_dialog(const char* filter) {
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetForegroundWindow();
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) return std::string(filename);
    return "";
}

static const char* OCP_FILTER = "OCP Scene\0*.ocp\0All\0*.*\0";
static const char* OBJ_FILTER = "OBJ Files\0*.obj\0All\0*.*\0";
static const char* STL_FILTER = "STL Files\0*.stl\0All\0*.*\0";
static const char* PNG_FILTER = "PNG Files\0*.png\0All\0*.*\0";

static bool save_scene_to_file() {
    auto p = save_file_dialog(OCP_FILTER);
    if (p.empty()) return false;
    std::string data = scene.save_to_string();
    FILE* f = fopen(p.c_str(), "w");
    if (!f) return false;
    fwrite(data.c_str(), 1, data.size(), f);
    fclose(f);
    status_text = "Saved: " + p;
    log_message("Saved: " + p);
    return true;
}

static bool load_scene_from_file() {
    auto p = open_file_dialog(OCP_FILTER);
    if (p.empty()) return false;
    FILE* f = fopen(p.c_str(), "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); status_text = "Failed to load: empty or invalid file"; return false; }
    std::string data(sz, 0);
    fread(&data[0], 1, sz, f);
    fclose(f);
    scene.load_from_string(data);
    modifier_stacks.clear();
    edit_bmeshes.clear();
    status_text = "Loaded: " + p;
    log_message("Loaded: " + p);
    return true;
}

static void add_primitive(const std::string& name) {
    save_undo("Add " + name);
    auto& creators = get_primitive_creators();
    auto it = creators.find(name);
    if (it == creators.end()) return;
    Mesh m = it->second(1.0f);
    auto node = std::make_unique<SceneNode>();
    node->name = name;
    node->mesh = std::make_unique<Mesh>(m);
    node->material = std::make_unique<Material>();
    node->material->name = name + "_mat";
    scene.add_node(std::move(node));
    status_text = "Added " + name;
    log_message("Added primitive: " + name);
}

static void setup_default_scene() {
    save_undo("New Scene");
    scene.clear();
    scene.lights.clear();
    scene.cameras.clear();

    Camera cam; cam.name = "Camera"; cam.set_view("front_right");
    scene.cameras.push_back(cam);
    camera = cam;

    Light l1; l1.name = "Main Light"; l1.type = Light::DIRECTIONAL;
    l1.position = vec3(5, 10, 7); l1.direction = glm::normalize(vec3(-0.5f, -1.0f, -0.7f));
    scene.lights.push_back(l1);
    Light l2; l2.name = "Fill Light"; l2.type = Light::POINT;
    l2.position = vec3(-3, 5, -2); l2.color = vec4(0.6f, 0.7f, 1.0f, 1); l2.intensity = 0.5f;
    scene.lights.push_back(l2);
    Light l3; l3.name = "Ambient"; l3.type = Light::AMBIENT;
    l3.color = vec4(0.15f, 0.15f, 0.2f, 1);
    scene.lights.push_back(l3);

    auto add_demo = [](const std::string& name, const vec3& pos, const vec4& col, float scale_val) {
        auto node = std::make_unique<SceneNode>();
        node->name = name;
        node->position = pos;
        node->scale = vec3(scale_val);
        node->mesh = std::make_unique<Mesh>(create_cube(1.0f));
        node->material = std::make_unique<Material>();
        node->material->diffuse = col;
        node->material->name = name + "_mat";
        scene.add_node(std::move(node));
    };
    add_demo("Red Cube", vec3(-1.5f, 0.5f, 0), vec4(0.8f, 0.1f, 0.1f, 1), 1.0f);
    {
        auto node = std::make_unique<SceneNode>();
        node->name = "Blue Sphere";
        node->position = vec3(0, 0.5f, 0);
        node->mesh = std::make_unique<Mesh>(create_sphere(0.5f));
        node->material = std::make_unique<Material>();
        node->material->diffuse = vec4(0.1f, 0.3f, 0.8f, 1);
        node->material->name = "Blue_mat";
        scene.add_node(std::move(node));
    }
    {
        auto node = std::make_unique<SceneNode>();
        node->name = "Gold Torus";
        node->position = vec3(1.5f, 0.5f, 0);
        node->mesh = std::make_unique<Mesh>(create_torus(0.4f, 0.15f));
        node->material = std::make_unique<Material>();
        node->material->diffuse = vec4(1.0f, 0.84f, 0.0f, 1);
        node->material->name = "Gold_mat";
        scene.add_node(std::move(node));
    }
    {
        auto node = std::make_unique<SceneNode>();
        node->name = "Ground";
        node->position = vec3(0, 0, 0);
        node->scale = vec3(5.0f);
        node->mesh = std::make_unique<Mesh>(create_plane(10.0f));
        node->material = std::make_unique<Material>();
        node->material->diffuse = vec4(0.3f, 0.6f, 0.3f, 1);
        node->material->name = "Ground_mat";
        node->material->shininess = 2;
        node->material->roughness = 1.0f;
        scene.add_node(std::move(node));
    }

    modifier_stacks.clear();
    status_text = "Default scene loaded";
    log_message("Default scene loaded");
}

static void add_modifier_to_selected(Modifier::Type type) {
    SceneNode* node = scene.selected_node;
    if (!node) return;
    save_undo("Add " + std::string(Modifier::type_name(type)));
    Modifier mod;
    mod.type = type;
    mod.name = Modifier::type_name(type);
    modifier_stacks[node].push_back(mod);
    log_message("Added modifier: " + std::string(Modifier::type_name(type)));
    status_text = "Added " + std::string(Modifier::type_name(type));
}

static void apply_modifier_to_node(SceneNode* node, int index) {
    if (!node || !node->mesh) return;
    auto it = modifier_stacks.find(node);
    if (it == modifier_stacks.end() || index < 0 || index >= (int)it->second.size()) return;
    Modifier& mod = it->second[index];
    if (!mod.enabled) return;

    save_undo("Apply " + mod.name);

    switch (mod.type) {
        case Modifier::SUBDIVISION:
            for (int i = 0; i < mod.subdiv_levels; i++)
                node->mesh->subdivide_catmull_clark();
            break;
        case Modifier::SMOOTH:
            node->mesh->smooth_vertices(mod.smooth_factor, mod.smooth_iterations);
            break;
        case Modifier::MIRROR: {
            int axis = mod.mirror_x ? 0 : (mod.mirror_y ? 1 : 2);
            node->mesh->mirror(axis);
            break;
        }
        case Modifier::SOLIDIFY:
            node->mesh->make_solid(mod.solidify_thickness);
            break;
        case Modifier::ARRAY:
        case Modifier::DECIMATE:
        case Modifier::LATTICE:
            log_message("Modifier apply not yet fully implemented: " + mod.name);
            break;
    }

    node->mesh->dirty = true;
    renderer.invalidate_mesh(node->mesh.get());
    it->second.erase(it->second.begin() + index);
    log_message("Applied modifier: " + mod.name);
}

static void remove_modifier_from_node(SceneNode* node, int index) {
    auto it = modifier_stacks.find(node);
    if (it == modifier_stacks.end() || index < 0 || index >= (int)it->second.size()) return;
    save_undo("Remove " + it->second[index].name);
    it->second.erase(it->second.begin() + index);
    log_message("Removed modifier");
}

static void exit_edit_mode() {
    if (edit_node && current_mode == MODE_EDIT) {
        sync_bmesh_to_mesh(edit_node);
        edit_bmeshes.erase(edit_node);
        log_message("Exited edit mode: " + edit_node->name);
    }
    edit_node = nullptr;
    current_mode = MODE_OBJECT;
}

static void exit_sculpt_mode() {
    if (edit_node && current_mode == MODE_SCULPT) {
        sync_bmesh_to_mesh(edit_node);
        edit_bmeshes.erase(edit_node);
        sculpt::clear_mask_weights();
        log_message("Exited sculpt mode: " + edit_node->name);
    }
    edit_node = nullptr;
    current_mode = MODE_OBJECT;
}

static void enter_edit_mode(SceneNode* node) {
    if (!node || !node->mesh) return;
    if (edit_node == node && current_mode == MODE_EDIT) return;
    exit_edit_mode();
    edit_node = node;
    edit_bmeshes[node] = BMesh();
    node->mesh->bmesh_from_mesh(edit_bmeshes[node]);
    edit_select_mode = 0;
    edit_tool = 0;
    current_mode = MODE_EDIT;
    status_text = "Edit Mode: " + node->name;
    log_message("Entered edit mode: " + node->name);
}

static void enter_sculpt_mode(SceneNode* node) {
    if (!node || !node->mesh) return;
    if (edit_node == node && current_mode == MODE_SCULPT) return;
    exit_edit_mode();
    edit_node = node;
    edit_bmeshes[node] = BMesh();
    node->mesh->bmesh_from_mesh(edit_bmeshes[node]);
    brush_settings = sculpt::BrushSettings();
    current_mode = MODE_SCULPT;
    status_text = "Sculpt Mode: " + node->name;
    log_message("Entered sculpt mode: " + node->name);
}

static void mouse_button_callback(GLFWwindow* w, int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            if (mods & GLFW_MOD_CONTROL) {
                mouse_zooming = true;
            } else {
                mouse_panning = true;
            }
            transform_active = false;
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (viewport_hovered) {
                mouse_orbiting = true;
                right_held_for_orbit = true;
                glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            }
            transform_active = false;
        } else if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (mods & GLFW_MOD_ALT) {
                mouse_orbiting = true;
                alt_held_for_orbit = true;
            } else if (current_mode == MODE_EDIT && viewport_hovered && edit_node) {
                double mx2, my2; glfwGetCursorPos(w, &mx2, &my2);
                vec3 origin, dir;
                camera.screen_to_ray((float)mx2, (float)my2, (float)WIN_W, (float)WIN_H, origin, dir);
                auto it = edit_bmeshes.find(edit_node);
                if (it != edit_bmeshes.end()) {
                    BMesh& bm = it->second;
                    if (edit_select_mode == 0) {
                        float best_dist = 1e10f;
                        BMVert* best_v = bm.vert_find_nearest(vec3(0), 1e10f);
                        for (int i = 0; i < bm.vert_count; ++i) {
                            BMVert* v = bm.verts[i];
                            if (v->hide) continue;
                            vec3 wp = edit_node->get_world_matrix() * vec4(v->co, 1.0f);
                            vec3 to = wp - origin;
                            float t = glm::dot(to, dir);
                            if (t < 0) continue;
                            vec3 closest = origin + dir * t;
                            float d = glm::length(wp - closest);
                            if (d < 0.15f && t < best_dist) { best_dist = t; best_v = v; }
                        }
                        if (best_v && best_dist < 1e10f) {
                            if (mods & GLFW_MOD_CONTROL) best_v->select = !best_v->select;
                            else { bm.select_all(false); best_v->select = true; }
                            status_text = "Selected vertex " + std::to_string(best_v->index);
                        } else if (!(mods & GLFW_MOD_CONTROL)) {
                            bm.select_all(false);
                        }
                    } else if (edit_select_mode == 1) {
                        float best_dist = 1e10f;
                        BMEdge* best_e = nullptr;
                        for (int i = 0; i < bm.edge_count; ++i) {
                            BMEdge* e = bm.edges[i];
                            if (e->hide) continue;
                            vec3 p1 = edit_node->get_world_matrix() * vec4(e->v1->co, 1.0f);
                            vec3 p2 = edit_node->get_world_matrix() * vec4(e->v2->co, 1.0f);
                            vec3 edge_vec = p2 - p1;
                            float t = glm::dot(origin - p1, edge_vec) / glm::dot(edge_vec, edge_vec);
                            t = std::clamp(t, 0.0f, 1.0f);
                            vec3 closest = p1 + edge_vec * t;
                            float d = glm::length(origin + dir * glm::dot(closest - origin, dir) - closest);
                            if (d < 0.12f) { best_e = e; break; }
                        }
                        if (best_e) {
                            if (mods & GLFW_MOD_CONTROL) best_e->select = !best_e->select;
                            else { bm.select_all(false); best_e->select = true; }
                            status_text = "Selected edge " + std::to_string(best_e->index);
                        } else if (!(mods & GLFW_MOD_CONTROL)) {
                            bm.select_all(false);
                        }
                    } else {
                        for (int i = 0; i < bm.face_count; ++i) {
                            BMFace* f = bm.faces[i];
                            if (f->hide) continue;
                            vec3 fc(0);
                            for (int j = 0; j < f->len; ++j)
                                fc += vec3(edit_node->get_world_matrix() * vec4(f->verts[j]->co, 1.0f));
                            fc /= (float)f->len;
                            vec3 fn = f->no;
                            float denom = glm::dot(dir, fn);
                            if (std::abs(denom) < 1e-6f) continue;
                            float t = glm::dot(fc - origin, fn) / denom;
                            if (t < 0) continue;
                            vec3 hit = origin + dir * t;
                            float d = glm::length(hit - fc);
                            if (d < 0.5f) {
                                if (mods & GLFW_MOD_CONTROL) f->select = !f->select;
                                else { bm.select_all(false); f->select = true; }
                                status_text = "Selected face " + std::to_string(f->index);
                                break;
                            }
                        }
                    }
                }
            } else if (current_mode == MODE_SCULPT && viewport_hovered && edit_node) {
                sculpt_stroke_active = true;
                save_undo("Sculpt Stroke");
            } else if (viewport_hovered) {
                double mx2, my2; glfwGetCursorPos(w, &mx2, &my2);
                float vx = (float)mx2, vy = (float)my2;
                vec3 origin, dir;
                camera.screen_to_ray(vx, vy, (float)WIN_W, (float)WIN_H, origin, dir);

                if (scene.selected_node && current_mode == MODE_OBJECT && !(mods & GLFW_MOD_CONTROL)) {
                    transform_active = true;
                    transform_start_pos = scene.selected_node->position;
                    transform_start_rot = scene.selected_node->rotation;
                    transform_start_scl = scene.selected_node->scale;
                    transform_start_mx = mx2;
                    transform_start_my = my2;
                } else {
                    SceneNode* hit; float dist;
                    if (scene.raycast(origin, dir, hit, dist)) {
                        if (mods & GLFW_MOD_CONTROL) {
                            scene.toggle_multi_select(hit);
                            status_text = "Multi-select: " + hit->name + " (" + std::to_string(scene.multi_selection.size()) + " selected)";
                        } else {
                            scene.select(hit);
                            status_text = "Selected: " + hit->name;
                        }
                        strncpy(rename_buf, hit->name.c_str(), sizeof(rename_buf) - 1);
                        rename_buf[sizeof(rename_buf) - 1] = '\0';
                        log_message("Selected: " + hit->name);
                    }
                    else scene.deselect();
                }
            }
        }
    } else {
        if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            mouse_orbiting = false;
            mouse_panning = false;
            mouse_zooming = false;
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            mouse_orbiting = false;
            right_held_for_orbit = false;
            glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (alt_held_for_orbit) {
                mouse_orbiting = false;
                alt_held_for_orbit = false;
            }
            if (transform_active) {
                transform_active = false;
                vec3 new_pos = scene.selected_node ? scene.selected_node->position : vec3(0);
                vec3 new_rot = scene.selected_node ? scene.selected_node->rotation : vec3(0);
                vec3 new_scl = scene.selected_node ? scene.selected_node->scale : vec3(1);
                bool moved = (scene.selected_node &&
                    (transform_start_pos != new_pos || transform_start_rot != new_rot || transform_start_scl != new_scl));
                if (moved) {
                    save_undo("Transform");
                    log_message("Transform confirmed");
                }
            }
            if (current_mode == MODE_SCULPT && sculpt_stroke_active) {
                sculpt_stroke_active = false;
            }
        }
    }
}

static void cursor_position_callback(GLFWwindow*, double mx, double my) {
    float dx = (float)(mx - last_mx), dy = (float)(my - last_my);
    if (mouse_orbiting) camera.orbit(dx * 0.5f, dy * 0.5f);
    else if (mouse_panning) camera.pan(dx, dy);
    else if (mouse_zooming) camera.zoom(-dy * 0.5f);

    if (transform_active && scene.selected_node && current_mode == MODE_OBJECT) {
        float fdx = (float)(mx - transform_start_mx);
        float fdy = (float)(my - transform_start_my);
        float sensitivity = 0.005f;
        if (selected_tool == 0) {
            vec3 right = camera.get_right();
            vec3 up = camera.get_up();
            scene.selected_node->position = transform_start_pos + right * fdx * sensitivity - up * fdy * sensitivity;
        } else if (selected_tool == 1) {
            scene.selected_node->rotation = transform_start_rot + vec3(-fdy * 2.0f, fdx * 2.0f, 0);
        } else if (selected_tool == 2) {
            float s = 1.0f + fdx * sensitivity;
            s = std::max(s, 0.01f);
            scene.selected_node->scale = transform_start_scl * s;
        }
    }

    if (current_mode == MODE_SCULPT && sculpt_stroke_active && edit_node) {
        double mx2, my2; glfwGetCursorPos(glfwGetCurrentContext(), &mx2, &my2);
        vec3 origin, dir;
        camera.screen_to_ray((float)mx2, (float)my2, (float)WIN_W, (float)WIN_H, origin, dir);
        auto it = edit_bmeshes.find(edit_node);
        if (it != edit_bmeshes.end()) {
            BMesh& bm = it->second;
            float best_dist = 1e10f;
            BMVert* best_v = nullptr;
            for (int i = 0; i < bm.vert_count; ++i) {
                BMVert* v = bm.verts[i];
                if (v->hide) continue;
                vec3 wp = edit_node->get_world_matrix() * vec4(v->co, 1.0f);
                vec3 to = wp - origin;
                float t = glm::dot(to, dir);
                if (t < 0) continue;
                vec3 closest = origin + dir * t;
                float d = glm::length(wp - closest);
                if (d < brush_settings.radius && t < best_dist) { best_dist = t; best_v = v; }
            }
            if (best_v) {
                vec3 wp = edit_node->get_world_matrix() * vec4(best_v->co, 1.0f);
                sculpt_hit_pos = wp;
                sculpt_hit_valid = true;
                sculpt::apply_brush(bm, best_v->co, best_v->no, brush_settings);
                sync_bmesh_to_mesh(edit_node);
            }
        }
    } else if (current_mode == MODE_SCULPT && edit_node) {
        double mx2, my2; glfwGetCursorPos(glfwGetCurrentContext(), &mx2, &my2);
        vec3 origin, dir;
        camera.screen_to_ray((float)mx2, (float)my2, (float)WIN_W, (float)WIN_H, origin, dir);
        auto it = edit_bmeshes.find(edit_node);
        if (it != edit_bmeshes.end()) {
            BMesh& bm = it->second;
            float best_dist = 1e10f;
            BMVert* best_v = nullptr;
            for (int i = 0; i < bm.vert_count; ++i) {
                BMVert* v = bm.verts[i];
                if (v->hide) continue;
                vec3 wp = edit_node->get_world_matrix() * vec4(v->co, 1.0f);
                vec3 to = wp - origin;
                float t = glm::dot(to, dir);
                if (t < 0) continue;
                vec3 closest = origin + dir * t;
                float d = glm::length(wp - closest);
                if (d < brush_settings.radius && t < best_dist) { best_dist = t; best_v = v; }
            }
            if (best_v) {
                sculpt_hit_pos = edit_node->get_world_matrix() * vec4(best_v->co, 1.0f);
                sculpt_hit_valid = true;
            } else {
                sculpt_hit_valid = false;
            }
        }
    }

    last_mx = mx; last_my = my;
}

static void scroll_callback(GLFWwindow*, double, double yoff) {
    camera.zoom((float)yoff * 2.0f);
}

static void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    if (h <= 0) h = 1;
    WIN_W = w; WIN_H = h;
    camera.aspect = (float)w / (float)h;
    renderer.resize(w, h);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;
    bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;
    (void)scancode;

    // Suppress bindings while Roblox camera mode (right-click held)
    if (right_held_for_orbit) return;

    if (key == GLFW_KEY_ESCAPE) {
        if (transform_active) {
            scene.selected_node->position = transform_start_pos;
            scene.selected_node->rotation = transform_start_rot;
            scene.selected_node->scale = transform_start_scl;
            transform_active = false;
            status_text = "Transform cancelled";
        }
    }
    else if (key == GLFW_KEY_KP_1) { camera.set_view("front"); status_text = "View: Front"; }
    else if (key == GLFW_KEY_KP_3) { camera.set_view("right"); status_text = "View: Right"; }
    else if (key == GLFW_KEY_KP_7) { camera.set_view("top"); status_text = "View: Top"; }
    else if (key == GLFW_KEY_KP_5) { camera.ortho = !camera.ortho; status_text = camera.ortho ? "Orthographic" : "Perspective"; }
    else if (key == GLFW_KEY_KP_0) { camera.set_view("front_right"); status_text = "View: Camera"; }
    // Blender: 1/2/3 views (no numpad)
    else if (!ctrl && key == GLFW_KEY_1) { camera.set_view("front"); status_text = "View: Front"; }
    else if (!ctrl && key == GLFW_KEY_2) { camera.set_view("right"); status_text = "View: Right"; }
    else if (!ctrl && key == GLFW_KEY_3) { camera.set_view("top"); status_text = "View: Top"; }
    else if (ctrl && key == GLFW_KEY_1) { camera.set_view("back"); status_text = "View: Back"; }
    else if (ctrl && key == GLFW_KEY_2) { camera.set_view("left"); status_text = "View: Left"; }
    else if (ctrl && key == GLFW_KEY_3) { camera.set_view("bottom"); status_text = "View: Bottom"; }
    else if (ctrl && key == GLFW_KEY_Z) undo();
    else if (ctrl && key == GLFW_KEY_Y) redo();
    else if (ctrl && key == GLFW_KEY_D) { save_undo("Duplicate"); scene.duplicate_selected(); status_text = "Duplicated"; log_message("Duplicated"); }
    else if (!ctrl && shift && key == GLFW_KEY_D) { save_undo("Duplicate"); scene.duplicate_selected(); status_text = "Duplicated"; log_message("Duplicated"); }
    else if (ctrl && key == GLFW_KEY_C) { scene.copy_selected(); status_text = "Copied"; log_message("Copied to clipboard"); }
    else if (ctrl && key == GLFW_KEY_V) { save_undo("Paste"); scene.paste_clipboard(); status_text = "Pasted"; log_message("Pasted from clipboard"); }
    else if (ctrl && !shift && key == GLFW_KEY_G) { save_undo("Group"); scene.group_selected(); status_text = "Grouped"; log_message("Grouped selected nodes"); }
    else if (ctrl && shift && key == GLFW_KEY_G) { save_undo("Ungroup"); scene.ungroup_selected(); status_text = "Ungrouped"; log_message("Ungrouped"); }
    else if (ctrl && key == GLFW_KEY_N) setup_default_scene();
    else if (ctrl && key == GLFW_KEY_S) { save_scene_to_file(); }
    else if (ctrl && key == GLFW_KEY_O) { load_scene_from_file(); }
    else if (ctrl && key == GLFW_KEY_A) {
        auto ns = scene.get_all_nodes();
        scene.multi_selection.clear();
        for (auto* n : ns) scene.multi_selection.push_back(n);
        if (!ns.empty()) scene.selected_node = ns[0];
    }
    else if (key == GLFW_KEY_DELETE) {
        if (edit_node && scene.selected_node == edit_node) {
            if (current_mode == MODE_EDIT) exit_edit_mode();
            else if (current_mode == MODE_SCULPT) exit_sculpt_mode();
        }
        if (scene.selected_node) {
            modifier_stacks.erase(scene.selected_node);
            edit_bmeshes.erase(scene.selected_node);
        }
        save_undo("Delete");
        scene.delete_selected();
        status_text = "Deleted";
        log_message("Deleted selected");
    }
    else if (key == GLFW_KEY_G && !ctrl) { selected_tool = 0; status_text = "Tool: Grab (G)"; log_message("Tool: Grab/Move"); }
    else if (key == GLFW_KEY_R && !ctrl) { selected_tool = 1; status_text = "Tool: Rotate (R)"; log_message("Tool: Rotate"); }
    else if (key == GLFW_KEY_S && !ctrl) { selected_tool = 2; status_text = "Tool: Scale (S)"; log_message("Tool: Scale"); }
    else if (key == GLFW_KEY_W && !ctrl) { scene.global_wireframe = !scene.global_wireframe; status_text = scene.global_wireframe ? "Wireframe: ON" : "Wireframe: OFF"; }
    else if (key == GLFW_KEY_X && !ctrl) { scene.xray_mode = !scene.xray_mode; status_text = scene.xray_mode ? "X-Ray: ON" : "X-Ray: OFF"; }
    else if (key == GLFW_KEY_F && scene.selected_node) {
        vec3 c = (scene.selected_node->get_bounding_box_min() + scene.selected_node->get_bounding_box_max()) * 0.5f;
        float s = glm::length(scene.selected_node->get_bounding_box_max() - scene.selected_node->get_bounding_box_min());
        camera.focus_on(c, s);
    }
    else if (key == GLFW_KEY_H) show_console = !show_console;
    else if (key == GLFW_KEY_GRAVE_ACCENT) { scene.grid_enabled = !scene.grid_enabled; status_text = scene.grid_enabled ? "Grid: ON" : "Grid: OFF"; }
    else if (key == GLFW_KEY_F1) show_about = true;
    else if (key == GLFW_KEY_TAB) {
        if (current_mode != MODE_OBJECT) {
            if (current_mode == MODE_EDIT) exit_edit_mode();
            else exit_sculpt_mode();
        } else if (scene.selected_node) {
            if (mods & GLFW_MOD_SHIFT) enter_sculpt_mode(scene.selected_node);
            else enter_edit_mode(scene.selected_node);
        }
    }
    else if (ctrl && key == GLFW_KEY_1) { selected_tool = 0; status_text = "Tool: Move"; }
    else if (ctrl && key == GLFW_KEY_2) { selected_tool = 1; status_text = "Tool: Rotate"; }
    else if (ctrl && key == GLFW_KEY_3) { selected_tool = 2; status_text = "Tool: Scale"; }
}

static void draw_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) setup_default_scene();
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) save_scene_to_file();
            if (ImGui::MenuItem("Load Scene", "Ctrl+O")) load_scene_from_file();
            ImGui::Separator();
            if (ImGui::MenuItem("Import OBJ")) {
                auto p = open_file_dialog(OBJ_FILTER);
                if (!p.empty()) { status_text = "Import: " + p; log_message("Import requested: " + p); }
            }
            if (ImGui::MenuItem("Export OBJ")) {
                auto p = save_file_dialog(OBJ_FILTER);
                if (!p.empty()) { OBJExporter::export_to_file(p, scene); status_text = "Exported: " + p; }
            }
            if (ImGui::MenuItem("Export STL")) {
                auto p = save_file_dialog(STL_FILTER);
                if (!p.empty()) { STLExporter::export_to_file(p, scene); status_text = "Exported: " + p; }
            }
            if (ImGui::MenuItem("Export PNG")) {
                auto p = save_file_dialog(PNG_FILTER);
                if (!p.empty()) { PNGExporter::export_framebuffer(p, WIN_W, WIN_H); status_text = "Screenshot: " + p; }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(glfwGetCurrentContext(), true);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (current_mode == MODE_OBJECT && scene.selected_node) {
                if (ImGui::MenuItem("Enter Edit Mode", "Tab")) enter_edit_mode(scene.selected_node);
                if (ImGui::MenuItem("Enter Sculpt Mode", "Shift+Tab")) enter_sculpt_mode(scene.selected_node);
                ImGui::Separator();
            } else if (current_mode == MODE_EDIT) {
                if (ImGui::MenuItem("Exit Edit Mode", "Tab")) exit_edit_mode();
                ImGui::Separator();
            } else if (current_mode == MODE_SCULPT) {
                if (ImGui::MenuItem("Exit Sculpt Mode", "Tab")) exit_sculpt_mode();
                ImGui::Separator();
            }
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C")) scene.copy_selected();
            if (ImGui::MenuItem("Paste", "Ctrl+V")) { save_undo("Paste"); scene.paste_clipboard(); }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) { save_undo("Duplicate"); scene.duplicate_selected(); }
            if (ImGui::MenuItem("Delete", "Del")) { save_undo("Delete"); scene.delete_selected(); status_text = "Deleted"; }
            ImGui::Separator();
            if (ImGui::MenuItem("Group", "Ctrl+G")) { save_undo("Group"); scene.group_selected(); }
            if (ImGui::MenuItem("Ungroup", "Ctrl+Shift+G")) { save_undo("Ungroup"); scene.ungroup_selected(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) {
                auto ns = scene.get_all_nodes();
                scene.multi_selection.clear();
                for (auto* n : ns) scene.multi_selection.push_back(n);
                if (!ns.empty()) scene.selected_node = ns[0];
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Front")) camera.set_view("front");
            if (ImGui::MenuItem("Back")) camera.set_view("back");
            if (ImGui::MenuItem("Left")) camera.set_view("left");
            if (ImGui::MenuItem("Right")) camera.set_view("right");
            if (ImGui::MenuItem("Top")) camera.set_view("top");
            if (ImGui::MenuItem("Bottom")) camera.set_view("bottom");
            if (ImGui::MenuItem("3D View")) camera.set_view("front_right");
            ImGui::Separator();
            if (ImGui::MenuItem("Focus Selected", "F") && scene.selected_node) {
                vec3 c = (scene.selected_node->get_bounding_box_min() + scene.selected_node->get_bounding_box_max()) * 0.5f;
                float s = glm::length(scene.selected_node->get_bounding_box_max() - scene.selected_node->get_bounding_box_min());
                camera.focus_on(c, s);
            }
            if (ImGui::MenuItem("Toggle Grid", "`")) scene.grid_enabled = !scene.grid_enabled;
            ImGui::Separator();
            ImGui::MenuItem("Wireframe Mode", "W", &scene.global_wireframe);
            ImGui::MenuItem("X-Ray Mode", "X", &scene.xray_mode);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Add")) {
            if (ImGui::MenuItem("Cube")) add_primitive("cube");
            if (ImGui::MenuItem("Sphere")) add_primitive("sphere");
            if (ImGui::MenuItem("Cylinder")) add_primitive("cylinder");
            if (ImGui::MenuItem("Cone")) add_primitive("cone");
            if (ImGui::MenuItem("Torus")) add_primitive("torus");
            if (ImGui::MenuItem("Plane")) add_primitive("plane");
            if (ImGui::MenuItem("Torus Knot")) add_primitive("torus_knot");
            if (ImGui::MenuItem("Ico Sphere")) add_primitive("ico_sphere");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Windows")) {
            if (ImGui::MenuItem("Console", "H")) show_console = !show_console;
            if (ImGui::MenuItem("Lights")) show_light_panel = !show_light_panel;
            if (ImGui::MenuItem("Settings")) show_settings = !show_settings;
            if (ImGui::MenuItem("Resource Library")) show_resource_browser = !show_resource_browser;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About OCP", "F1")) show_about = true;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

static void draw_toolbar() {
    ImGui::SetNextWindowPos(ImVec2(0, 22), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)WIN_W, 32), ImGuiCond_Always);
    ImGui::Begin("##Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::SameLine(8);

    if (ImGui::Button("+")) ImGui::OpenPopup("AddMenu");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add Primitive");
    if (ImGui::BeginPopup("AddMenu")) {
        if (ImGui::MenuItem("Cube")) add_primitive("cube");
        if (ImGui::MenuItem("Sphere")) add_primitive("sphere");
        if (ImGui::MenuItem("Cylinder")) add_primitive("cylinder");
        if (ImGui::MenuItem("Cone")) add_primitive("cone");
        if (ImGui::MenuItem("Torus")) add_primitive("torus");
        if (ImGui::MenuItem("Plane")) add_primitive("plane");
        if (ImGui::MenuItem("Torus Knot")) add_primitive("torus_knot");
        if (ImGui::MenuItem("Ico Sphere")) add_primitive("ico_sphere");
        ImGui::EndPopup();
    }

    ImGui::SameLine(0, 12);
    ImGui::TextDisabled("|");
    ImGui::SameLine(0, 8);

    if (ImGui::RadioButton("G##mv", selected_tool == 0)) selected_tool = 0;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move Tool (G)");
    ImGui::SameLine();
    if (ImGui::RadioButton("E##rt", selected_tool == 1)) selected_tool = 1;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotate Tool (E)");
    ImGui::SameLine();
    if (ImGui::RadioButton("R##sc", selected_tool == 2)) selected_tool = 2;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale Tool (R)");

    ImGui::SameLine(0, 12);
    ImGui::TextDisabled("|");
    ImGui::SameLine(0, 8);

    if (ImGui::SmallButton("Front")) camera.set_view("front");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Front View (Numpad 1)");
    ImGui::SameLine();
    if (ImGui::SmallButton("Top")) camera.set_view("top");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Top View (Numpad 7)");
    ImGui::SameLine();
    if (ImGui::SmallButton("Right")) camera.set_view("right");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right View (Numpad 3)");
    ImGui::SameLine();
    if (ImGui::SmallButton("3D")) camera.set_view("front_right");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("3D View");

    ImGui::SameLine(0, 12);
    ImGui::TextDisabled("|");
    ImGui::SameLine(0, 8);

    if (ImGui::Button(scene.global_wireframe ? "W##on" : "W##off")) { scene.global_wireframe = !scene.global_wireframe; }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Wireframe Toggle (W)");
    ImGui::SameLine();
    if (ImGui::Button(scene.xray_mode ? "X##on" : "X##off")) { scene.xray_mode = !scene.xray_mode; }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("X-Ray Toggle (X)");
    ImGui::SameLine();
    if (ImGui::Button(scene.grid_enabled ? "G##on" : "G##off")) { scene.grid_enabled = !scene.grid_enabled; }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid Toggle (`)");

    ImGui::SameLine(0, 12);
    ImGui::TextDisabled("|");
    ImGui::SameLine(0, 8);

    ImGui::PushItemWidth(80);
    ImGui::DragFloat("##snap", &snap_value, 0.05f, 0.0f, 10.0f, "Snap: %.2f");
    ImGui::PopItemWidth();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap Value");

    ImGui::End();
}

static void draw_scene_tree() {
    ImGui::SetNextWindowPos(ImVec2(0, 54), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)LEFT_PANEL_W, (float)WIN_H - 54), ImGuiCond_Always);
    ImGui::Begin("Scene Hierarchy", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::InputTextWithHint("##search", "Search...", scene_search, sizeof(scene_search));
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 44);
    if (ImGui::SmallButton("All+##exp")) tree_force_state = 1;
    ImGui::SameLine();
    if (ImGui::SmallButton("All-##col")) tree_force_state = -1;
    ImGui::Separator();

    auto nodes = scene.get_all_nodes();
    for (auto* node : nodes) {
        if (scene_search[0] != '\0') {
            std::string ns = to_lower(node->name);
            std::string ss = to_lower(scene_search);
            if (ns.find(ss) == std::string::npos) continue;
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (scene.selected_node == node) flags |= ImGuiTreeNodeFlags_Selected;
        bool in_multi = false;
        for (auto* ms : scene.multi_selection) { if (ms == node) { in_multi = true; break; } }
        if (in_multi && scene.selected_node != node) flags |= ImGuiTreeNodeFlags_Selected;

        std::string label;
        if (node->children.size() > 0) label = "[G] ";
        else if (node->mesh) label = "[M] ";
        else label = "[N] ";
        label += node->name;
        if (!node->visible) label += " *";
        if (node->locked) label += " #";

        if (tree_force_state != 0) {
            ImGui::SetNextItemOpen(tree_force_state > 0);
        }
        ImGui::TreeNodeEx((void*)(uintptr_t)node, flags, "%s", label.c_str());
        if (ImGui::IsItemClicked()) {
            if (ImGui::GetIO().KeyCtrl) {
                scene.toggle_multi_select(node);
            } else {
                scene.select(node);
            }
            strncpy(rename_buf, node->name.c_str(), sizeof(rename_buf) - 1);
            rename_buf[sizeof(rename_buf) - 1] = '\0';
        }
        if (ImGui::IsItemClicked(1)) ImGui::OpenPopup("node_menu");

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            SceneNode* src = node;
            ImGui::SetDragDropPayload("SCENE_NODE_DRAG", &src, sizeof(SceneNode*));
            ImGui::Text("Move %s", node->name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE_DRAG")) {
                SceneNode* dragged = *(SceneNode**)payload->Data;
                if (dragged != node) {
                    save_undo("Reorder");
                    log_message("Reorder: " + dragged->name);
                }
            }
            ImGui::EndDragDropTarget();
        }
    }
    tree_force_state = 0;

    if (ImGui::BeginPopup("node_menu")) {
        if (ImGui::MenuItem("Rename")) renaming = true;
        if (ImGui::MenuItem("Toggle Visibility") && scene.selected_node) {
            save_undo("Toggle visibility");
            scene.selected_node->visible = !scene.selected_node->visible;
        }
        if (ImGui::MenuItem("Lock/Unlock") && scene.selected_node) {
            scene.selected_node->locked = !scene.selected_node->locked;
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C")) scene.copy_selected();
        if (ImGui::MenuItem("Paste", "Ctrl+V")) { save_undo("Paste"); scene.paste_clipboard(); }
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) { save_undo("Duplicate"); scene.duplicate_selected(); }
        if (ImGui::MenuItem("Delete", "Del")) { save_undo("Delete"); scene.delete_selected(); }
        ImGui::Separator();
        if (ImGui::MenuItem("Group", "Ctrl+G")) { save_undo("Group"); scene.group_selected(); }
        if (ImGui::MenuItem("Ungroup", "Ctrl+Shift+G")) { save_undo("Ungroup"); scene.ungroup_selected(); }
        ImGui::Separator();
        if (ImGui::MenuItem("Focus", "F") && scene.selected_node) {
            vec3 center = (scene.selected_node->get_bounding_box_min() + scene.selected_node->get_bounding_box_max()) * 0.5f;
            float size = glm::length(scene.selected_node->get_bounding_box_max() - scene.selected_node->get_bounding_box_min());
            camera.focus_on(center, size);
        }
        ImGui::EndPopup();
    }

    if (renaming && scene.selected_node) {
        ImGui::OpenPopup("Rename Node");
    }
    if (ImGui::BeginPopupModal("Rename Node", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (scene.selected_node) {
            ImGui::InputText("Name", rename_buf, sizeof(rename_buf));
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                save_undo("Rename");
                scene.selected_node->name = rename_buf;
                renaming = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                renaming = false;
                ImGui::CloseCurrentPopup();
            }
        } else {
            renaming = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();
    ImGui::Text("Nodes: %d", scene.get_node_count());
    ImGui::End();
}

static void draw_properties() {
    ImGui::SetNextWindowPos(ImVec2((float)WIN_W - RIGHT_PANEL_W, 54), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)RIGHT_PANEL_W, (float)WIN_H * 0.5f - 27), ImGuiCond_Always);
    ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    SceneNode* node = scene.selected_node;
    if (!node) {
        ImGui::TextDisabled("No selection");
        ImGui::End();
        return;
    }

    char name_buf[128];
    strncpy(name_buf, node->name.c_str(), sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    if (ImGui::InputText("##name", name_buf, sizeof(name_buf))) {
        node->name = name_buf;
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("PropsTabs")) {

        if (ImGui::BeginTabItem("Transform")) {
            ImGui::Checkbox("Local", &local_space_mode);
            ImGui::SameLine();
            ImGui::Checkbox("Visible", &node->visible);
            ImGui::SameLine();
            ImGui::Checkbox("Locked", &node->locked);

            ImGui::Separator();

            ImGui::Text("Position");
            if (local_space_mode) {
                ImGui::DragFloat3("##pos", &node->position.x, 0.1f);
            } else {
                vec3 wp = node->get_world_position();
                ImGui::DragFloat3("##wpos", &wp.x, 0.1f);
            }
            if (ImGui::SmallButton("Reset##pos")) node->position = vec3(0);

            ImGui::Separator();
            ImGui::Text("Rotation");
            ImGui::DragFloat3("##rot", &node->rotation.x, 1.0f);
            if (ImGui::SmallButton("Reset##rot")) node->rotation = vec3(0);

            ImGui::Separator();
            ImGui::Text("Scale");
            ImGui::DragFloat3("##scl", &node->scale.x, 0.05f, 0.01f, 100.0f);
            if (ImGui::SmallButton("Reset##scl")) node->scale = vec3(1);

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Material")) {
            if (node->material) {
                vec4& d = node->material->diffuse;
                float col[4] = {d.r, d.g, d.b, d.a};
                if (ImGui::ColorEdit4("Base Color", col)) {
                    d = vec4(col[0], col[1], col[2], col[3]);
                    node->material->opacity = col[3];
                }

                vec4& sp = node->material->specular;
                float scol[4] = {sp.r, sp.g, sp.b, sp.a};
                if (ImGui::ColorEdit4("Specular", scol))
                    sp = vec4(scol[0], scol[1], scol[2], scol[3]);

                vec4& em = node->material->emission;
                float ecol[4] = {em.r, em.g, em.b, em.a};
                if (ImGui::ColorEdit4("Emission", ecol))
                    em = vec4(ecol[0], ecol[1], ecol[2], ecol[3]);

                ImGui::Separator();
                ImGui::Text("PBR Properties");
                ImGui::SliderFloat("Metallic", &node->material->metallic, 0.0f, 1.0f);
                ImGui::SliderFloat("Roughness", &node->material->roughness, 0.0f, 1.0f);
                ImGui::SliderFloat("Shininess", &node->material->shininess, 1.0f, 200.0f);
                ImGui::SliderFloat("Opacity", &node->material->opacity, 0.0f, 1.0f);

                ImGui::Separator();
                ImGui::Checkbox("Wireframe", &node->material->wireframe);
                ImGui::SameLine();
                ImGui::Checkbox("Backface Cull", &node->material->backface_culling);

                ImGui::Separator();
                ImGui::Text("Texture Slots");
                if (ImGui::Button("Base Color Map...")) { log_message("Texture: Base Color - not yet implemented"); }
                if (ImGui::Button("Normal Map...")) { log_message("Texture: Normal - not yet implemented"); }
                if (ImGui::Button("Roughness Map...")) { log_message("Texture: Roughness - not yet implemented"); }
                if (ImGui::Button("Metallic Map...")) { log_message("Texture: Metallic - not yet implemented"); }
                if (ImGui::Button("Emission Map...")) { log_message("Texture: Emission - not yet implemented"); }
            } else {
                ImGui::TextDisabled("No material assigned");
                if (ImGui::Button("Create Material")) {
                    node->material = std::make_unique<Material>();
                    node->material->name = node->name + "_mat";
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Modifiers")) {
            if (ImGui::Button("Add Modifier")) ImGui::OpenPopup("AddModifierPopup");
            if (ImGui::BeginPopup("AddModifierPopup")) {
                if (ImGui::MenuItem("Subdivision Surface")) add_modifier_to_selected(Modifier::SUBDIVISION);
                if (ImGui::MenuItem("Mirror")) add_modifier_to_selected(Modifier::MIRROR);
                if (ImGui::MenuItem("Array")) add_modifier_to_selected(Modifier::ARRAY);
                if (ImGui::MenuItem("Smooth")) add_modifier_to_selected(Modifier::SMOOTH);
                if (ImGui::MenuItem("Lattice")) add_modifier_to_selected(Modifier::LATTICE);
                if (ImGui::MenuItem("Decimate")) add_modifier_to_selected(Modifier::DECIMATE);
                if (ImGui::MenuItem("Solidify")) add_modifier_to_selected(Modifier::SOLIDIFY);
                ImGui::EndPopup();
            }

            ImGui::Separator();

            auto it = modifier_stacks.find(node);
            if (it != modifier_stacks.end() && !it->second.empty()) {
                auto& mods = it->second;
                bool modified = false;
                int apply_idx = -1;
                int remove_idx = -1;
                for (int i = 0; i < (int)mods.size(); i++) {
                    Modifier& mod = mods[i];
                    ImGui::PushID(i);

                    std::string hdr = std::string(Modifier::type_name(mod.type)) + "##mod_" + std::to_string(i);
                    if (ImGui::CollapsingHeader(hdr.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::Checkbox("Enabled", &mod.enabled);
                        ImGui::SameLine();
                        if (i > 0 && ImGui::SmallButton("Up")) { std::swap(mods[i], mods[i - 1]); }
                        ImGui::SameLine();
                        if (i < (int)mods.size() - 1 && ImGui::SmallButton("Down")) { std::swap(mods[i], mods[i + 1]); }

                        ImGui::Separator();

                        switch (mod.type) {
                            case Modifier::SUBDIVISION:
                                ImGui::SliderInt("Levels", &mod.subdiv_levels, 1, 4);
                                break;
                            case Modifier::MIRROR:
                                ImGui::Checkbox("X", &mod.mirror_x); ImGui::SameLine();
                                ImGui::Checkbox("Y", &mod.mirror_y); ImGui::SameLine();
                                ImGui::Checkbox("Z", &mod.mirror_z);
                                ImGui::DragFloat("Offset", &mod.mirror_offset, 0.01f);
                                break;
                            case Modifier::ARRAY:
                                ImGui::SliderInt("Count", &mod.array_count, 1, 100);
                                ImGui::DragFloat3("Offset", &mod.array_offset.x, 0.1f);
                                ImGui::Checkbox("Relative", &mod.array_relative);
                                break;
                            case Modifier::SMOOTH:
                                ImGui::SliderFloat("Factor", &mod.smooth_factor, 0.0f, 1.0f);
                                ImGui::SliderInt("Iterations", &mod.smooth_iterations, 1, 20);
                                break;
                            case Modifier::LATTICE:
                                ImGui::DragFloat3("Resolution", &mod.lattice_resolution.x, 1.0f, 2.0f, 32.0f);
                                break;
                            case Modifier::DECIMATE:
                                ImGui::SliderFloat("Ratio", &mod.decimate_ratio, 0.01f, 1.0f);
                                break;
                            case Modifier::SOLIDIFY:
                                ImGui::DragFloat("Thickness", &mod.solidify_thickness, 0.01f, 0.001f, 10.0f);
                                break;
                        }

                        ImGui::Separator();
                        if (ImGui::Button("Apply")) { apply_idx = i; }
                        ImGui::SameLine();
                        if (ImGui::Button("Remove")) { remove_idx = i; }
                    }

                    ImGui::PopID();
                    if (apply_idx >= 0 || remove_idx >= 0) { modified = true; break; }
                }
                if (apply_idx >= 0) apply_modifier_to_node(node, apply_idx);
                if (remove_idx >= 0) remove_modifier_from_node(node, remove_idx);
                (void)modified;
            } else {
                ImGui::TextDisabled("No modifiers");
                ImGui::TextWrapped("Click 'Add Modifier' to add a modifier to this object.");
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Mesh")) {
            if (node->mesh) {
                ImGui::Text("Vertices: %d", node->mesh->get_vertex_count());
                ImGui::Text("Triangles: %d", node->mesh->get_triangle_count());
                ImGui::Text("Faces: %d", (int)node->mesh->faces.size());

                ImGui::Separator();
                ImGui::Text("Operations");

                if (ImGui::Button("Subdivide")) {
                    save_undo("Subdivide");
                    node->mesh->subdivide();
                    renderer.invalidate_mesh(node->mesh.get());
                }
                ImGui::SameLine();
                if (ImGui::Button("Smooth")) {
                    save_undo("Smooth");
                    node->mesh->smooth_vertices();
                    renderer.invalidate_mesh(node->mesh.get());
                }

                if (ImGui::Button("Invert Faces")) {
                    save_undo("Invert");
                    node->mesh->invert_faces();
                    renderer.invalidate_mesh(node->mesh.get());
                }
                ImGui::SameLine();
                if (ImGui::Button("Weld Vertices")) {
                    save_undo("Weld");
                    node->mesh->weld_vertices();
                    renderer.invalidate_mesh(node->mesh.get());
                }

                if (ImGui::Button("Center Pivot")) {
                    save_undo("Center Pivot");
                    node->mesh->center_pivot();
                    renderer.invalidate_mesh(node->mesh.get());
                }
                ImGui::SameLine();
                if (ImGui::Button("Recalc Normals")) {
                    node->mesh->compute_normals();
                    renderer.invalidate_mesh(node->mesh.get());
                }

                if (ImGui::Button("Flip Normals")) {
                    save_undo("Flip Normals");
                    node->mesh->flip_normals();
                    renderer.invalidate_mesh(node->mesh.get());
                }
            } else {
                ImGui::TextDisabled("No mesh");
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

static void draw_ai_panel() {
    ImGui::SetNextWindowPos(ImVec2((float)WIN_W - RIGHT_PANEL_W, (float)WIN_H * 0.5f + 27), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)RIGHT_PANEL_W, (float)WIN_H * 0.5f - 27), ImGuiCond_Always);
    ImGui::Begin("AI Object Generator", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::TextWrapped("Describe what to create (FR/EN):");
    ImGui::InputTextMultiline("##prompt", ai_prompt, sizeof(ai_prompt), ImVec2(-1, 70));

    if (ImGui::Button("Generate", ImVec2(-1, 30))) {
        ai_prompt_history.push_back(std::string(ai_prompt));
        if ((int)ai_prompt_history.size() > MAX_AI_HISTORY)
            ai_prompt_history.pop_front();
        save_undo("AI Generate");
        ai_generator.generate_from_prompt(ai_prompt, &scene);
        last_ai_result = ai_generator.get_last_description();
        status_text = "AI Generated: " + last_ai_result;
        log_message("AI: " + last_ai_result);
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Quick Examples", ImGuiTreeNodeFlags_DefaultOpen)) {
        int col = 0;
        auto exbtn = [&](const char* label, const char* prompt) {
            if (col > 0) ImGui::SameLine();
            if (ImGui::SmallButton(label)) strcpy(ai_prompt, prompt);
            col++;
            if (col >= 4) col = 0;
        };
        exbtn("House", "house");
        exbtn("Robot", "robot");
        exbtn("Castle", "castle");
        exbtn("Space", "spaceship");
        exbtn("Tree", "tree");
        exbtn("Sword", "sword");
        exbtn("Snowman", "snowman");
        exbtn("DNA", "dna");
        exbtn("Skull", "skull");
        exbtn("Plane", "airplane");
        exbtn("Flower", "flower");
        exbtn("Mushroom", "mushroom");
        exbtn("Donut", "donut pink glaze");
        exbtn("Spiral", "neon spiral 5x circle");
        if (col > 0) ImGui::NewLine();
    }

    if (!ai_prompt_history.empty() && ImGui::CollapsingHeader("Prompt History")) {
        for (int i = (int)ai_prompt_history.size() - 1; i >= 0; i--) {
            if (ImGui::Selectable(ai_prompt_history[i].c_str())) {
                strncpy(ai_prompt, ai_prompt_history[i].c_str(), sizeof(ai_prompt) - 1);
                ai_prompt[sizeof(ai_prompt) - 1] = '\0';
            }
        }
    }

    if (!last_ai_result.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("Last: %s", last_ai_result.c_str());
    }

    ImGui::Separator();
    if (ImGui::Button("Clear Scene", ImVec2(-1, 0))) setup_default_scene();

    ImGui::End();
}

static void draw_light_panel() {
    if (!show_light_panel) return;
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Lights", &show_light_panel)) {
        for (int i = 0; i < (int)scene.lights.size(); i++) {
            Light& l = scene.lights[i];
            ImGui::PushID(i);
            if (ImGui::TreeNode(l.name.c_str())) {
                ImGui::Checkbox("Enabled", &l.enabled);
                const char* types[] = {"Point", "Directional", "Spot", "Ambient"};
                int t = (int)l.type;
                if (ImGui::Combo("Type", &t, types, 4)) l.type = (Light::Type)t;
                ImGui::InputFloat3("Position", &l.position.x);
                ImGui::InputFloat3("Direction", &l.direction.x);
                float c[3] = {l.color.r, l.color.g, l.color.b};
                if (ImGui::ColorEdit3("Color", c)) l.color = vec4(c[0], c[1], c[2], 1.0f);
                ImGui::SliderFloat("Intensity", &l.intensity, 0.0f, 5.0f);
                ImGui::SliderFloat("Range", &l.range, 1.0f, 100.0f);
                if (l.type == Light::SPOT) {
                    ImGui::SliderFloat("Spot Angle", &l.spot_angle, 5.0f, 90.0f);
                    ImGui::SliderFloat("Spot Exp", &l.spot_exponent, 1.0f, 128.0f);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::Separator();
        if (ImGui::Button("Add Point Light")) {
            Light l; l.name = "Point Light_" + std::to_string(scene.lights.size());
            l.type = Light::POINT; l.position = vec3(0, 3, 3);
            scene.lights.push_back(l);
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Directional")) {
            Light l; l.name = "Directional_" + std::to_string(scene.lights.size());
            l.type = Light::DIRECTIONAL; l.direction = glm::normalize(vec3(-0.5f, -1, -0.7f));
            scene.lights.push_back(l);
        }
    }
    ImGui::End();
}

static void draw_console() {
    if (!show_console) return;
    ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Console", &show_console)) {
        if (ImGui::Button("Clear")) console_log.clear();
        ImGui::Separator();
        ImGui::BeginChild("LogRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
        for (auto& msg : console_log) {
            ImGui::TextUnformatted(msg.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }
    ImGui::End();
}

struct ResourceLibrary {
    bool loaded = false;
    std::string base_path;
    std::vector<std::string> texture_categories;
    std::map<std::string, std::vector<std::string>> textures_by_cat;
    std::vector<std::string> hdri_files;
    std::vector<std::string> material_files;
    std::vector<std::string> scene_files;
    std::vector<std::string> brush_files;
    std::vector<std::string> model_files;
    int selected_tab = 0;
    int selected_category = 0;
    int selected_item = -1;
    char search_buf[128] = "";
};

static ResourceLibrary resource_lib;

static void discover_resources() {
    namespace fs = std::filesystem;
    char exe_path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    fs::path exe_dir = fs::path(exe_path).parent_path();
    resource_lib.base_path = (exe_dir / "resources").string();

    if (!fs::exists(resource_lib.base_path)) {
        fs::path alt = fs::path("E:\\Object Creation Project - KWS") / "resources";
        if (fs::exists(alt)) {
            resource_lib.base_path = alt.string();
        } else {
            log_message("Resource library not found at: " + resource_lib.base_path);
            return;
        }
    }

    fs::path tex_dir = fs::path(resource_lib.base_path) / "textures";
    if (fs::exists(tex_dir)) {
        for (auto& cat : fs::directory_iterator(tex_dir)) {
            if (cat.is_directory()) {
                std::string cat_name = cat.path().filename().string();
                resource_lib.texture_categories.push_back(cat_name);
                std::vector<std::string> files;
                for (auto& f : fs::directory_iterator(cat.path())) {
                    if (f.path().extension() == ".png") {
                        files.push_back(f.path().filename().string());
                    }
                }
                std::sort(files.begin(), files.end());
                resource_lib.textures_by_cat[cat_name] = files;
            }
        }
        std::sort(resource_lib.texture_categories.begin(), resource_lib.texture_categories.end());
    }

    auto collect = [&](const std::string& subdir, std::vector<std::string>& out, const std::string& ext) {
        fs::path dir = fs::path(resource_lib.base_path) / subdir;
        if (fs::exists(dir)) {
            for (auto& f : fs::directory_iterator(dir)) {
                if (f.path().extension() == ext) {
                    out.push_back(f.path().filename().string());
                }
            }
            std::sort(out.begin(), out.end());
        }
    };
    collect("hdri", resource_lib.hdri_files, ".hdr");
    collect("materials", resource_lib.material_files, ".json");
    collect("scenes", resource_lib.scene_files, ".json");
    collect("brushes", resource_lib.brush_files, ".png");
    collect("models", resource_lib.model_files, ".ocpm");

    int total = 0;
    for (auto& kv : resource_lib.textures_by_cat) total += (int)kv.second.size();
    total += (int)resource_lib.hdri_files.size();
    total += (int)resource_lib.material_files.size();
    total += (int)resource_lib.scene_files.size();
    total += (int)resource_lib.brush_files.size();
    total += (int)resource_lib.model_files.size();

    resource_lib.loaded = true;
    log_message("Resource library loaded: " + std::to_string(resource_lib.texture_categories.size()) + " texture categories, " +
                std::to_string(total) + " total assets from " + resource_lib.base_path);
}

static void draw_resource_browser() {
    if (!show_resource_browser) return;
    ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Resource Library - KitariosWebStudio", &show_resource_browser)) {
        if (!resource_lib.loaded) {
            if (ImGui::Button("Scan for Resources")) {
                discover_resources();
            }
            ImGui::TextWrapped("Click to scan for the bundled resource library (textures, HDRIs, materials, scenes, models).");
        } else {
            ImGui::InputText("Search", resource_lib.search_buf, sizeof(resource_lib.search_buf));
            ImGui::Separator();

            const char* tabs[] = {"Textures", "HDRI", "Materials", "Scenes", "Models", "Brushes"};
            int tab_count = 6;
            for (int i = 0; i < tab_count; i++) {
                if (i > 0) ImGui::SameLine();
                if (ImGui::RadioButton(tabs[i], resource_lib.selected_tab == i)) {
                    resource_lib.selected_tab = i;
                    resource_lib.selected_item = -1;
                    resource_lib.selected_category = 0;
                }
            }
            ImGui::Separator();

            if (resource_lib.selected_tab == 0) {
                ImGui::BeginChild("CatList", ImVec2(150, 0), ImGuiChildFlags_Borders);
                for (int i = 0; i < (int)resource_lib.texture_categories.size(); i++) {
                    bool sel = (resource_lib.selected_category == i);
                    if (ImGui::Selectable(resource_lib.texture_categories[i].c_str(), sel)) {
                        resource_lib.selected_category = i;
                        resource_lib.selected_item = -1;
                    }
                }
                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::BeginChild("ItemList", ImVec2(0, 0), ImGuiChildFlags_Borders);
                if (resource_lib.selected_category < (int)resource_lib.texture_categories.size()) {
                    std::string cat = resource_lib.texture_categories[resource_lib.selected_category];
                    auto it = resource_lib.textures_by_cat.find(cat);
                    if (it != resource_lib.textures_by_cat.end()) {
                        std::string search = resource_lib.search_buf;
                        std::transform(search.begin(), search.end(), search.begin(), ::tolower);
                        for (int i = 0; i < (int)it->second.size(); i++) {
                            std::string fname = it->second[i];
                            if (!search.empty()) {
                                std::string lower = fname;
                                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                                if (lower.find(search) == std::string::npos) continue;
                            }
                            bool sel = (resource_lib.selected_item == i);
                            if (ImGui::Selectable(fname.c_str(), sel)) {
                                resource_lib.selected_item = i;
                            }
                            if (ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGui::Text("%s / %s", cat.c_str(), fname.c_str());
                                ImGui::Text("Path: %s/textures/%s/%s", resource_lib.base_path.c_str(), cat.c_str(), fname.c_str());
                                ImGui::EndTooltip();
                            }
                        }
                    }
                }
                ImGui::EndChild();
            } else {
                std::vector<std::string>* items = nullptr;
                const char* label = "";
                switch (resource_lib.selected_tab) {
                    case 1: items = &resource_lib.hdri_files; label = "HDRI Environment Maps"; break;
                    case 2: items = &resource_lib.material_files; label = "Material Presets"; break;
                    case 3: items = &resource_lib.scene_files; label = "Template Scenes"; break;
                    case 4: items = &resource_lib.model_files; label = "3D Mesh Templates"; break;
                    case 5: items = &resource_lib.brush_files; label = "Brush Presets"; break;
                    default: break;
                }
                ImGui::Text("%s (%d items)", label, items ? (int)items->size() : 0);
                ImGui::Separator();
                if (items) {
                    ImGui::BeginChild("BrowseList", ImVec2(0, 0), ImGuiChildFlags_Borders);
                    std::string search = resource_lib.search_buf;
                    std::transform(search.begin(), search.end(), search.begin(), ::tolower);
                    for (int i = 0; i < (int)items->size(); i++) {
                        std::string fname = (*items)[i];
                        if (!search.empty()) {
                            std::string lower = fname;
                            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                            if (lower.find(search) == std::string::npos) continue;
                        }
                        bool sel = (resource_lib.selected_item == i);
                        if (ImGui::Selectable(fname.c_str(), sel)) {
                            resource_lib.selected_item = i;
                        }
                    }
                    ImGui::EndChild();
                }
            }

            ImGui::Separator();
            int total_items = 0;
            for (auto& kv : resource_lib.textures_by_cat) total_items += (int)kv.second.size();
            total_items += (int)resource_lib.hdri_files.size() + (int)resource_lib.material_files.size();
            total_items += (int)resource_lib.scene_files.size() + (int)resource_lib.model_files.size();
            total_items += (int)resource_lib.brush_files.size();
            ImGui::Text("Total: %d assets | Path: %s", total_items, resource_lib.base_path.c_str());
        }
    }
    ImGui::End();
}

static void draw_settings() {
    if (!show_settings) return;
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", &show_settings)) {
        ImGui::Text("Camera");
        ImGui::SliderFloat("FOV", &camera.fov, 10.0f, 170.0f);
        ImGui::Checkbox("Orthographic", &camera.ortho);
        if (camera.ortho) ImGui::SliderFloat("Ortho Size", &camera.ortho_size, 1.0f, 50.0f);

        ImGui::Separator();
        ImGui::Text("Scene");
        float bg[4] = {scene.background_color.r, scene.background_color.g, scene.background_color.b, scene.background_color.a};
        if (ImGui::ColorEdit4("Background", bg)) scene.background_color = vec4(bg[0], bg[1], bg[2], bg[3]);
        ImGui::Checkbox("Show Grid", &scene.grid_enabled);
        ImGui::SliderFloat("Grid Size", &scene.grid_size, 5.0f, 100.0f);
        ImGui::SliderInt("Grid Divisions", &scene.grid_subdivisions, 5, 50);

        ImGui::Separator();
        ImGui::Text("Undo Stack: %d", (int)undo_stack.size());
        ImGui::Text("Redo Stack: %d", (int)redo_stack.size());
    }
    ImGui::End();
}

static void draw_about() {
    if (!show_about) return;
    ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_Always);
    if (ImGui::Begin("About OCP", &show_about, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("OCP - Object Creation Project");
        ImGui::Separator();
        ImGui::Text("Version 3.1 (C++)");
        ImGui::Text("Build: %s %s", __DATE__, __TIME__);
        ImGui::Separator();
        ImGui::TextWrapped("Professional 3D object creation tool with AI-powered\nprocedural generation, modifier stack, & organic deformation.");
        ImGui::Separator();
        ImGui::TextWrapped("Created by KitariosWebStudio - KWS");
        ImGui::TextWrapped("OpenGL 3.3 Core | Dear ImGui | GLFW | GLM");
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Features", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BulletText("Manual primitive creation (9 types)");
            ImGui::BulletText("60+ AI procedural generators with organic deformation");
            ImGui::BulletText("Bilingual prompt engine (FR/EN)");
            ImGui::BulletText("Full PBR material editor with texture slots");
            ImGui::BulletText("Light management");
            ImGui::BulletText("Modifier stack (Subdivision, Mirror, Array, Smooth, etc.)");
            ImGui::BulletText("Edit Mode: BMesh topology, 33 modeling tools");
            ImGui::BulletText("Sculpt Mode: 10 brushes with symmetry");
            ImGui::BulletText("OBJ/STL/PNG export");
            ImGui::BulletText("Scene save/load");
            ImGui::BulletText("Undo/Redo (50 levels)");
            ImGui::BulletText("Multi-selection, Copy/Paste, Group/Ungroup");
            ImGui::BulletText("Wireframe & X-Ray modes");
            ImGui::BulletText("Resource Library browser");
            ImGui::BulletText("Context-sensitive properties with tabbed UI");
        }

        if (ImGui::CollapsingHeader("Keyboard Shortcuts")) {
            ImGui::Columns(2, nullptr, false);
            ImGui::Text("G"); ImGui::NextColumn(); ImGui::Text("Move Tool"); ImGui::NextColumn();
            ImGui::Text("E"); ImGui::NextColumn(); ImGui::Text("Rotate Tool"); ImGui::NextColumn();
            ImGui::Text("R"); ImGui::NextColumn(); ImGui::Text("Scale Tool"); ImGui::NextColumn();
            ImGui::Text("W"); ImGui::NextColumn(); ImGui::Text("Wireframe Toggle"); ImGui::NextColumn();
            ImGui::Text("X"); ImGui::NextColumn(); ImGui::Text("X-Ray Toggle"); ImGui::NextColumn();
            ImGui::Text("F"); ImGui::NextColumn(); ImGui::Text("Focus Selected"); ImGui::NextColumn();
            ImGui::Text("H"); ImGui::NextColumn(); ImGui::Text("Console Toggle"); ImGui::NextColumn();
            ImGui::Text("Del"); ImGui::NextColumn(); ImGui::Text("Delete Selected"); ImGui::NextColumn();
            ImGui::Text("`"); ImGui::NextColumn(); ImGui::Text("Grid Toggle"); ImGui::NextColumn();
            ImGui::Separator(); ImGui::NextColumn(); ImGui::NextColumn();
            ImGui::Text("Ctrl+Z"); ImGui::NextColumn(); ImGui::Text("Undo"); ImGui::NextColumn();
            ImGui::Text("Ctrl+Y"); ImGui::NextColumn(); ImGui::Text("Redo"); ImGui::NextColumn();
            ImGui::Text("Ctrl+S"); ImGui::NextColumn(); ImGui::Text("Save"); ImGui::NextColumn();
            ImGui::Text("Ctrl+O"); ImGui::NextColumn(); ImGui::Text("Open"); ImGui::NextColumn();
            ImGui::Text("Ctrl+D"); ImGui::NextColumn(); ImGui::Text("Duplicate"); ImGui::NextColumn();
            ImGui::Text("Ctrl+C"); ImGui::NextColumn(); ImGui::Text("Copy"); ImGui::NextColumn();
            ImGui::Text("Ctrl+V"); ImGui::NextColumn(); ImGui::Text("Paste"); ImGui::NextColumn();
            ImGui::Text("Ctrl+G"); ImGui::NextColumn(); ImGui::Text("Group"); ImGui::NextColumn();
            ImGui::Text("Ctrl+A"); ImGui::NextColumn(); ImGui::Text("Select All"); ImGui::NextColumn();
            ImGui::Text("Shift+D"); ImGui::NextColumn(); ImGui::Text("Duplicate"); ImGui::NextColumn();
            ImGui::Separator(); ImGui::NextColumn(); ImGui::NextColumn();
            ImGui::Text("Numpad 1"); ImGui::NextColumn(); ImGui::Text("Front View"); ImGui::NextColumn();
            ImGui::Text("Numpad 3"); ImGui::NextColumn(); ImGui::Text("Right View"); ImGui::NextColumn();
            ImGui::Text("Numpad 7"); ImGui::NextColumn(); ImGui::Text("Top View"); ImGui::NextColumn();
            ImGui::Text("Numpad 5"); ImGui::NextColumn(); ImGui::Text("Persp/Ortho Toggle"); ImGui::NextColumn();
            ImGui::Text("Numpad 0"); ImGui::NextColumn(); ImGui::Text("Camera View"); ImGui::NextColumn();
            ImGui::Separator(); ImGui::NextColumn(); ImGui::NextColumn();
            ImGui::Text("Tab"); ImGui::NextColumn(); ImGui::Text("Edit Mode Toggle"); ImGui::NextColumn();
            ImGui::Text("Shift+Tab"); ImGui::NextColumn(); ImGui::Text("Sculpt Mode Toggle"); ImGui::NextColumn();
            ImGui::Separator(); ImGui::NextColumn(); ImGui::NextColumn();
            ImGui::Text("Alt+Left"); ImGui::NextColumn(); ImGui::Text("Orbit"); ImGui::NextColumn();
            ImGui::Text("Shift+Mid"); ImGui::NextColumn(); ImGui::Text("Pan"); ImGui::NextColumn();
            ImGui::Text("Ctrl+Mid"); ImGui::NextColumn(); ImGui::Text("Zoom"); ImGui::NextColumn();
            ImGui::Columns(1);
        }

        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) show_about = false;
    }
    ImGui::End();
}

static void draw_edit_panel() {
    if (current_mode != MODE_EDIT || !edit_node) return;

    ImGui::SetNextWindowPos(ImVec2(0, 54), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)LEFT_PANEL_W, (float)WIN_H * 0.5f - 27), ImGuiCond_Always);
    ImGui::Begin("Edit Mode", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::Text("Editing: %s", edit_node->name.c_str());
    ImGui::Separator();

    ImGui::Text("Select Mode");
    if (ImGui::RadioButton("Vertex##sel", edit_select_mode == 0)) edit_select_mode = 0;
    if (ImGui::RadioButton("Edge##sel", edit_select_mode == 1)) edit_select_mode = 1;
    if (ImGui::RadioButton("Face##sel", edit_select_mode == 2)) edit_select_mode = 2;

    ImGui::Separator();
    ImGui::Text("Tools");

    auto tool_btn = [](const char* label, const char* tip, bool active) {
        bool r = ImGui::RadioButton(label, active);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
        return r;
    };

    if (tool_btn("Extrude", "Extrude selected", edit_tool == 0)) edit_tool = 0;
    if (tool_btn("Inset", "Inset faces", edit_tool == 1)) edit_tool = 1;
    if (tool_btn("Bevel", "Bevel edges/verts", edit_tool == 2)) edit_tool = 2;
    if (tool_btn("Merge", "Merge at center", edit_tool == 7)) edit_tool = 7;
    if (tool_btn("Solidify", "Solidify mesh", edit_tool == 12)) edit_tool = 12;
    if (tool_btn("Mirror", "Mirror mesh", edit_tool == 13)) edit_tool = 13;

    ImGui::Separator();
    ImGui::Text("Operations");

    auto it = edit_bmeshes.find(edit_node);
    if (it != edit_bmeshes.end()) {
        BMesh& bm = it->second;

        int sel_v = 0, sel_e = 0, sel_f = 0;
        for (int i = 0; i < bm.vert_count; ++i) if (bm.verts[i]->select) sel_v++;
        for (int i = 0; i < bm.edge_count; ++i) if (bm.edges[i]->select) sel_e++;
        for (int i = 0; i < bm.face_count; ++i) if (bm.faces[i]->select) sel_f++;
        ImGui::Text("V:%d E:%d F:%d", sel_v, sel_e, sel_f);

        if (ImGui::Button("Apply Tool##edit")) {
            const char* tool_names[] = {"Extrude","Inset","Bevel","","","","","Merge","","","","","Solidify","Mirror"};
            std::string desc = "Edit: ";
            if (edit_tool >= 0 && edit_tool <= 13 && tool_names[edit_tool]) desc += tool_names[edit_tool];
            else desc += "Tool";
            save_undo(desc);
            switch (edit_tool) {
                case 0: edit_tools::extrude_region(bm, vec3(0, 0.2f, 0)); break;
                case 1: edit_tools::inset_faces(bm, 0.05f); break;
                case 2: edit_tools::bevel_edges(bm, 0.05f); break;
                case 7: edit_tools::merge_at_center(bm); break;
                case 12: edit_tools::solidify(bm, 0.1f); break;
                case 13: edit_tools::mirror(bm, 0); break;
            }
            sync_bmesh_to_mesh(edit_node);
            log_message("Applied edit tool");
        }

        ImGui::Separator();
        if (ImGui::Button("Subdivide")) {
            save_undo("Subdivide");
            edit_tools::subdivide(bm, 1);
            sync_bmesh_to_mesh(edit_node);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Subdivide selected faces");
        ImGui::SameLine();
        if (ImGui::Button("Smooth")) {
            save_undo("Smooth");
            edit_tools::smooth(bm, 0.5f, 1);
            sync_bmesh_to_mesh(edit_node);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Smooth vertex positions");
        if (ImGui::Button("Triangulate")) {
            save_undo("Triangulate");
            edit_tools::triangulate(bm);
            sync_bmesh_to_mesh(edit_node);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Convert all faces to triangles");
        ImGui::SameLine();
        if (ImGui::Button("Recalc N")) {
            save_undo("Recalc Normals");
            edit_tools::recalculate_normals(bm);
            sync_bmesh_to_mesh(edit_node);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Recalculate face normals");
        if (ImGui::Button("Select All")) { bm.select_all(true); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select all geometry");
        ImGui::SameLine();
        if (ImGui::Button("Deselect All")) { bm.select_all(false); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Deselect everything");
    }

    ImGui::Separator();
    if (ImGui::Button("Exit Edit Mode", ImVec2(-1, 0))) exit_edit_mode();

    ImGui::End();
}

static void draw_sculpt_panel() {
    if (current_mode != MODE_SCULPT || !edit_node) return;

    ImGui::SetNextWindowPos(ImVec2(0, 54), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)LEFT_PANEL_W, (float)WIN_H * 0.5f - 27), ImGuiCond_Always);
    ImGui::Begin("Sculpt Mode", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::Text("Sculpting: %s", edit_node->name.c_str());
    ImGui::Separator();

    ImGui::Text("Brush");
    const char* brush_names[] = {"Draw", "Clay", "Inflate", "Smooth", "Flatten", "Grab", "Crease", "Pinch", "Mask", "Smooth Mask"};
    int bt = (int)brush_settings.type;
    if (ImGui::Combo("Type", &bt, brush_names, 10))
        brush_settings.type = (sculpt::BrushType)bt;

    ImGui::SliderFloat("Radius", &brush_settings.radius, 0.01f, 5.0f);
    ImGui::SliderFloat("Strength", &brush_settings.strength, 0.01f, 2.0f);
    ImGui::SliderFloat("Focal", &brush_settings.focal, 0.0f, 1.0f);

    ImGui::Separator();
    ImGui::Text("Symmetry");
    ImGui::Checkbox("X##sym", &brush_settings.use_symmetry_x);
    ImGui::SameLine();
    ImGui::Checkbox("Y##sym", &brush_settings.use_symmetry_y);
    ImGui::SameLine();
    ImGui::Checkbox("Z##sym", &brush_settings.use_symmetry_z);

    ImGui::Separator();
    if (edit_node) {
        auto it = edit_bmeshes.find(edit_node);
        if (it != edit_bmeshes.end()) {
            ImGui::Text("Vertices: %d", it->second.vert_count);
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Exit Sculpt Mode", ImVec2(-1, 0))) exit_sculpt_mode();

    ImGui::End();
}

static void draw_status_bar() {
    ImGui::SetNextWindowPos(ImVec2(0, (float)WIN_H - BOTTOM_OFFSET), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)WIN_W, (float)BOTTOM_OFFSET), ImGuiCond_Always);
    ImGui::Begin("##StatusBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::Text("%s", status_text.c_str());

    ImGui::SameLine(300);
    ImGui::Text("FPS: %.0f", current_fps);

    const char* tool_names[] = {"Move", "Rotate", "Scale"};
    const char* mode_names[] = {"Object", "Edit", "Sculpt"};
    ImGui::SameLine(400);
    ImGui::Text("| %s | %s", mode_names[current_mode], tool_names[selected_tool]);

    ImGui::SameLine(500);
    ImGui::Text("| Wire:%s XRay:%s",
        scene.global_wireframe ? "ON" : "OFF",
        scene.xray_mode ? "ON" : "OFF");

    int total_verts = 0, total_tris = 0;
    for (auto* n : scene.get_all_nodes()) {
        if (n->mesh) {
            total_verts += n->mesh->get_vertex_count();
            total_tris += n->mesh->get_triangle_count();
        }
    }

    float right_start = (float)WIN_W - 400.0f;
    ImGui::SameLine(right_start);
    ImGui::Text("Nodes: %d | Verts: %d | Tris: %d",
        scene.get_node_count(), total_verts, total_tris);

    if (scene.selected_node) {
        ImGui::SameLine(right_start + 320);
        ImGui::Text("| %s", scene.selected_node->name.c_str());
    }

    ImGui::End();
}

int main() {
    if (!glfwInit()) { fprintf(stderr, "GLFW init failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(WIN_W, WIN_H, APP_TITLE, nullptr, nullptr);
    if (!window) { fprintf(stderr, "Window creation failed\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int version = gladLoadGL(glfwGetProcAddress);
    if (!version) { fprintf(stderr, "GLAD init failed\n"); return 1; }
    printf("OpenGL %d.%d loaded\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.12f, 0.95f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.2f, 0.2f, 0.28f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.32f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.45f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.22f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.4f, 0.6f, 0.9f, 1.0f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    renderer.initialize(WIN_W, WIN_H);
    setup_default_scene();
    discover_resources();
    log_message("OCP v3.0 initialized - KitariosWebStudio - KWS");

    last_frame_time = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double now = glfwGetTime();
        double dt = now - last_frame_time;
        last_frame_time = now;
        frame_counter++;
        fps_update_timer += dt;
        if (fps_update_timer >= 0.5) {
            current_fps = (float)frame_counter / (float)fps_update_timer;
            frame_counter = 0;
            fps_update_timer = 0.0;
        }

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        // Roblox-style WASD camera movement (while right-click held)
        if (right_held_for_orbit) {
            float move_speed = 5.0f * (float)dt;
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
                move_speed *= 3.0f;
            }
            float fwd = 0, right = 0, up = 0;
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) fwd += 1.0f;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) fwd -= 1.0f;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) right -= 1.0f;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) right += 1.0f;
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) up += 1.0f;
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) up -= 1.0f;
            if (fwd != 0 || right != 0 || up != 0) {
                camera.move(fwd, right, up, move_speed);
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        viewport_hovered = (mx > LEFT_PANEL_W && mx < WIN_W - RIGHT_PANEL_W && my > TOP_OFFSET && my < WIN_H - BOTTOM_OFFSET);

        draw_menu_bar();
        draw_toolbar();
        draw_scene_tree();
        draw_properties();
        draw_ai_panel();
        draw_light_panel();
        draw_console();
        draw_settings();
        draw_about();
        draw_resource_browser();
        draw_edit_panel();
        draw_sculpt_panel();
        draw_status_bar();

        scene.update();

        ImGui::Render();

        int vp_x = 0, vp_y = TOP_OFFSET;
        int vp_w = WIN_W - RIGHT_PANEL_W, vp_h = WIN_H - TOP_OFFSET - BOTTOM_OFFSET;
        renderer.render_scene(scene, camera, vp_x, vp_y, vp_w, vp_h);

        if (scene.selected_node) {
            renderer.render_gizmo(scene.selected_node, selected_tool, camera.get_view_matrix(), camera.get_projection_matrix(), camera);
        }

        if (current_mode == MODE_EDIT && edit_node) {
            auto it = edit_bmeshes.find(edit_node);
            if (it != edit_bmeshes.end()) {
                renderer.render_edit_overlays(it->second, edit_node->get_world_matrix(),
                    camera.get_view_matrix(), camera.get_projection_matrix(), edit_select_mode, camera);
            }
        }

        if (current_mode == MODE_SCULPT && sculpt_hit_valid) {
            renderer.render_sculpt_cursor(sculpt_hit_pos, brush_settings.radius,
                camera.get_view_matrix(), camera.get_projection_matrix());
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    renderer.cleanup();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
