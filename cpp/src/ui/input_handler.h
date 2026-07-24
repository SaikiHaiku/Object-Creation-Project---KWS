#pragma once

struct GLFWwindow;

namespace ocp {

struct AppState;
struct EditModeState;
struct TransformState;
struct UIState;
class Camera;

void setup_input(GLFWwindow* window, AppState& app, EditModeState& edit,
                 TransformState& transform, UIState& ui);
void process_camera_input(GLFWwindow* window, Camera& camera, float dt);

} // namespace ocp
