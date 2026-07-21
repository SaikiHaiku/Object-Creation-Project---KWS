#pragma once
#include "math_utils.h"
#include <string>

namespace ocp {

class Camera {
public:
    std::string name = "Camera";
    vec3 position = vec3(5, 5, 5);
    vec3 target = vec3(0);
    vec3 up = vec3(0, 1, 0);
    float fov = 60.0f;
    float near_clip = 0.1f;
    float far_clip = 1000.0f;
    float aspect = 16.0f / 9.0f;
    bool ortho = false;
    float ortho_size = 10.0f;

    void orbit(float dyaw, float dpitch);
    void zoom(float delta);
    void pan(float dx, float dy);
    void focus_on(const vec3& center, float size);
    void set_view(const std::string& name);

    mat4 get_view_matrix() const;
    mat4 get_projection_matrix() const;

    void screen_to_ray(float sx, float sy, float vw, float vh,
                       vec3& out_origin, vec3& out_dir) const;

private:
    float yaw = -45.0f;
    float pitch = 30.0f;
    float distance = 10.0f;
    void update_position_from_orbit();
};

} // namespace ocp
