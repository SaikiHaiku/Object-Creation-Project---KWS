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

#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>
#include <stack>
#include <deque>

using namespace ocp;

static const char* APP_TITLE = "OCP - Object Creation Project | KitariosWebStudio - KWS";
static int WIN_W = 1600, WIN_H = 900;

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
static char scene_search[128] = "";
static char rename_buf[128] = "";
static bool renaming = false;
static float snap_value = 0.0f;
static std::deque<std::string> console_log;
static const int MAX_LOG = 200;

static bool mouse_orbiting = false, mouse_panning = false;
static double last_mx = 0, last_my = 0;
static bool viewport_hovered = false;

struct UndoState {
    std::string data;
    std::string description;
};
static std::vector<UndoState> undo_stack;
static std::vector<UndoState> redo_stack;
static const int MAX_UNDO = 50;

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

    status_text = "Default scene loaded";
    log_message("Default scene loaded");
}

static void setup_key_callbacks(GLFWwindow* window);

static void mouse_button_callback(GLFWwindow* w, int button, int action, int) {
    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_MIDDLE) mouse_orbiting = true;
        else if (button == GLFW_MOUSE_BUTTON_RIGHT) mouse_panning = true;
        else if (button == GLFW_MOUSE_BUTTON_LEFT && viewport_hovered) {
            double mx, my; glfwGetCursorPos(w, &mx, &my);
            float vx = (float)mx, vy = (float)my;
            vec3 origin, dir;
            camera.screen_to_ray(vx, vy, (float)WIN_W, (float)WIN_H, origin, dir);
            SceneNode* hit; float dist;
            if (scene.raycast(origin, dir, hit, dist)) {
                scene.select(hit);
                status_text = "Selected: " + hit->name;
                strncpy(rename_buf, hit->name.c_str(), sizeof(rename_buf));
                log_message("Selected: " + hit->name);
            }
            else scene.deselect();
        }
    } else {
        if (button == GLFW_MOUSE_BUTTON_MIDDLE) mouse_orbiting = false;
        else if (button == GLFW_MOUSE_BUTTON_RIGHT) mouse_panning = false;
    }
}

static void cursor_position_callback(GLFWwindow*, double mx, double my) {
    float dx = (float)(mx - last_mx), dy = (float)(my - last_my);
    if (mouse_orbiting) camera.orbit(dx * 0.5f, dy * 0.5f);
    else if (mouse_panning) camera.pan(dx, dy);
    last_mx = mx; last_my = my;
}

static void scroll_callback(GLFWwindow*, double, double yoff) {
    camera.zoom((float)yoff * 2.0f);
}

static void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    WIN_W = w; WIN_H = h;
    camera.aspect = (float)w / (float)h;
    renderer.resize(w, h);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;
    bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;

    if (key == GLFW_KEY_DELETE) { scene.delete_selected(); status_text = "Deleted"; log_message("Deleted selected"); }
    else if (ctrl && key == GLFW_KEY_Z) undo();
    else if (ctrl && key == GLFW_KEY_Y) redo();
    else if (ctrl && key == GLFW_KEY_D) { scene.duplicate_selected(); status_text = "Duplicated"; log_message("Duplicated"); }
    else if (ctrl && key == GLFW_KEY_N) setup_default_scene();
    else if (ctrl && key == GLFW_KEY_S) {
        auto p = save_file_dialog("OCP Scene\0*.ocp\0All\0*.*\0");
        if (!p.empty()) {
            std::string data = scene.save_to_string();
            FILE* f = fopen(p.c_str(), "w");
            if (f) { fwrite(data.c_str(), 1, data.size(), f); fclose(f); status_text = "Saved: " + p; log_message("Saved: " + p); }
        }
    }
    else if (ctrl && key == GLFW_KEY_O) {
        auto p = open_file_dialog("OCP Scene\0*.ocp\0All\0*.*\0");
        if (!p.empty()) {
            FILE* f = fopen(p.c_str(), "r");
            if (f) {
                fseek(f, 0, SEEK_END);
                long sz = ftell(f);
                fseek(f, 0, SEEK_SET);
                std::string data(sz, 0);
                fread(&data[0], 1, sz, f);
                fclose(f);
                scene.load_from_string(data);
                status_text = "Loaded: " + p;
                log_message("Loaded: " + p);
            }
        }
    }
    else if (ctrl && key == GLFW_KEY_A) { auto ns = scene.get_all_nodes(); if (!ns.empty()) scene.select(ns[0]); }
    else if (ctrl && key == GLFW_KEY_1) { selected_tool = 0; status_text = "Move tool"; }
    else if (ctrl && key == GLFW_KEY_2) { selected_tool = 1; status_text = "Rotate tool"; }
    else if (ctrl && key == GLFW_KEY_3) { selected_tool = 2; status_text = "Scale tool"; }
    else if (key == GLFW_KEY_F && scene.selected_node) {
        vec3 c = (scene.selected_node->get_bounding_box_min() + scene.selected_node->get_bounding_box_max()) * 0.5f;
        float s = glm::length(scene.selected_node->get_bounding_box_max() - scene.selected_node->get_bounding_box_min());
        camera.focus_on(c, s);
    }
    else if (key == GLFW_KEY_G) scene.grid_enabled = !scene.grid_enabled;
    else if (key == GLFW_KEY_H) show_console = !show_console;
    else if (key == GLFW_KEY_F1) show_about = true;
}

