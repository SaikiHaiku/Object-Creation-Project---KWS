#include "editor_ui.h"
#include "core/app_state.h"
#include "core/events.h"
#include "geometry/primitives.h"
#include "geometry/bmesh.h"
#include "editor/edit_tools.h"
#include "editor/sculpt_tools.h"

#include "imgui.h"

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <deque>

namespace ocp {

extern void sync_bmesh_to_mesh(SceneNode* node, EditModeState& edit, Renderer& renderer);

static void add_primitive(AppState& app, const std::string& name) {
    auto& creators = get_primitive_creators();
    auto it = creators.find(name);
    if (it == creators.end()) return;
    Mesh m = it->second(1.0f);
    auto node = std::make_unique<SceneNode>();
    node->name = name;
    node->mesh = std::make_unique<Mesh>(m);
    node->material = std::make_unique<Material>();
    node->material->name = name + "_mat";
    app.scene.add_node(std::move(node));
    app.status_text = "Added " + name;
    app.log_message("Added primitive: " + name);
}

// ===========================================================================
// setup_default_scene
// ===========================================================================
void setup_default_scene(AppState& app) {
    {
        Camera cam;
        cam.name = "Main Camera";
        cam.position = vec3(5, 5, 5);
        cam.target = vec3(0);
        app.scene.cameras.push_back(cam);
    }

    {
        Light l;
        l.name = "Sun";
        l.type = Light::DIRECTIONAL;
        l.position = vec3(10, 15, 10);
        l.direction = glm::normalize(vec3(-1, -2, -1));
        l.color = vec4(1.0f, 0.95f, 0.9f, 1.0f);
        l.intensity = 1.2f;
        app.scene.lights.push_back(l);
    }
    {
        Light l;
        l.name = "Fill Light";
        l.type = Light::POINT;
        l.position = vec3(-5, 3, -5);
        l.color = vec4(0.6f, 0.65f, 0.8f, 1.0f);
        l.intensity = 0.5f;
        app.scene.lights.push_back(l);
    }
    {
        Light l;
        l.name = "Rim Light";
        l.type = Light::POINT;
        l.position = vec3(0, 8, -8);
        l.color = vec4(0.8f, 0.85f, 1.0f, 1.0f);
        l.intensity = 0.7f;
        app.scene.lights.push_back(l);
    }

    add_primitive(app, "cube");
    if (app.scene.selected_node) {
        app.scene.selected_node->material->set_color(0.8f, 0.1f, 0.1f, 1.0f);
        app.scene.selected_node->name = "Red Cube";
        app.scene.selected_node->material->name = "Red_Mat";
    }

    add_primitive(app, "sphere");
    if (app.scene.selected_node) {
        app.scene.selected_node->position = vec3(2.0f, 0.5f, 0.0f);
        app.scene.selected_node->material->set_color(0.1f, 0.3f, 0.9f, 1.0f);
        app.scene.selected_node->name = "Blue Sphere";
        app.scene.selected_node->material->name = "Blue_Mat";
    }

    add_primitive(app, "torus");
    if (app.scene.selected_node) {
        app.scene.selected_node->position = vec3(-2.0f, 0.5f, 0.0f);
        app.scene.selected_node->material->set_color(0.85f, 0.65f, 0.13f, 1.0f);
        app.scene.selected_node->material->metallic = 0.8f;
        app.scene.selected_node->material->roughness = 0.2f;
        app.scene.selected_node->name = "Gold Torus";
        app.scene.selected_node->material->name = "Gold_Mat";
    }

    add_primitive(app, "plane");
    if (app.scene.selected_node) {
        app.scene.selected_node->position = vec3(0, -0.01f, 0);
        app.scene.selected_node->scale = vec3(5.0f, 1.0f, 5.0f);
        app.scene.selected_node->material->set_color(0.35f, 0.35f, 0.38f, 1.0f);
        app.scene.selected_node->name = "Ground";
        app.scene.selected_node->material->name = "Ground_Mat";
    }

    app.scene.deselect();
    app.status_text = "Default scene loaded";
    app.log_message("Default scene created with Camera, 3 Lights, Cube, Sphere, Torus, Ground");
}

// ===========================================================================
// UIState::discover_resources
// ===========================================================================
struct ResourceItem {
    std::string name;
    std::string path;
    std::string category;
};

static std::vector<ResourceItem> g_resources;

void UIState::discover_resources() {
    g_resources.clear();

    const char* search_dirs[] = {
        "resources/textures",
        "resources/materials",
        "resources/scenes",
        "resources/models",
        "resources/fonts",
        "resources/brushes"
    };
    const char* categories[] = {
        "textures", "materials", "scenes", "models", "fonts", "brushes"
    };

    for (int i = 0; i < 6; i++) {
        std::error_code ec;
        if (!std::filesystem::exists(search_dirs[i], ec)) continue;
        for (auto& entry : std::filesystem::directory_iterator(search_dirs[i], ec)) {
            if (!entry.is_regular_file()) continue;
            ResourceItem item;
            item.name = entry.path().filename().string();
            item.path = entry.path().string();
            item.category = categories[i];
            g_resources.push_back(std::move(item));
        }
    }
}

// ===========================================================================
// draw_menu_bar
// ===========================================================================
static void draw_menu_bar(AppState& app, EditModeState& edit, UIState& ui) {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
            app.scene.clear();
            setup_default_scene(app);
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            app.status_text = "Scene saved (placeholder)";
            app.log_message("Scene saved (placeholder)");
        }
        if (ImGui::MenuItem("Load", "Ctrl+O")) {
            app.status_text = "Open (placeholder)";
            app.log_message("Open scene (placeholder)");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Import OBJ...")) {
            app.status_text = "Import OBJ (placeholder)";
        }
        if (ImGui::MenuItem("Export OBJ...")) {
            app.status_text = "Export OBJ (placeholder)";
        }
        if (ImGui::MenuItem("Export STL...")) {
            app.status_text = "Export STL (placeholder)";
        }
        if (ImGui::MenuItem("Export PNG...")) {
            app.status_text = "Export PNG (placeholder)";
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            app.log_message("Exit requested");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Enter Edit Mode", "Tab")) {
            if (app.scene.selected_node && app.current_mode == MODE_OBJECT)
                enter_edit_mode(app, edit, app.scene.selected_node);
        }
        if (ImGui::MenuItem("Enter Sculpt Mode")) {
            if (app.scene.selected_node && app.current_mode == MODE_OBJECT)
                enter_sculpt_mode(app, edit, app.scene.selected_node);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Undo", "Ctrl+Z")) { app.status_text = "Undo (use shortcut)"; }
        if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z")) { app.status_text = "Redo (use shortcut)"; }
        ImGui::Separator();
        if (ImGui::MenuItem("Copy", nullptr, false, false)) {}
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, false)) {}
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, false)) {}
        if (ImGui::MenuItem("Delete", "Del", false, false)) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Group", "Ctrl+G", false, false)) {}
        if (ImGui::MenuItem("Ungroup", "Ctrl+Shift+G", false, false)) {}
        if (ImGui::MenuItem("Select All", "Ctrl+A", false, false)) {}
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Front", "1"))  app.camera.set_view("Front");
        if (ImGui::MenuItem("Back"))        app.camera.set_view("Back");
        if (ImGui::MenuItem("Left"))         app.camera.set_view("Left");
        if (ImGui::MenuItem("Right", "2"))  app.camera.set_view("Right");
        if (ImGui::MenuItem("Top", "3"))    app.camera.set_view("Top");
        if (ImGui::MenuItem("Bottom"))       app.camera.set_view("Bottom");
        if (ImGui::MenuItem("Perspective", "0")) app.camera.set_view("Perspective");
        ImGui::Separator();
        if (ImGui::MenuItem("Focus Selection", "F")) {
            if (app.scene.selected_node)
                app.camera.focus_on(app.scene.selected_node->get_world_position(), 2.0f);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Grid", nullptr, app.scene.grid_enabled))
            app.scene.grid_enabled = !app.scene.grid_enabled;
        if (ImGui::MenuItem("Wireframe", "W", app.scene.global_wireframe))
            app.scene.global_wireframe = !app.scene.global_wireframe;
        if (ImGui::MenuItem("X-Ray", "X", app.scene.xray_mode))
            app.scene.xray_mode = !app.scene.xray_mode;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Add")) {
        if (ImGui::MenuItem("Cube"))       add_primitive(app, "cube");
        if (ImGui::MenuItem("Sphere"))     add_primitive(app, "sphere");
        if (ImGui::MenuItem("Cylinder"))   add_primitive(app, "cylinder");
        if (ImGui::MenuItem("Cone"))       add_primitive(app, "cone");
        if (ImGui::MenuItem("Torus"))      add_primitive(app, "torus");
        if (ImGui::MenuItem("Plane"))      add_primitive(app, "plane");
        if (ImGui::MenuItem("TorusKnot"))  add_primitive(app, "torus_knot");
        if (ImGui::MenuItem("IcoSphere"))  add_primitive(app, "ico_sphere");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Windows")) {
        if (ImGui::MenuItem("Console", "`", ui.show_console))
            ui.show_console = !ui.show_console;
        if (ImGui::MenuItem("Lights", nullptr, ui.show_light_panel))
            ui.show_light_panel = !ui.show_light_panel;
        if (ImGui::MenuItem("Settings", nullptr, ui.show_settings))
            ui.show_settings = !ui.show_settings;
        if (ImGui::MenuItem("Resource Library", nullptr, ui.show_resource_browser))
            ui.show_resource_browser = !ui.show_resource_browser;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About", "F1"))
            ui.show_about = !ui.show_about;
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

// ===========================================================================
// draw_toolbar
// ===========================================================================
static void draw_toolbar(AppState& app, Camera& camera) {
    ImGui::SetNextWindowPos(ImVec2(LEFT_PANEL_W, 0));
    ImGui::SetNextWindowSize(ImVec2(
        (float)(ImGui::GetIO().DisplaySize.x - LEFT_PANEL_W - RIGHT_PANEL_W), TOP_OFFSET));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::Begin("##Toolbar", nullptr, flags);

    if (ImGui::Button("Add +", ImVec2(60, 0))) {
        ImGui::OpenPopup("AddPrimitivePopup");
    }
    if (ImGui::BeginPopup("AddPrimitivePopup")) {
        if (ImGui::MenuItem("Cube"))          add_primitive(app, "cube");
        if (ImGui::MenuItem("Sphere"))        add_primitive(app, "sphere");
        if (ImGui::MenuItem("Cylinder"))      add_primitive(app, "cylinder");
        if (ImGui::MenuItem("Cone"))          add_primitive(app, "cone");
        if (ImGui::MenuItem("Torus"))         add_primitive(app, "torus");
        if (ImGui::MenuItem("Plane"))         add_primitive(app, "plane");
        if (ImGui::MenuItem("TorusKnot"))     add_primitive(app, "torus_knot");
        if (ImGui::MenuItem("IcoSphere"))     add_primitive(app, "ico_sphere");
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

    ImGui::RadioButton("G##tool", &app.selected_tool, 0); ImGui::SameLine();
    ImGui::RadioButton("E##tool", &app.selected_tool, 1); ImGui::SameLine();
    ImGui::RadioButton("R##tool", &app.selected_tool, 2); ImGui::SameLine();

    ImGui::TextDisabled("|");
    ImGui::SameLine();

    if (ImGui::Button("Front"))  camera.set_view("Front");  ImGui::SameLine();
    if (ImGui::Button("Top"))    camera.set_view("Top");    ImGui::SameLine();
    if (ImGui::Button("Right"))  camera.set_view("Right");  ImGui::SameLine();
    if (ImGui::Button("3D"))     camera.set_view("Perspective"); ImGui::SameLine();

    ImGui::TextDisabled("|");
    ImGui::SameLine();

    bool wf = app.scene.global_wireframe;
    if (ImGui::Checkbox("Wire##wf", &wf)) {
        app.scene.global_wireframe = wf;
    }
    ImGui::SameLine();

    bool xr = app.scene.xray_mode;
    if (ImGui::Checkbox("XRay##xr", &xr)) {
        app.scene.xray_mode = xr;
    }
    ImGui::SameLine();

    bool gr = app.scene.grid_enabled;
    if (ImGui::Checkbox("Grid##gr", &gr)) {
        app.scene.grid_enabled = gr;
    }
    ImGui::SameLine();

    ImGui::TextDisabled("|");
    ImGui::SameLine();

    ImGui::Text("Snap:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    ImGui::DragFloat("##snap", &app.snap_value, 0.05f, 0.0f, 10.0f, "%.2f");

    ImGui::PopStyleVar();

    ImGui::End();
    ImGui::PopStyleVar();
}

// ===========================================================================
// draw_scene_tree
// ===========================================================================
static void draw_scene_tree(AppState& app, UIState& ui) {
    float panel_h = ImGui::GetIO().DisplaySize.y - TOP_OFFSET - BOTTOM_OFFSET;

    ImGui::SetNextWindowPos(ImVec2(0, TOP_OFFSET));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_W, panel_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("##SceneTree", nullptr, flags);

    ImGui::Text("Scene Tree");
    ImGui::Separator();

    ImGui::InputTextWithHint("##search", "Search...", ui.scene_search, sizeof(ui.scene_search));
    ImGui::SameLine();
    if (ImGui::Button("All+")) {
        for (auto& n : app.scene.get_all_nodes()) n->visible = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("All-")) {
        for (auto& n : app.scene.get_all_nodes()) n->visible = false;
    }

    ImGui::Separator();

    float tree_h = ImGui::GetContentRegionAvail().y - 30;
    ImGui::BeginChild("##node_list", ImVec2(0, tree_h), false);

    auto all_nodes = app.scene.get_all_nodes();
    for (auto* node : all_nodes) {
        if (!node) continue;

        std::string search_lower = ui.scene_search;
        std::string node_lower = node->name;
        std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
        std::transform(node_lower.begin(), node_lower.end(), node_lower.begin(), ::tolower);
        if (!search_lower.empty() && node_lower.find(search_lower) == std::string::npos)
            continue;

        std::string prefix;
        if (node->children.size() > 0) prefix += "[G] ";
        if (node->mesh) prefix += "[M] ";
        if (node->material) prefix += "[N] ";

        std::string label = prefix + node->name;
        ImGui::PushID(node);

        bool is_selected = (app.scene.selected_node == node);
        bool vis = node->visible;

        ImGui::PushStyleColor(ImGuiCol_Text,
            vis ? ImVec4(1, 1, 1, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        if (ImGui::Selectable(label.c_str(), is_selected)) {
            bool shift = ImGui::GetIO().KeyShift;
            if (shift)
                app.scene.toggle_multi_select(node);
            else
                app.scene.select(node);
        }

        ImGui::PopStyleColor();

        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("SCENE_NODE", &node, sizeof(SceneNode*));
            ImGui::Text("Move %s", node->name.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE")) {
                SceneNode* dragged = *(SceneNode**)payload->Data;
                if (dragged && dragged != node) {
                    app.scene.select(node);
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) {
            ImGui::OpenPopup("NodeContextMenu");
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    if (ImGui::BeginPopup("NodeContextMenu")) {
        SceneNode* ctx_node = app.scene.selected_node;
        if (ctx_node) {
            ImGui::Text("%s", ctx_node->name.c_str());
            ImGui::Separator();

            if (ImGui::MenuItem("Rename")) {
                ui.renaming = true;
                strncpy(ui.rename_buf, ctx_node->name.c_str(), sizeof(ui.rename_buf) - 1);
                ImGui::OpenPopup("RenameNode");
            }
            if (ImGui::MenuItem("Toggle Visibility")) {
                ctx_node->visible = !ctx_node->visible;
            }
            if (ImGui::MenuItem(ctx_node->locked ? "Unlock" : "Lock")) {
                ctx_node->locked = !ctx_node->locked;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy")) {
                app.scene.copy_selected();
            }
            if (ImGui::MenuItem("Paste")) {
                app.scene.paste_clipboard();
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
                app.scene.duplicate_selected();
            }
            if (ImGui::MenuItem("Delete", "Del")) {
                app.scene.delete_selected();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Group", "Ctrl+G")) {
                app.scene.group_selected();
            }
            if (ImGui::MenuItem("Ungroup", "Ctrl+Shift+G")) {
                app.scene.ungroup_selected();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Focus", "F")) {
                app.camera.focus_on(ctx_node->get_world_position(), 2.0f);
            }
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("RenameNode", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        SceneNode* rn = app.scene.selected_node;
        if (rn) {
            ImGui::Text("Rename \"%s\":", rn->name.c_str());
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("##rename", ui.rename_buf, sizeof(ui.rename_buf));
            ImGui::SameLine();
            if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                rn->name = ui.rename_buf;
                ui.renaming = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ui.renaming = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

// ===========================================================================
// draw_properties
// ===========================================================================
static void draw_properties(AppState& app, EditModeState& edit, TransformState& transform) {
    float panel_h = ImGui::GetIO().DisplaySize.y - TOP_OFFSET - BOTTOM_OFFSET;

    ImGui::SetNextWindowPos(ImVec2((float)(ImGui::GetIO().DisplaySize.x - RIGHT_PANEL_W), TOP_OFFSET));
    ImGui::SetNextWindowSize(ImVec2(RIGHT_PANEL_W, panel_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("##Properties", nullptr, flags);

    SceneNode* node = app.scene.selected_node;
    if (!node) {
        ImGui::TextDisabled("No selection");
        ImGui::End();
        return;
    }

    ImGui::Text("Properties");
    ImGui::Separator();

    char name_buf[128];
    strncpy(name_buf, node->name.c_str(), sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputText("##nodename", name_buf, sizeof(name_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        node->name = name_buf;
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("PropTabs")) {

        // --- Transform Tab ---
        if (ImGui::BeginTabItem("Transform")) {
            bool vis = node->visible;
            bool lck = node->locked;

            ImGui::Checkbox("Local Space", &app.local_space_mode);
            ImGui::SameLine();
            ImGui::Checkbox("Visible", &vis);
            node->visible = vis;
            ImGui::SameLine();
            ImGui::Checkbox("Locked", &lck);
            node->locked = lck;

            ImGui::Separator();
            ImGui::Text("Position");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
            if (ImGui::SmallButton("R##pos")) {
                node->position = vec3(0);
            }
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::DragFloat3("##pos", &node->position.x, 0.01f);

            ImGui::Text("Rotation");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
            if (ImGui::SmallButton("R##rot")) {
                node->rotation = vec3(0);
            }
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::DragFloat3("##rot", &node->rotation.x, 0.5f);

            ImGui::Text("Scale");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
            if (ImGui::SmallButton("R##scl")) {
                node->scale = vec3(1);
            }
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::DragFloat3("##scl", &node->scale.x, 0.01f, 0.01f, 100.0f);

            ImGui::EndTabItem();
        }

        // --- Material Tab ---
        if (ImGui::BeginTabItem("Material")) {
            if (node->material) {
                Material& mat = *node->material;

                ImGui::Text("Base Color");
                ImGui::ColorEdit4("##diffuse", &mat.diffuse.x);

                ImGui::Text("Specular Color");
                ImGui::ColorEdit4("##specular", &mat.specular.x);

                ImGui::Separator();

                ImGui::Text("Metallic");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderFloat("##metallic", &mat.metallic, 0.0f, 1.0f);

                ImGui::Text("Roughness");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderFloat("##roughness", &mat.roughness, 0.0f, 1.0f);

                ImGui::Text("AO");
                float ao = mat.ambient.r;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::SliderFloat("##ao", &ao, 0.0f, 1.0f)) {
                    mat.ambient = vec4(ao, ao, ao, 1.0f);
                }

                ImGui::Text("Emission");
                ImGui::ColorEdit4("##emission", &mat.emission.x);

                ImGui::Separator();

                const char* shaders[] = { "Default", "Unlit", "PBR", "Toon", "Glass" };
                static int shader_idx = 0;
                ImGui::Text("Shader");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Combo("##shader", &shader_idx, shaders, IM_ARRAYSIZE(shaders));

                ImGui::Checkbox("Backface Culling", &mat.backface_culling);

                ImGui::Separator();
                ImGui::Text("Opacity");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderFloat("##opacity", &mat.opacity, 0.0f, 1.0f);
            } else {
                ImGui::TextDisabled("No material");
            }
            ImGui::EndTabItem();
        }

        // --- Info Tab ---
        if (ImGui::BeginTabItem("Info")) {
            if (node->mesh) {
                ImGui::Text("Vertices: %d", node->mesh->get_vertex_count());
                ImGui::Text("Triangles: %d", node->mesh->get_triangle_count());
                ImGui::Text("Indices: %d", node->mesh->get_index_count());
            } else {
                ImGui::TextDisabled("No mesh");
            }
            ImGui::Separator();
            ImGui::Text("Position: %.2f, %.2f, %.2f",
                        node->position.x, node->position.y, node->position.z);
            ImGui::Text("Children: %d", (int)node->children.size());
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ===========================================================================
// draw_ai_panel
// ===========================================================================
static void draw_ai_panel(AppState& app) {
    ImGui::SetNextWindowSize(ImVec2(320, 260), ImGuiCond_FirstUseEver);

    ImGui::Begin("AI Generator", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Describe the object to generate:");
    ImGui::InputTextMultiline("##aiprompt", app.ai_prompt, sizeof(app.ai_prompt),
                              ImVec2(ImGui::GetContentRegionAvail().x, 60));

    if (ImGui::Button("Generate", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        if (strlen(app.ai_prompt) > 0) {
            app.status_text = "Generating...";
            app.log_message("AI prompt: " + std::string(app.ai_prompt));

            app.ai_prompt_history.push_back(std::string(app.ai_prompt));
            if ((int)app.ai_prompt_history.size() > app.MAX_AI_HISTORY)
                app.ai_prompt_history.pop_front();

            Scene* result = app.ai_generator.generate_from_prompt(app.ai_prompt, &app.scene);
            if (result) {
                app.last_ai_result = app.ai_generator.get_last_description();
                app.status_text = "Generation complete";
                app.log_message("AI generation complete");
            } else {
                app.status_text = "Generation failed";
                app.log_message("AI generation failed");
            }
        }
    }

    if (!app.ai_prompt_history.empty()) {
        ImGui::Text("History:");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::BeginCombo("##aihist", "")) {
            for (int i = (int)app.ai_prompt_history.size() - 1; i >= 0; i--) {
                if (ImGui::Selectable(app.ai_prompt_history[i].c_str())) {
                    strncpy(app.ai_prompt, app.ai_prompt_history[i].c_str(), sizeof(app.ai_prompt) - 1);
                }
            }
            ImGui::EndCombo();
        }
    }

    if (!app.last_ai_result.empty()) {
        ImGui::Separator();
        ImGui::Text("Last Result:");
        ImGui::TextWrapped("%s", app.last_ai_result.c_str());
    }

    ImGui::End();
}

// ===========================================================================
// draw_light_panel
// ===========================================================================
static void draw_light_panel(AppState& app) {
    ImGui::SetNextWindowSize(ImVec2(320, 400), ImGuiCond_FirstUseEver);

    ImGui::Begin("Light Manager", &app.show_light_panel, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Lights (%d)", (int)app.scene.lights.size());
    ImGui::Separator();

    float list_h = ImGui::GetContentRegionAvail().y - 80;
    ImGui::BeginChild("##lightlist", ImVec2(0, list_h), true);

    for (int i = 0; i < (int)app.scene.lights.size(); i++) {
        Light& l = app.scene.lights[i];
        ImGui::PushID(i);

        bool sel = false;
        if (ImGui::Selectable(l.name.c_str(), sel)) {
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40);
        if (ImGui::SmallButton("X")) {
            app.scene.lights.erase(app.scene.lights.begin() + i);
            i--;
            ImGui::PopID();
            continue;
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    if (ImGui::Button("Add Light", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        Light l;
        l.name = "Light_" + std::to_string(app.scene.lights.size() + 1);
        l.type = Light::POINT;
        l.position = vec3(0, 5, 0);
        app.scene.lights.push_back(l);
        app.log_message("Added light: " + l.name);
    }

    if (!app.scene.lights.empty()) {
        ImGui::Separator();
        ImGui::Text("Edit Selected Light:");

        static int edit_idx = 0;
        if (edit_idx >= (int)app.scene.lights.size())
            edit_idx = 0;

        Light& l = app.scene.lights[edit_idx];

        char light_name_buf[128];
        strncpy(light_name_buf, l.name.c_str(), sizeof(light_name_buf) - 1);
        light_name_buf[sizeof(light_name_buf) - 1] = '\0';
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::InputText("##lghtname", light_name_buf, sizeof(light_name_buf),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            l.name = light_name_buf;
        }

        int type_int = (int)l.type;
        const char* types[] = { "Directional", "Point", "Spot", "Ambient" };
        if (ImGui::Combo("Type", &type_int, types, 4)) {
            l.type = (Light::Type)type_int;
        }

        ImGui::DragFloat3("Position", &l.position.x, 0.05f);
        ImGui::DragFloat3("Direction", &l.direction.x, 0.05f);
        ImGui::ColorEdit4("Color", &l.color.x);
        ImGui::DragFloat("Intensity", &l.intensity, 0.05f, 0.0f, 10.0f);
        ImGui::DragFloat3("Attenuation", &l.attenuation.x, 0.001f, 0.0f, 1.0f);
    }

    ImGui::End();
}

// ===========================================================================
// draw_console
// ===========================================================================
static void draw_console(AppState& app, UIState& ui) {
    ImGui::SetNextWindowSize(ImVec2(500, 250), ImGuiCond_FirstUseEver);

    ImGui::Begin("Console", &ui.show_console, ImGuiWindowFlags_NoCollapse);

    float avail = ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("##consolescroll", ImVec2(0, avail - 30), true);

    for (const auto& msg : app.console_log) {
        ImGui::TextUnformatted(msg.c_str());
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

    if (ImGui::Button("Clear")) {
        app.console_log.clear();
        app.log_message("Console cleared");
    }

    ImGui::End();
}

// ===========================================================================
// draw_settings
// ===========================================================================
static void draw_settings(AppState& app) {
    ImGui::SetNextWindowSize(ImVec2(350, 300), ImGuiCond_FirstUseEver);

    ImGui::Begin("Settings", &app.show_settings, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Application Settings");
    ImGui::Separator();

    ImGui::Text("Grid Size:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::DragFloat("##gridsize", &app.scene.grid_size, 0.5f, 1.0f, 100.0f);

    ImGui::Text("Grid Subdivisions:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::DragInt("##gridsub", &app.scene.grid_subdivisions, 1, 2, 100);

    ImGui::Separator();

    ImGui::Text("Background Color:");
    ImGui::ColorEdit4("##bgcolor", &app.scene.background_color.x);

    ImGui::Separator();

    ImGui::Text("Camera");
    ImGui::Text("FOV:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::DragFloat("##fov", &app.camera.fov, 1.0f, 10.0f, 170.0f, "%.1f");

    ImGui::Text("Near Clip:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::DragFloat("##near", &app.camera.near_clip, 0.01f, 0.01f, 10.0f);

    ImGui::Text("Far Clip:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::DragFloat("##far", &app.camera.far_clip, 10.0f, 100.0f, 5000.0f);

    ImGui::End();
}

// ===========================================================================
// draw_about
// ===========================================================================
static void draw_about(UIState& ui) {
    ImGui::SetNextWindowSize(ImVec2(420, 380), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("About OCP", &ui.show_about,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::Text("OCP - Object Creation Project");
    ImGui::Text("Version 3.3 - KitariosWebStudio (KWS)");
    ImGui::Separator();

    ImGui::Text("Keyboard Shortcuts:");
    ImGui::Separator();

    struct Shortcut { const char* key; const char* desc; };
    Shortcut shortcuts[] = {
        {"Tab",           "Toggle Edit / Object mode"},
        {"G / E / R",     "Grab / Rotate / Scale tool"},
        {"W",             "Toggle wireframe"},
        {"X",             "Toggle X-Ray"},
        {"F",             "Focus on selection"},
        {"H",             "Toggle visibility"},
        {"1 / 2 / 3",     "Front / Right / Top view"},
        {"Numpad 5",      "Toggle ortho/perspective"},
        {"Numpad 0",      "Perspective view"},
        {"Del / Bksp",    "Delete selected"},
        {"Ctrl+Z",        "Undo"},
        {"Ctrl+Y",        "Redo"},
        {"Ctrl+D",        "Duplicate"},
        {"Ctrl+G",        "Group"},
        {"Ctrl+Shift+G",  "Ungroup"},
        {"Ctrl+N",        "New scene"},
        {"Ctrl+S",        "Save"},
        {"Ctrl+O",        "Open"},
        {"Ctrl+A",        "Select all (edit mode)"},
        {"`",             "Toggle console"},
        {"F1",            "Toggle about dialog"},
        {"Esc",           "Cancel / deselect"},
        {"Z / E / Q",     "Vertex / Edge / Face select"},
    };

    ImGui::BeginChild("##shortcuts", ImVec2(0, 240), true);
    for (auto& s : shortcuts) {
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%-16s", s.key);
        ImGui::SameLine(150);
        ImGui::Text("%s", s.desc);
    }
    ImGui::EndChild();

    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        ui.show_about = false;
    }

    ImGui::End();
}

// ===========================================================================
// draw_resource_browser
// ===========================================================================
static void draw_resource_browser(AppState& app, UIState& ui) {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    ImGui::Begin("Resource Library", &ui.show_resource_browser, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Resource Library (%d items)", (int)g_resources.size());
    ImGui::Separator();

    static int category_filter = 0;
    const char* categories[] = { "All", "Textures", "Materials", "Scenes", "Models", "Fonts", "Brushes" };
    ImGui::SetNextItemWidth(120);
    ImGui::Combo("##catfilter", &category_filter, categories, IM_ARRAYSIZE(categories));

    ImGui::SameLine();
    static char res_search[128] = "";
    ImGui::SetNextItemWidth(150);
    ImGui::InputTextWithHint("##ressearch", "Search...", res_search, sizeof(res_search));

    ImGui::Separator();

    float avail_h = ImGui::GetContentRegionAvail().y - 30;
    ImGui::BeginChild("##reslist", ImVec2(0, avail_h), true);

    for (auto& item : g_resources) {
        if (category_filter > 0) {
            if (item.category != categories[category_filter])
                continue;
        }
        if (strlen(res_search) > 0) {
            std::string item_lower = item.name;
            std::string search_lower = res_search;
            std::transform(item_lower.begin(), item_lower.end(), item_lower.begin(), ::tolower);
            std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
            if (item_lower.find(search_lower) == std::string::npos)
                continue;
        }

        ImGui::PushID(&item);
        if (ImGui::Selectable(item.name.c_str())) {
            app.status_text = "Selected resource: " + item.name;
            app.log_message("Resource selected: " + item.path);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Path: %s", item.path.c_str());
            ImGui::Text("Category: %s", item.category.c_str());
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }

    ImGui::EndChild();

    ImGui::End();
}

// ===========================================================================
// draw_edit_panel
// ===========================================================================
static void draw_edit_panel(AppState& app, EditModeState& edit) {
    float panel_h = ImGui::GetIO().DisplaySize.y - TOP_OFFSET - BOTTOM_OFFSET;

    ImGui::SetNextWindowPos(ImVec2(0, TOP_OFFSET));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_W, panel_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("##EditPanel", nullptr, flags);

    ImGui::Text("Edit Mode");
    if (edit.edit_node) {
        ImGui::TextDisabled("%s", edit.edit_node->name.c_str());
    }
    ImGui::Separator();

    ImGui::Text("Select Mode");
    ImGui::RadioButton("Vertex##sel", &edit.select_mode, 0);
    ImGui::RadioButton("Edge##sel", &edit.select_mode, 1);
    ImGui::RadioButton("Face##sel", &edit.select_mode, 2);

    ImGui::Separator();

    ImGui::Text("Tool");
    ImGui::RadioButton("Extrude##tool", &edit.tool, 0);
    ImGui::RadioButton("Inset##tool", &edit.tool, 1);
    ImGui::RadioButton("Bevel##tool", &edit.tool, 2);
    ImGui::RadioButton("Merge##tool", &edit.tool, 3);
    ImGui::RadioButton("Solidify##tool", &edit.tool, 4);
    ImGui::RadioButton("Mirror##tool", &edit.tool, 5);

    ImGui::Separator();

    if (edit.edit_node) {
        auto it = edit.bmeshes.find(edit.edit_node);
        if (it != edit.bmeshes.end()) {
            BMesh& bm = it->second;
            ImGui::Text("Vertices: %d", bm.vert_count);
            ImGui::Text("Edges: %d", bm.edge_count);
            ImGui::Text("Faces: %d", bm.face_count);

            ImGui::Separator();

            int sel_v = 0, sel_e = 0, sel_f = 0;
            for (int i = 0; i < bm.vert_count; i++)
                if (bm.verts[i]->select) sel_v++;
            for (int i = 0; i < bm.edge_count; i++)
                if (bm.edges[i]->select) sel_e++;
            for (int i = 0; i < bm.face_count; i++)
                if (bm.faces[i]->select) sel_f++;

            ImGui::Text("Selected: V=%d  E=%d  F=%d", sel_v, sel_e, sel_f);
        }
    }

    ImGui::Separator();

    if (ImGui::Button("Apply Tool", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        if (edit.edit_node) {
            auto it = edit.bmeshes.find(edit.edit_node);
            if (it != edit.bmeshes.end()) {
                BMesh& bm = it->second;
                switch (edit.tool) {
                    case 0: edit_tools::extrude_region(bm, vec3(0, 0.2f, 0)); break;
                    case 1: edit_tools::inset_faces(bm, 0.1f); break;
                    case 2: edit_tools::bevel_edges(bm, 0.1f, 2); break;
                    case 3: edit_tools::merge_vertices(bm); break;
                    case 4: edit_tools::solidify(bm, 0.1f); break;
                    case 5: edit_tools::mirror(bm, 0); break;
                }
                sync_bmesh_to_mesh(edit.edit_node, edit, app.renderer);
                app.log_message("Applied edit tool");
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Operations");

    if (ImGui::Button("Subdivide", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        if (edit.edit_node) {
            auto it = edit.bmeshes.find(edit.edit_node);
            if (it != edit.bmeshes.end()) {
                edit_tools::subdivide(it->second, 1);
                sync_bmesh_to_mesh(edit.edit_node, edit, app.renderer);
            }
        }
    }
    if (ImGui::Button("Smooth", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        if (edit.edit_node) {
            auto it = edit.bmeshes.find(edit.edit_node);
            if (it != edit.bmeshes.end()) {
                edit_tools::smooth(it->second, 0.5f, 1);
                sync_bmesh_to_mesh(edit.edit_node, edit, app.renderer);
            }
        }
    }
    if (ImGui::Button("Triangulate", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        if (edit.edit_node) {
            auto it = edit.bmeshes.find(edit.edit_node);
            if (it != edit.bmeshes.end()) {
                edit_tools::triangulate(it->second);
                sync_bmesh_to_mesh(edit.edit_node, edit, app.renderer);
            }
        }
    }
    if (ImGui::Button("Recalc Normals", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        if (edit.edit_node) {
            auto it = edit.bmeshes.find(edit.edit_node);
            if (it != edit.bmeshes.end()) {
                edit_tools::recalculate_normals(it->second);
                sync_bmesh_to_mesh(edit.edit_node, edit, app.renderer);
            }
        }
    }
    if (ImGui::Button("Select All", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        if (edit.edit_node) {
            auto it = edit.bmeshes.find(edit.edit_node);
            if (it != edit.bmeshes.end()) {
                it->second.select_all(true);
            }
        }
    }
    if (ImGui::Button("Deselect All", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        if (edit.edit_node) {
            auto it = edit.bmeshes.find(edit.edit_node);
            if (it != edit.bmeshes.end()) {
                it->second.select_all(false);
            }
        }
    }

    ImGui::Separator();

    if (ImGui::Button("Exit Edit Mode", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        exit_edit_mode(app, edit);
    }

    ImGui::End();
}

// ===========================================================================
// draw_sculpt_panel
// ===========================================================================
static void draw_sculpt_panel(AppState& app, EditModeState& edit) {
    float panel_h = ImGui::GetIO().DisplaySize.y - TOP_OFFSET - BOTTOM_OFFSET;

    ImGui::SetNextWindowPos(ImVec2(0, TOP_OFFSET));
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_W, panel_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("##SculptPanel", nullptr, flags);

    ImGui::Text("Sculpt Mode");
    if (edit.edit_node) {
        ImGui::TextDisabled("%s", edit.edit_node->name.c_str());
    }
    ImGui::Separator();

    const char* brush_names[] = { "Draw", "Clay", "Inflate", "Smooth", "Flatten",
                                   "Grab", "Crease", "Pinch", "Mask", "Smooth Mask" };
    int brush_idx = (int)brush_settings.type;
    ImGui::Text("Brush");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::Combo("##brush", &brush_idx, brush_names, IM_ARRAYSIZE(brush_names))) {
        brush_settings.type = (sculpt::BrushType)brush_idx;
    }

    ImGui::Separator();

    ImGui::Text("Radius");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::SliderFloat("##radius", &brush_settings.radius, 0.05f, 5.0f, "%.2f");

    ImGui::Text("Strength");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::SliderFloat("##strength", &brush_settings.strength, 0.0f, 2.0f, "%.2f");

    ImGui::Text("Focal");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::SliderFloat("##focal", &brush_settings.focal, 0.0f, 1.0f, "%.2f");

    ImGui::Separator();

    ImGui::Text("Symmetry");
    ImGui::Checkbox("X", &brush_settings.use_symmetry_x);
    ImGui::SameLine();
    ImGui::Checkbox("Y", &brush_settings.use_symmetry_y);
    ImGui::SameLine();
    ImGui::Checkbox("Z", &brush_settings.use_symmetry_z);

    ImGui::Separator();

    if (edit.edit_node) {
        auto it = edit.bmeshes.find(edit.edit_node);
        if (it != edit.bmeshes.end()) {
            ImGui::Text("Vertices: %d", it->second.vert_count);
        }
    }

    ImGui::Separator();

    if (ImGui::Button("Exit Sculpt Mode", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        exit_sculpt_mode(app, edit);
    }

    ImGui::End();
}

// ===========================================================================
// draw_status_bar
// ===========================================================================
static void draw_status_bar(AppState& app, float fps, int win_w) {
    float bar_h = BOTTOM_OFFSET;
    ImGui::SetNextWindowPos(ImVec2(0, (float)(ImGui::GetIO().DisplaySize.y - bar_h)));
    ImGui::SetNextWindowSize(ImVec2((float)win_w, bar_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
    ImGui::Begin("##StatusBar", nullptr, flags);

    ImGui::TextUnformatted(app.status_text.c_str());
    ImGui::SameLine(ImGui::GetIO().DisplaySize.x - 350);

    ImGui::Text("FPS: %.1f", fps);
    ImGui::SameLine();

    const char* mode_str = "Object";
    if (app.current_mode == MODE_EDIT) mode_str = "Edit";
    else if (app.current_mode == MODE_SCULPT) mode_str = "Sculpt";
    ImGui::Text("Mode: %s", mode_str);
    ImGui::SameLine();

    const char* tool_names[] = { "Grab", "Rotate", "Scale" };
    if (app.selected_tool >= 0 && app.selected_tool <= 2)
        ImGui::Text("Tool: %s", tool_names[app.selected_tool]);
    ImGui::SameLine();

    ImGui::Text("W:%s X:%s",
                app.scene.global_wireframe ? "ON" : "OFF",
                app.scene.xray_mode ? "ON" : "OFF");
    ImGui::SameLine();

    int total_verts = 0, total_tris = 0;
    for (auto* n : app.scene.get_all_nodes()) {
        if (n && n->mesh) {
            total_verts += n->mesh->get_vertex_count();
            total_tris += n->mesh->get_triangle_count();
        }
    }
    ImGui::Text("N:%d V:%d T:%d",
                app.scene.get_node_count(), total_verts, total_tris);

    if (app.scene.selected_node) {
        ImGui::SameLine();
        ImGui::Text("| %s", app.scene.selected_node->name.c_str());
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// ===========================================================================
// draw_editor_ui  (master)
// ===========================================================================
void draw_editor_ui(AppState& app, EditModeState& edit, TransformState& transform,
                    UIState& ui, int win_w, int win_h, bool viewport_hov, float fps)
{
    draw_menu_bar(app, edit, ui);

    if (app.current_mode != MODE_OBJECT) {
        if (app.current_mode == MODE_EDIT)
            draw_edit_panel(app, edit);
        else if (app.current_mode == MODE_SCULPT)
            draw_sculpt_panel(app, edit);
    } else {
        draw_scene_tree(app, ui);
    }

    draw_toolbar(app, app.camera);
    draw_properties(app, edit, transform);
    draw_status_bar(app, fps, win_w);

    if (ui.show_console)
        draw_console(app, ui);
    if (ui.show_light_panel)
        draw_light_panel(app);
    if (ui.show_settings)
        draw_settings(app);
    if (ui.show_about)
        draw_about(ui);
    if (ui.show_resource_browser)
        draw_resource_browser(app, ui);

    draw_ai_panel(app);
}

} // namespace ocp
