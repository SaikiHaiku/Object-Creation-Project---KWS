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

// --- Proper Value Noise with Smooth Interpolation ---

inline float hash_val(int x, int y) {
    int n = x * 73856093 ^ y * 19349669;
    n = (n ^ (n >> 13)) * 1274126177;
    n = n ^ (n >> 16);
    return (float)(n & 0x7FFFFFFF) / 1073741823.5f - 1.0f;
}

inline float hash_val3(int x, int y, int z) {
    int n = x * 73856093 ^ y * 19349669 ^ z * 83492791;
    n = (n ^ (n >> 13)) * 1274126177;
    n = n ^ (n >> 16);
    return (float)(n & 0x7FFFFFFF) / 1073741823.5f - 1.0f;
}

inline float noise2d(float x, float y) {
    int ix = (int)std::floor(x);
    int iy = (int)std::floor(y);
    float fx = x - (float)ix;
    float fy = y - (float)iy;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float a = hash_val(ix, iy);
    float b = hash_val(ix + 1, iy);
    float c = hash_val(ix, iy + 1);
    float d = hash_val(ix + 1, iy + 1);
    return lerp(lerp(a, b, fx), lerp(c, d, fx), fy);
}

inline float noise3d(float x, float y, float z) {
    int ix = (int)std::floor(x);
    int iy = (int)std::floor(y);
    int iz = (int)std::floor(z);
    float fx = x - (float)ix;
    float fy = y - (float)iy;
    float fz = z - (float)iz;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    fz = fz * fz * (3.0f - 2.0f * fz);
    float v000 = hash_val3(ix, iy, iz);
    float v100 = hash_val3(ix + 1, iy, iz);
    float v010 = hash_val3(ix, iy + 1, iz);
    float v110 = hash_val3(ix + 1, iy + 1, iz);
    float v001 = hash_val3(ix, iy, iz + 1);
    float v101 = hash_val3(ix + 1, iy, iz + 1);
    float v011 = hash_val3(ix, iy + 1, iz + 1);
    float v111 = hash_val3(ix + 1, iy + 1, iz + 1);
    return lerp(
        lerp(lerp(v000, v100, fx), lerp(v010, v110, fx), fy),
        lerp(lerp(v001, v101, fx), lerp(v011, v111, fx), fy),
        fz
    );
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

inline float fbm3d(float x, float y, float z, int octaves = 6) {
    float val = 0.0f, amp = 0.5f, freq = 1.0f;
    for (int i = 0; i < octaves; i++) {
        val += amp * noise3d(x * freq, y * freq, z * freq);
        freq *= 2.0f;
        amp *= 0.5f;
    }
    return val;
}

template<typename MeshType>
void deform_mesh(MeshType& m, float amount, float seed) {
    for (auto& v : m.vertices) {
        float wx = v.position.x + fbm3d(v.position.x * 2.0f + seed + 100.0f, v.position.y * 2.0f, v.position.z * 2.0f, 3) * amount * 0.6f;
        float wy = v.position.y + fbm3d(v.position.x * 2.0f, v.position.y * 2.0f + seed + 200.0f, v.position.z * 2.0f, 3) * amount * 0.6f;
        float wz = v.position.z + fbm3d(v.position.x * 2.0f, v.position.y * 2.0f, v.position.z * 2.0f + seed + 300.0f, 3) * amount * 0.6f;
        float n = fbm3d(wx * 3.0f + seed,
                        wy * 3.0f + seed * 0.7f,
                        wz * 3.0f + seed * 1.3f, 6);
        float ridge = 1.0f - std::abs(fbm3d(wx * 4.0f + seed + 50.0f, wy * 4.0f, wz * 4.0f, 4));
        ridge = ridge * ridge * 0.25f;
        v.position += v.normal * (n + ridge) * amount;
    }
    m.dirty = true;
    m.compute_normals();
}

inline mat4 mat4_inverse(const mat4& m) { return glm::inverse(m); }
inline mat4 mat4_transpose(const mat4& m) { return glm::transpose(m); }
inline mat3 mat3_normal_from_mat4(const mat4& m) { return glm::transpose(glm::inverse(mat3(m))); }

} // namespace ocp