static void draw_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) setup_default_scene();
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                auto p = save_file_dialog("OCP Scene\0*.ocp\0All\0*.*\0");
                if (!p.empty()) {
                    std::string data = scene.save_to_string();
                    FILE* f = fopen(p.c_str(), "w");
                    if (f) { fwrite(data.c_str(), 1, data.size(), f); fclose(f); status_text = "Saved: " + p; }
                }
            }
            if (ImGui::MenuItem("Load Scene", "Ctrl+O")) {
                auto p = open_file_dialog("OCP Scene\0*.ocp\0All\0*.*\0");
                if (!p.empty()) {
                    FILE* f = fopen(p.c_str(), "r");
                    if (f) {
                        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
                        std::string data(sz, 0); fread(&data[0], 1, sz, f); fclose(f);
                        scene.load_from_string(data);
                        status_text = "Loaded: " + p;
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import OBJ")) {
                auto p = open_file_dialog("OBJ Files\0*.obj\0All\0*.*\0");
                if (!p.empty()) { status_text = "Import: " + p; log_message("Import requested: " + p); }
            }
            if (ImGui::MenuItem("Export OBJ")) {
                auto p = save_file_dialog("OBJ Files\0*.obj\0All\0*.*\0");
                if (!p.empty()) { OBJExporter::export_to_file(p, scene); status_text = "Exported: " + p; }
            }
            if (ImGui::MenuItem("Export STL")) {
                auto p = save_file_dialog("STL Files\0*.stl\0All\0*.*\0");
                if (!p.empty()) { STLExporter::export_to_file(p, scene); status_text = "Exported: " + p; }
            }
            if (ImGui::MenuItem("Export PNG")) {
                auto p = save_file_dialog("PNG Files\0*.png\0All\0*.*\0");
                if (!p.empty()) { PNGExporter::export_framebuffer(p, WIN_W, WIN_H); status_text = "Screenshot: " + p; }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(glfwGetCurrentContext(), true);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) scene.duplicate_selected();
            if (ImGui::MenuItem("Delete", "Del")) { save_undo("Delete"); scene.delete_selected(); status_text = "Deleted"; }
            if (ImGui::MenuItem("Select All", "Ctrl+A")) { auto ns = scene.get_all_nodes(); if (!ns.empty()) scene.select(ns[0]); }
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
            if (ImGui::MenuItem("Toggle Grid", "G")) scene.grid_enabled = !scene.grid_enabled;
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
    ImGui::SameLine(10);
    if (ImGui::Button("Cube")) add_primitive("cube");
    ImGui::SameLine();
    if (ImGui::Button("Sphere")) add_primitive("sphere");
    ImGui::SameLine();
    if (ImGui::Button("Cylinder")) add_primitive("cylinder");
    ImGui::SameLine();
    if (ImGui::Button("Cone")) add_primitive("cone");
    ImGui::SameLine();
    if (ImGui::Button("Torus")) add_primitive("torus");
    ImGui::SameLine();
    if (ImGui::Button("Plane")) add_primitive("plane");
    ImGui::SameLine(250);
    ImGui::Text("|");
    ImGui::SameLine();
    if (ImGui::Button("Front")) camera.set_view("front");
    ImGui::SameLine();
    if (ImGui::Button("Top")) camera.set_view("top");
    ImGui::SameLine();
    if (ImGui::Button("Right")) camera.set_view("right");
    ImGui::SameLine();
    if (ImGui::Button("3D")) camera.set_view("front_right");
    ImGui::SameLine(450);
    ImGui::Text("|");
    ImGui::SameLine();
    if (ImGui::RadioButton("Move", selected_tool == 0)) selected_tool = 0;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", selected_tool == 1)) selected_tool = 1;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", selected_tool == 2)) selected_tool = 2;
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::DragFloat("Snap", &snap_value, 0.05f, 0.0f, 10.0f);
    ImGui::PopItemWidth();
    ImGui::End();
}

