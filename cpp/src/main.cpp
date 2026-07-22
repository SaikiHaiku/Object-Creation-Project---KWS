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

static void mouse_button_callback(GLFWwindow* w, int button, int action, int mods) {
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
                if (mods & GLFW_MOD_CONTROL) {
                    scene.toggle_multi_select(hit);
                    status_text = "Multi-select: " + hit->name + " (" + std::to_string(scene.multi_selection.size()) + " selected)";
                } else {
                    scene.select(hit);
                    status_text = "Selected: " + hit->name;
                }
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
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;

    if (ctrl && key == GLFW_KEY_Z) undo();
    else if (ctrl && key == GLFW_KEY_Y) redo();
    else if (ctrl && key == GLFW_KEY_D) { save_undo("Duplicate"); scene.duplicate_selected(); status_text = "Duplicated"; log_message("Duplicated"); }
    else if (ctrl && key == GLFW_KEY_C) { scene.copy_selected(); status_text = "Copied"; log_message("Copied to clipboard"); }
    else if (ctrl && key == GLFW_KEY_V) { save_undo("Paste"); scene.paste_clipboard(); status_text = "Pasted"; log_message("Pasted from clipboard"); }
    else if (ctrl && !shift && key == GLFW_KEY_G) { save_undo("Group"); scene.group_selected(); status_text = "Grouped"; log_message("Grouped selected nodes"); }
    else if (ctrl && shift && key == GLFW_KEY_G) { save_undo("Ungroup"); scene.ungroup_selected(); status_text = "Ungrouped"; log_message("Ungrouped"); }
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
    else if (ctrl && key == GLFW_KEY_A) {
        auto ns = scene.get_all_nodes();
        scene.multi_selection.clear();
        for (auto* n : ns) scene.multi_selection.push_back(n);
        if (!ns.empty()) scene.selected_node = ns[0];
    }
    else if (key == GLFW_KEY_DELETE) { save_undo("Delete"); scene.delete_selected(); status_text = "Deleted"; log_message("Deleted selected"); }
    else if (key == GLFW_KEY_G && !ctrl) { selected_tool = 0; status_text = "Tool: Move (G)"; log_message("Tool: Move"); }
    else if (key == GLFW_KEY_E && !ctrl) { selected_tool = 1; status_text = "Tool: Rotate (E)"; log_message("Tool: Rotate"); }
    else if (key == GLFW_KEY_R && !ctrl) { selected_tool = 2; status_text = "Tool: Scale (R)"; log_message("Tool: Scale"); }
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
    else if (ctrl && key == GLFW_KEY_1) { selected_tool = 0; status_text = "Tool: Move"; }
    else if (ctrl && key == GLFW_KEY_2) { selected_tool = 1; status_text = "Tool: Rotate"; }
    else if (ctrl && key == GLFW_KEY_3) { selected_tool = 2; status_text = "Tool: Scale"; }
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
            if (ImGui::MenuItem("Resource Library", "R")) show_resource_browser = !show_resource_browser;
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
    if (ImGui::RadioButton("Move[G]", selected_tool == 0)) selected_tool = 0;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate[E]", selected_tool == 1)) selected_tool = 1;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale[R]", selected_tool == 2)) selected_tool = 2;
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::DragFloat("Snap", &snap_value, 0.05f, 0.0f, 10.0f);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    if (ImGui::Button(scene.global_wireframe ? "Wire[ON]" : "Wire[W]")) { scene.global_wireframe = !scene.global_wireframe; }
    ImGui::SameLine();
    if (ImGui::Button(scene.xray_mode ? "XRay[ON]" : "XRay[X]")) { scene.xray_mode = !scene.xray_mode; }
    ImGui::SameLine();
    if (ImGui::Button(scene.grid_enabled ? "Grid[ON]" : "Grid[OFF]")) { scene.grid_enabled = !scene.grid_enabled; }
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
        bool in_multi = false;
        for (auto* ms : scene.multi_selection) { if (ms == node) { in_multi = true; break; } }
        if (in_multi && scene.selected_node != node) flags |= ImGuiTreeNodeFlags_Selected;
        if (!node->visible) flags |= ImGuiTreeNodeFlags_DefaultOpen;
        std::string label = node->mesh ? "[M] " : "[N] ";
        label += node->name;
        if (node->children.size() > 0) {
            label = "[G] " + node->name;
        }
        ImGui::TreeNodeEx((void*)(uintptr_t)node, flags, "%s", label.c_str());
        if (ImGui::IsItemClicked()) {
            if (ImGui::GetIO().KeyCtrl) {
                scene.toggle_multi_select(node);
            } else {
                scene.select(node);
            }
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
    ImGui::SetNextWindowSize(ImVec2(450, 300), ImGuiCond_Always);
    if (ImGui::Begin("About OCP", &show_about, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("OCP - Object Creation Project");
        ImGui::Separator();
        ImGui::Text("Version 2.2 (C++)");
        ImGui::Text("Build: %s %s", __DATE__, __TIME__);
        ImGui::Separator();
        ImGui::TextWrapped("Professional 3D object creation tool with AI-powered\nprocedural generation & organic deformation.");
        ImGui::Separator();
        ImGui::TextWrapped("Created by KitariosWebStudio - KWS");
        ImGui::TextWrapped("OpenGL 3.3 Core | Dear ImGui | GLFW | GLM");
        ImGui::Separator();
        ImGui::Text("Features:");
        ImGui::BulletText("Manual primitive creation (9 types)");
        ImGui::BulletText("60+ AI procedural generators with organic deformation");
        ImGui::BulletText("Bilingual prompt engine (FR/EN)");
        ImGui::BulletText("Full material editor (PBR)");
        ImGui::BulletText("Light management");
        ImGui::BulletText("OBJ/STL/PNG export");
        ImGui::BulletText("Scene save/load");
        ImGui::BulletText("Undo/Redo (50 levels)");
        ImGui::BulletText("Multi-selection, Copy/Paste, Group/Ungroup");
        ImGui::BulletText("Wireframe & X-Ray modes");
        ImGui::BulletText("Blender-style QWERTY tools");
        ImGui::Separator();
        ImGui::Text("Tools: G=Move, E=Rotate, R=Scale, W=Wire, X=Xray");
        ImGui::Text("Actions: Ctrl+C/V copy/paste, Ctrl+G group");
        if (ImGui::Button("OK", ImVec2(120, 0))) show_about = false;
    }
    ImGui::End();
}

static void draw_status_bar() {
    ImGui::SetNextWindowPos(ImVec2(0, (float)WIN_H - 24), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)WIN_W, 24), ImGuiCond_Always);
    ImGui::Begin("##StatusBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::Text("%s", status_text.c_str());
    ImGui::SameLine(300);
    const char* tool_names[] = {"Move[G]", "Rotate[E]", "Scale[R]"};
    ImGui::Text("| %s | Wire:%s | XRay:%s",
        tool_names[selected_tool],
        scene.global_wireframe ? "ON" : "OFF",
        scene.xray_mode ? "ON" : "OFF");
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
    discover_resources();
    log_message("OCP v2.2 initialized - KitariosWebStudio - KWS");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

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
        draw_status_bar();

        scene.update();

        ImGui::Render();

        int vp_x = 0, vp_y = TOP_OFFSET;
        int vp_w = WIN_W - RIGHT_PANEL_W, vp_h = WIN_H - TOP_OFFSET - BOTTOM_OFFSET;
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
