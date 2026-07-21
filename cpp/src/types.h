#pragma once
#include "math_utils.h"
#include <string>

namespace ocp {

struct Material {
    std::string name = "Material";
    vec4 diffuse = vec4(0.8f, 0.8f, 0.8f, 1.0f);
    vec4 specular = vec4(1.0f);
    vec4 ambient = vec4(0.2f, 0.2f, 0.2f, 1.0f);
    vec4 emission = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    float shininess = 32.0f;
    float metallic = 0.0f;
    float roughness = 0.5f;
    float opacity = 1.0f;
    bool wireframe = false;
    bool backface_culling = true;

    void set_color(float r, float g, float b, float a = 1.0f) { diffuse = vec4(r, g, b, a); opacity = a; }
    void set_from_hex(const std::string& hex);
};

struct Light {
    std::string name = "Light";
    enum Type { POINT, DIRECTIONAL, SPOT, AMBIENT } type = POINT;
    vec3 position = vec3(0, 5, 5);
    vec3 direction = vec3(0, -1, 0);
    vec4 color = vec4(1.0f);
    float intensity = 1.0f;
    float range = 50.0f;
    float spot_angle = 45.0f;
    float spot_exponent = 32.0f;
    vec3 attenuation = vec3(1.0f, 0.09f, 0.032f);
    bool cast_shadows = true;
    bool enabled = true;
};

} // namespace ocp