static void draw_scene_tree() {
    ImGui::SetNextWindowPos(ImVec2(0, 54), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(220, (float)WIN_H - 54), ImGuiCond_Always);
    ImGui::Begin("Scene Hierarchy", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::InputTextWithHint("##search", "Search...", scene_search, sizeof(scene_search));
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
        if (!node->visible) flags |= ImGuiTreeNodeFlags_DefaultOpen;
        std::string label = node->mesh ? "[M] " : "[N] ";
        label += node->name;
        ImGui::TreeNodeEx((void*)(uintptr_t)node, flags, "%s", label.c_str());
        if (ImGui::IsItemClicked()) {
            scene.select(node);
            strncpy(rename_buf, node->name.c_str(), sizeof(rename_buf));
        }
        if (ImGui::IsItemClicked(1)) ImGui::OpenPopup("node_menu");
    }
    if (ImGui::BeginPopup("node_menu")) {
        if (ImGui::MenuItem("Rename")) renaming = true;
        if (ImGui::MenuItem("Toggle Visibility") && scene.selected_node) {
            save_undo("Toggle visibility");
            scene.selected_node->visible = !scene.selected_node->visible;
        }
        if (ImGui::MenuItem("Lock/Unlock") && scene.selected_node) {
            scene.selected_node->locked = !scene.selected_node->locked;
        }
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) scene.duplicate_selected();
        if (ImGui::MenuItem("Delete", "Del")) { save_undo("Delete"); scene.delete_selected(); }
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
    ImGui::SetNextWindowPos(ImVec2((float)WIN_W - 350, 54), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(350, (float)WIN_H * 0.5f - 27), ImGuiCond_Always);
    ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    SceneNode* node = scene.selected_node;
    if (!node) { ImGui::TextDisabled("No selection"); ImGui::End(); return; }

    char name_buf[128];
    strncpy(name_buf, node->name.c_str(), sizeof(name_buf));
    if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
        node->name = name_buf;
    }

    ImGui::Separator();
    ImGui::Text("Transform");
    vec3 old_pos = node->position;
    vec3 old_rot = node->rotation;
    vec3 old_scl = node->scale;
    ImGui::DragFloat3("Position", &node->position.x, 0.1f);
    ImGui::DragFloat3("Rotation", &node->rotation.x, 1.0f);
    ImGui::DragFloat3("Scale", &node->scale.x, 0.05f, 0.01f, 100.0f);

    ImGui::Checkbox("Visible", &node->visible);
    ImGui::SameLine();
    ImGui::Checkbox("Locked", &node->locked);

    if (node->material) {
        ImGui::Separator();
        ImGui::Text("Material");
        ImGui::InputText("Mat Name", (char*)node->material->name.c_str(), node->material->name.size());
        vec4& d = node->material->diffuse;
        float col[4] = {d.r, d.g, d.b, d.a};
        if (ImGui::ColorEdit4("Diffuse", col)) { d = vec4(col[0], col[1], col[2], col[3]); node->material->opacity = col[3]; }
        vec4& sp = node->material->specular;
        float scol[4] = {sp.r, sp.g, sp.b, sp.a};
        if (ImGui::ColorEdit4("Specular", scol)) sp = vec4(scol[0], scol[1], scol[2], scol[3]);
        vec4& em = node->material->emission;
        float ecol[4] = {em.r, em.g, em.b, em.a};
        if (ImGui::ColorEdit4("Emission", ecol)) em = vec4(ecol[0], ecol[1], ecol[2], ecol[3]);
        ImGui::SliderFloat("Metallic", &node->material->metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &node->material->roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Shininess", &node->material->shininess, 1.0f, 200.0f);
        ImGui::SliderFloat("Opacity", &node->material->opacity, 0.0f, 1.0f);
        ImGui::Checkbox("Wireframe", &node->material->wireframe);
        ImGui::SameLine();
        ImGui::Checkbox("Backface Cull", &node->material->backface_culling);
    }

    if (node->mesh) {
        ImGui::Separator();
        ImGui::Text("Mesh Info");
        ImGui::Text("Vertices: %d", node->mesh->get_vertex_count());
        ImGui::Text("Triangles: %d", node->mesh->get_triangle_count());
        ImGui::Text("Faces: %d", (int)node->mesh->faces.size());
        if (ImGui::Button("Subdivide")) { save_undo("Subdivide"); node->mesh->subdivide(); renderer.invalidate_mesh(node->mesh.get()); }
        ImGui::SameLine();
        if (ImGui::Button("Invert Faces")) { save_undo("Invert"); node->mesh->invert_faces(); renderer.invalidate_mesh(node->mesh.get()); }
        ImGui::SameLine();
        if (ImGui::Button("Weld Vertices")) { save_undo("Weld"); node->mesh->weld_vertices(); renderer.invalidate_mesh(node->mesh.get()); }
    }
    ImGui::End();
}

