#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <algorithm>

namespace ocp {

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat3 = glm::mat3;
using mat4 = glm::mat4;

inline float deg2rad(float d) { return d * 0.017453292519943295f; }
inline float rad2deg(float r) { return r * 57.29577951308232f; }

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float clamp(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
inline float smoothstep(float e0, float e1, float x) {
    float t = clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float noise2d(float x, float y) {
    int n = (int)(x * 57.0f + y * 131.0f);
    n = (n << 13) ^ n;
    return 1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7FFFFFFF) / 1073741824.0f;
}

inline float fbm(float x, float y, int octaves = 6) {
    float val = 0.0f, amp = 0.5f, freq = 1.0f;
    for (int i = 0; i < octaves; i++) {
        val += amp * noise2d(x * freq, y * freq);
        freq *= 2.0f;
        amp *= 0.5f;
    }
    return val;
}

inline mat4 mat4_inverse(const mat4& m) { return glm::inverse(m); }
inline mat4 mat4_transpose(const mat4& m) { return glm::transpose(m); }
inline mat3 mat3_normal_from_mat4(const mat4& m) { return glm::transpose(glm::inverse(mat3(m))); }

} // namespace ocp
