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

using namespace ocp;

static const char* APP_TITLE = "OCP - Object Creation Project | KitariosWebStudio - KWS";
static int WIN_W = 1600, WIN_H = 900;

// State
static Scene scene;
static Camera camera;
static Renderer renderer;
static ObjectGenerator ai_generator;
static int selected_tool = 0; // 0=Move, 1=Rotate, 2=Scale
static char ai_prompt[512] = "";
static std::string status_text = "Ready";
static std::string last_ai_result;

// Mouse state
static bool mouse_orbiting = false, mouse_panning = false;
static double last_mx = 0, last_my = 0;
static bool viewport_hovered = false;

// File dialogs (simple native)
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


static void add_primitive(const std::string& name) {
    auto& creators = get_primitive_creators();
    auto it = creators.find(name);
    if (it == creators.end()) return;
    Mesh m = it->second(1.0f);
    SceneNode* node = new SceneNode();
    node->name = name;
    node->mesh = new Mesh(m);
    node->material = new Material();
    node->material->name = name + "_mat";
    scene.add_node(node);
    scene.select(node);
    status_text = "Added " + name;
}

static void setup_default_scene() {
    scene.clear();
    // Lights
    Light l1; l1.name = "Main Light"; l1.type = Light::DIRECTIONAL;
    l1.position = vec3(5, 10, 7); l1.direction = glm::normalize(vec3(-0.5f, -1.0f, -0.7f));
    scene.lights.push_back(l1);
    Light l2; l2.name = "Fill Light"; l2.type = Light::POINT;
    l2.position = vec3(-3, 5, -2); l2.color = vec4(0.6f, 0.7f, 1.0f, 1); l2.intensity = 0.5f;
    scene.lights.push_back(l2);
    Light l3; l3.name = "Ambient"; l3.type = Light::AMBIENT;
    l3.color = vec4(0.15f, 0.15f, 0.2f, 1);
    scene.lights.push_back(l3);

    // Demo objects
    auto add_demo = [](const std::string& name, const vec3& pos, const vec4& col, float scale_val) {
        Mesh m = create_cube(1.0f);
        SceneNode* n = new SceneNode();
        n->name = name;
        n->position = pos;
        n->scale = vec3(scale_val);
        n->mesh = new Mesh(m);
        n->material = new Material();
        n->material->diffuse = col;
        n->material->name = name + "_mat";
        scene.add_node(n);
    };
    add_demo("Red Cube", vec3(-1.5f, 0.5f, 0), vec4(0.8f, 0.1f, 0.1f, 1), 1.0f);
    add_demo("Blue Sphere", vec3(0, 0.5f, 0), vec4(0.1f, 0.3f, 0.8f, 1), 1.0f);
    scene.root.children.back()->mesh = new Mesh(create_sphere(0.5f));
    add_demo("Gold Torus", vec3(1.5f, 0.5f, 0), vec4(1.0f, 0.84f, 0.0f, 1), 1.0f);
    scene.root.children.back()->mesh = new Mesh(create_torus(0.4f, 0.15f));
    add_demo("Ground", vec3(0, 0, 0), vec4(0.3f, 0.6f, 0.3f, 1), 5.0f);
    scene.root.children.back()->mesh = new Mesh(create_plane(10.0f));
    scene.root.children.back()->material->shininess = 2;
    scene.root.children.back()->material->roughness = 1.0f;

    camera = Camera();
    camera.set_view("front_right");
    status_text = "Default scene loaded";
}

// GLFW callbacks
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
            if (scene.raycast(origin, dir, hit, dist)) { scene.select(hit); status_text = "Selected: " + hit->name; }
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

// ImGui panels
static void draw_toolbar() {
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)WIN_W, 52), ImGuiCond_Always);
    ImGui::Begin("##Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
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
    ImGui::SameLine();
    if (ImGui::Button("TorusKnot")) add_primitive("torus_knot");
    ImGui::SameLine();
    if (ImGui::Button("IcoSphere")) add_primitive("ico_sphere");
    ImGui::SameLine(300);
    ImGui::Text("| Views:");
    ImGui::SameLine();
    if (ImGui::Button("Front")) camera.set_view("front");
    ImGui::SameLine();
    if (ImGui::Button("Top")) camera.set_view("top");
    ImGui::SameLine();
    if (ImGui::Button("Right")) camera.set_view("right");
    ImGui::SameLine();
    if (ImGui::Button("3D")) camera.set_view("front_right");
    ImGui::SameLine(550);
    ImGui::Text("| Tools:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Move", selected_tool == 0)) selected_tool = 0;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", selected_tool == 1)) selected_tool = 1;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", selected_tool == 2)) selected_tool = 2;
    ImGui::End();
}