static void draw_ai_panel() {
    ImGui::SetNextWindowPos(ImVec2((float)WIN_W - 350, (float)WIN_H * 0.5f + 27), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(350, (float)WIN_H * 0.5f - 27), ImGuiCond_Always);
    ImGui::Begin("AI Object Generator", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::TextWrapped("Describe what to create (FR/EN):");
    ImGui::InputTextMultiline("##prompt", ai_prompt, sizeof(ai_prompt), ImVec2(-1, 80));
    if (ImGui::Button("Generate", ImVec2(-1, 30))) {
        save_undo("AI Generate");
        Scene* result = ai_generator.generate_from_prompt(ai_prompt, &scene);
        last_ai_result = ai_generator.get_last_description();
        status_text = "AI Generated: " + last_ai_result;
        log_message("AI: " + last_ai_result);
    }
    ImGui::Separator();
    ImGui::Text("Quick Examples:");
    if (ImGui::Button("House", ImVec2(-1, 0))) { strcpy(ai_prompt, "house"); }
    if (ImGui::Button("Robot", ImVec2(-1, 0))) { strcpy(ai_prompt, "robot"); }
    if (ImGui::Button("Castle", ImVec2(-1, 0))) { strcpy(ai_prompt, "castle"); }
    if (ImGui::Button("Spaceship", ImVec2(-1, 0))) { strcpy(ai_prompt, "spaceship"); }
    if (ImGui::Button("Tree", ImVec2(-1, 0))) { strcpy(ai_prompt, "tree"); }
    if (ImGui::Button("Sword", ImVec2(-1, 0))) { strcpy(ai_prompt, "sword"); }
    if (ImGui::Button("Snowman", ImVec2(-1, 0))) { strcpy(ai_prompt, "snowman"); }
    if (ImGui::Button("DNA", ImVec2(-1, 0))) { strcpy(ai_prompt, "dna"); }
    if (ImGui::Button("Skull", ImVec2(-1, 0))) { strcpy(ai_prompt, "skull"); }
    if (ImGui::Button("Airplane", ImVec2(-1, 0))) { strcpy(ai_prompt, "airplane"); }
    if (ImGui::Button("Flower", ImVec2(-1, 0))) { strcpy(ai_prompt, "flower"); }
    if (ImGui::Button("Mushroom", ImVec2(-1, 0))) { strcpy(ai_prompt, "mushroom"); }
    if (ImGui::Button("Donut", ImVec2(-1, 0))) { strcpy(ai_prompt, "donut pink glaze"); }
    if (ImGui::Button("Neon Spiral x5", ImVec2(-1, 0))) { strcpy(ai_prompt, "neon spiral 5x circle"); }
    if (ImGui::Button("Clear Scene", ImVec2(-1, 0))) { setup_default_scene(); }
    if (!last_ai_result.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("Last: %s", last_ai_result.c_str());
    }
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
    ImGui::SetNextWindowSize(ImVec2(450, 300), ImGuiCond_Always);
    if (ImGui::Begin("About OCP", &show_about, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("OCP - Object Creation Project");
        ImGui::Separator();
        ImGui::Text("Version 2.0 (C++)");
        ImGui::Text("Build: %s %s", __DATE__, __TIME__);
        ImGui::Separator();
        ImGui::TextWrapped("Professional 3D object creation tool with AI-powered\nprocedural generation.");
        ImGui::Separator();
        ImGui::TextWrapped("Created by KitariosWebStudio - KWS");
        ImGui::TextWrapped("OpenGL 3.3 Core | Dear ImGui | GLFW | GLM");
        ImGui::Separator();
        ImGui::Text("Features:");
        ImGui::BulletText("Manual primitive creation (9 types)");
        ImGui::BulletText("35+ AI procedural scene generators");
        ImGui::BulletText("Bilingual prompt engine (FR/EN)");
        ImGui::BulletText("Full material editor (PBR)");
        ImGui::BulletText("Light management");
        ImGui::BulletText("OBJ/STL/PNG export");
        ImGui::BulletText("Scene save/load");
        ImGui::BulletText("Undo/Redo system");
        ImGui::BulletText("Keyboard shortcuts");
        ImGui::Separator();
        ImGui::Text("Shortcuts: Ctrl+Z/Y undo/redo, F focus, G grid, H console");
        if (ImGui::Button("OK", ImVec2(120, 0))) show_about = false;
    }
    ImGui::End();
}

static void draw_status_bar() {
    ImGui::SetNextWindowPos(ImVec2(0, (float)WIN_H - 24), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)WIN_W, 24), ImGuiCond_Always);
    ImGui::Begin("##StatusBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::Text("%s", status_text.c_str());
    ImGui::SameLine(WIN_W - 400);
    ImGui::Text("Nodes: %d | Verts: %d | Tris: %d",
        scene.get_node_count(),
        [&]() { int v = 0; for (auto* n : scene.get_all_nodes()) if (n->mesh) v += n->mesh->get_vertex_count(); return v; }(),
        [&]() { int t = 0; for (auto* n : scene.get_all_nodes()) if (n->mesh) t += n->mesh->get_triangle_count(); return t; }());
    ImGui::SameLine(WIN_W - 150);
    ImGui::Text("OpenGL 3.3");
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
    log_message("OCP v2.0 initialized - KitariosWebStudio - KWS");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        viewport_hovered = (mx > 220 && mx < WIN_W - 350 && my > 54 && my < WIN_H - 24);

        draw_menu_bar();
        draw_toolbar();
        draw_scene_tree();
        draw_properties();
        draw_ai_panel();
        draw_light_panel();
        draw_console();
        draw_settings();
        draw_about();
        draw_status_bar();

        scene.update();

        ImGui::Render();

        int vp_x = 0, vp_y = 54;
        int vp_w = WIN_W - 350, vp_h = WIN_H - 54 - 24;
        renderer.render_scene(scene, camera, vp_x, vp_y, vp_w, vp_h);

        if (scene.selected_node) {
            renderer.render_gizmo(scene.selected_node, selected_tool, camera.get_view_matrix(), camera.get_projection_matrix(), camera);
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
