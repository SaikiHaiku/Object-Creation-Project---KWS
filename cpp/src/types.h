#pragma once
#include "math_utils.h"
#include <string>
#include <cstdint>

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

    void set_from_hex(const std::string& hex) {
        std::string h = hex;
        if (!h.empty() && h[0] == '#') h = h.substr(1);
        if (h.size() < 6) return;
        auto hex_byte = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
            if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
            if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
            return 0;
        };
        float r = hex_byte(h[0]) * 16 + hex_byte(h[1]);
        float g = hex_byte(h[2]) * 16 + hex_byte(h[3]);
        float b = hex_byte(h[4]) * 16 + hex_byte(h[5]);
        float a = 1.0f;
        if (h.size() >= 8) a = (hex_byte(h[6]) * 16 + hex_byte(h[7])) / 255.0f;
        diffuse = vec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
        opacity = a;
    }

    std::string to_hex() const {
        auto to_hex2 = [](float v) -> std::string {
            int iv = std::max(0, std::min(255, (int)(v * 255.0f)));
            const char* hex = "0123456789abcdef";
            return std::string(1, hex[iv >> 4]) + hex[iv & 0xf];
        };
        return "#" + to_hex2(diffuse.r) + to_hex2(diffuse.g) + to_hex2(diffuse.b);
    }
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