static void draw_scene_tree() {
    ImGui::SetNextWindowPos(ImVec2(0, 54), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(220, (float)WIN_H - 54), ImGuiCond_Always);
    ImGui::Begin("Scene Hierarchy", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    auto nodes = scene.get_all_nodes();
    for (auto* node : nodes) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (scene.selected_node == node) flags |= ImGuiTreeNodeFlags_Selected;
        if (!node->visible) flags |= ImGuiTreeNodeFlags_Selected;
        std::string label = node->mesh ? "[M] " : "[N] ";
        label += node->name;
        ImGui::TreeNodeEx((void*)(uintptr_t)node, flags, "%s", label.c_str());
        if (ImGui::IsItemClicked()) scene.select(node);
        if (ImGui::IsItemClicked(1)) ImGui::OpenPopup("node_menu");
    }
    if (ImGui::BeginPopup("node_menu")) {
        if (ImGui::MenuItem("Rename") && scene.selected_node) {
            // Simple rename via input
        }
        if (ImGui::MenuItem("Toggle Visibility") && scene.selected_node) {
            scene.selected_node->visible = !scene.selected_node->visible;
        }
        if (ImGui::MenuItem("Duplicate")) scene.duplicate_selected();
        if (ImGui::MenuItem("Delete")) scene.delete_selected();
        if (ImGui::MenuItem("Focus") && scene.selected_node) {
            vec3 center = (scene.selected_node->get_bounding_box_min() + scene.selected_node->get_bounding_box_max()) * 0.5f;
            float size = glm::length(scene.selected_node->get_bounding_box_max() - scene.selected_node->get_bounding_box_min());
            camera.focus_on(center, size);
        }
        ImGui::EndPopup();
    }
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
    if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) node->name = name_buf;

    ImGui::Separator();
    ImGui::Text("Transform");
    ImGui::DragFloat3("Position", &node->position.x, 0.1f);
    ImGui::DragFloat3("Rotation", &node->rotation.x, 1.0f);
    ImGui::DragFloat3("Scale", &node->scale.x, 0.05f, 0.01f, 100.0f);

    if (node->material) {
        ImGui::Separator();
        ImGui::Text("Material");
        vec4& d = node->material->diffuse;
        float col[4] = {d.r, d.g, d.b, d.a};
        if (ImGui::ColorEdit4("Diffuse", col)) { d = vec4(col[0], col[1], col[2], col[3]); node->material->opacity = col[3]; }
        ImGui::SliderFloat("Metallic", &node->material->metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &node->material->roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Shininess", &node->material->shininess, 1.0f, 200.0f);
        ImGui::Checkbox("Wireframe", &node->material->wireframe);
        ImGui::Checkbox("Backface Culling", &node->material->backface_culling);
    }

    if (node->mesh) {
        ImGui::Separator();
        ImGui::Text("Mesh Info");
        ImGui::Text("Vertices: %d", node->mesh->get_vertex_count());
        ImGui::Text("Triangles: %d", node->mesh->get_triangle_count());
        if (ImGui::Button("Subdivide")) { node->mesh->subdivide(); renderer.invalidate_mesh(node->mesh); }
        ImGui::SameLine();
        if (ImGui::Button("Invert Faces")) { node->mesh->invert_faces(); renderer.invalidate_mesh(node->mesh); }
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
        Scene* result = ai_generator.generate_from_prompt(ai_prompt, &scene);
        last_ai_result = ai_generator.get_last_description();
        status_text = "AI Generated: " + last_ai_result;
        if (result != &scene) {
            // Transfer lights/camera from old scene
        }
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
    if (ImGui::Button("Neon Spiral", ImVec2(-1, 0))) { strcpy(ai_prompt, "neon spiral"); }
    if (ImGui::Button("Clear Scene", ImVec2(-1, 0))) { setup_default_scene(); }
    if (!last_ai_result.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("Last: %s", last_ai_result.c_str());
    }
    ImGui::End();
}

static void draw_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) setup_default_scene();
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
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) scene.duplicate_selected();
            if (ImGui::MenuItem("Delete", "Del")) scene.delete_selected();
            if (ImGui::MenuItem("Select All")) { auto ns = scene.get_all_nodes(); if (!ns.empty()) scene.select(ns[0]); }
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
            if (ImGui::MenuItem("Focus Selected") && scene.selected_node) {
                vec3 c = (scene.selected_node->get_bounding_box_min() + scene.selected_node->get_bounding_box_max()) * 0.5f;
                float s = glm::length(scene.selected_node->get_bounding_box_max() - scene.selected_node->get_bounding_box_min());
                camera.focus_on(c, s);
            }
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
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About OCP")) {
                // Could open a popup
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
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

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Check if mouse is in viewport area
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        viewport_hovered = (mx > 220 && mx < WIN_W - 350 && my > 54 && my < WIN_H);

        draw_menu_bar();
        draw_toolbar();
        draw_scene_tree();
        draw_properties();
        draw_ai_panel();

        scene.update();

        ImGui::Render();

        // Render 3D viewport into the viewport area
        int vp_x = 220, vp_y = 0;
        int vp_w = WIN_W - 570, vp_h = WIN_H - 54;
        renderer.render_scene(scene, camera, vp_x, vp_y, vp_w, vp_h);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Status bar
        glViewport(0, 0, display_w, display_h);

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
