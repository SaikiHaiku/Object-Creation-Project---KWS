#include "camera.h"
#include <algorithm>
#include <cmath>

namespace ocp {

void Camera::update_position_from_orbit() {
    float py = deg2rad(pitch), pyaw = deg2rad(yaw);
    position.x = target.x + distance * cosf(py) * cosf(pyaw);
    position.y = target.y + distance * sinf(py);
    position.z = target.z + distance * cosf(py) * sinf(pyaw);
}

void Camera::orbit(float dyaw, float dpitch) {
    yaw += dyaw;
    pitch = std::clamp(pitch + dpitch, -89.0f, 89.0f);
    update_position_from_orbit();
}

void Camera::zoom(float delta) {
    distance = std::max(0.5f, distance - delta * distance * 0.1f);
    update_position_from_orbit();
}

void Camera::pan(float dx, float dy) {
    vec3 forward = glm::normalize(target - position);
    vec3 right = glm::normalize(glm::cross(forward, up));
    vec3 u = glm::cross(right, forward);
    float speed = distance * 0.002f;
    target += right * dx * speed + u * (-dy) * speed;
    update_position_from_orbit();
}

vec3 Camera::get_forward() const {
    return glm::normalize(target - position);
}

vec3 Camera::get_right() const {
    return glm::normalize(glm::cross(get_forward(), up));
}

vec3 Camera::get_up() const {
    return glm::normalize(glm::cross(get_right(), get_forward()));
}

void Camera::move(float fwd, float right, float up_amount, float speed) {
    vec3 forward_dir = get_forward();
    vec3 right_dir = get_right();
    vec3 up_dir = this->up;
    vec3 move_vec = forward_dir * fwd + right_dir * right + up_dir * up_amount;
    target += move_vec * speed;
    update_position_from_orbit();
}

void Camera::focus_on(const vec3& center, float size) {
    target = center;
    distance = size * 2.0f;
    update_position_from_orbit();
}

void Camera::set_view(const std::string& v) {
    if (v == "front")       { yaw = 0;   pitch = 0; }
    else if (v == "back")   { yaw = 180; pitch = 0; }
    else if (v == "left")   { yaw = 90;  pitch = 0; }
    else if (v == "right")  { yaw = -90; pitch = 0; }
    else if (v == "top")    { yaw = 0;   pitch = 89; }
    else if (v == "bottom") { yaw = 0;   pitch = -89; }
    else if (v == "front_right") { yaw = -45; pitch = 30; }
    update_position_from_orbit();
}

mat4 Camera::get_view_matrix() const {
    return glm::lookAt(position, target, up);
}

mat4 Camera::get_projection_matrix() const {
    if (ortho) {
        float h = ortho_size;
        float w = h * aspect;
        return glm::ortho(-w, w, -h, h, near_clip, far_clip);
    }
    return glm::perspective(deg2rad(fov), aspect, near_clip, far_clip);
}

void Camera::screen_to_ray(float sx, float sy, float vw, float vh,
                           vec3& out_origin, vec3& out_dir) const {
    float ndc_x = (2.0f * sx / vw) - 1.0f;
    float ndc_y = 1.0f - (2.0f * sy / vh);
    mat4 vp = get_projection_matrix() * get_view_matrix();
    mat4 inv_vp = glm::inverse(vp);
    vec4 near4 = inv_vp * vec4(ndc_x, ndc_y, -1.0f, 1.0f);
    vec4 far4 = inv_vp * vec4(ndc_x, ndc_y, 1.0f, 1.0f);
    near4 /= near4.w; far4 /= far4.w;
    out_origin = vec3(near4);
    out_dir = glm::normalize(vec3(far4) - out_origin);
}

} // namespace ocp
