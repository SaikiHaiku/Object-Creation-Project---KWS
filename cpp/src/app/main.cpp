#define GLFW_INCLUDE_NONE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "core/app_state.h"
#include "core/events.h"
#include "ui/editor_ui.h"
#include "ui/input_handler.h"

#include <cstdio>

using namespace ocp;

static const char* APP_TITLE = "OCP - Object Creation Project | KitariosWebStudio - KWS";
static int WIN_W = 1600, WIN_H = 900;

static void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    if (h <= 0) h = 1;
    WIN_W = w; WIN_H = h;
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

    AppState app;
    EditModeState edit_state;
    TransformState transform;
    UIState ui;

    app.renderer.initialize(WIN_W, WIN_H);
    setup_default_scene(app);
    ui.discover_resources();
    app.log_message("OCP v3.3 initialized - KitariosWebStudio - KWS");

    setup_input(window, app, edit_state, transform, ui);

    double last_frame_time = glfwGetTime();
    float fps_update_timer = 0.0f;
    int frame_counter = 0;
    float current_fps = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double now = glfwGetTime();
        double dt = now - last_frame_time;
        last_frame_time = now;
        frame_counter++;
        fps_update_timer += (float)dt;
        if (fps_update_timer >= 0.5f) {
            current_fps = (float)frame_counter / fps_update_timer;
            frame_counter = 0;
            fps_update_timer = 0.0f;
        }

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        process_camera_input(window, app.camera, (float)dt);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        bool vph = (mx > LEFT_PANEL_W && mx < WIN_W - RIGHT_PANEL_W &&
                    my > TOP_OFFSET && my < WIN_H - BOTTOM_OFFSET);

        draw_editor_ui(app, edit_state, transform, ui, WIN_W, WIN_H, vph, current_fps);

        app.scene.update();

        ImGui::Render();

        int vp_x = 0, vp_y = TOP_OFFSET;
        int vp_w = WIN_W - RIGHT_PANEL_W, vp_h = WIN_H - TOP_OFFSET - BOTTOM_OFFSET;
        app.renderer.render_scene(app.scene, app.camera, vp_x, vp_y, vp_w, vp_h);

        if (app.scene.selected_node) {
            app.renderer.render_gizmo(app.scene.selected_node, app.selected_tool,
                app.camera.get_view_matrix(), app.camera.get_projection_matrix(), app.camera);
        }

        if (app.current_mode == MODE_EDIT && edit_state.edit_node) {
            auto it = edit_state.bmeshes.find(edit_state.edit_node);
            if (it != edit_state.bmeshes.end()) {
                app.renderer.render_edit_overlays(it->second, edit_state.edit_node->get_world_matrix(),
                    app.camera.get_view_matrix(), app.camera.get_projection_matrix(),
                    edit_state.select_mode, app.camera);
            }
        }

        if (app.current_mode == MODE_SCULPT && sculpt_hit_valid) {
            app.renderer.render_sculpt_cursor(sculpt_hit_pos, brush_settings.radius,
                app.camera.get_view_matrix(), app.camera.get_projection_matrix());
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    app.renderer.cleanup();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
