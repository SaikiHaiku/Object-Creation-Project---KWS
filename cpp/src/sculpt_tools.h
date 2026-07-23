#pragma once
#include "bmesh.h"

namespace ocp {
namespace sculpt {

enum class BrushType {
    Draw,
    Clay,
    Inflate,
    Smooth,
    Flatten,
    Grab,
    Crease,
    Pinch,
    Mask,
    SmoothMask
};

struct BrushSettings {
    BrushType type = BrushType::Draw;
    float radius = 0.5f;
    float strength = 0.5f;
    float focal = 0.5f;       // 0=sharp, 1=smooth falloff
    bool use_symmetry_x = false;
    bool use_symmetry_y = false;
    bool use_symmetry_z = false;
};

void apply_brush(BMesh& bm, const vec3& hit_pos, const vec3& hit_normal,
                 const BrushSettings& settings);

void smooth_vertices(BMesh& bm, const vec3& center, float radius, float strength);

} // namespace sculpt
} // namespace ocp
