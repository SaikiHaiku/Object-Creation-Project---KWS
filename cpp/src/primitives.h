#pragma once
#include "mesh.h"
#include <map>
#include <string>
#include <functional>

namespace ocp {

Mesh create_cube(float size = 1.0f);
Mesh create_sphere(float radius = 0.5f, int segments = 64, int rings = 32);
Mesh create_cylinder(float radius = 0.5f, float height = 1.0f, int segments = 48);
Mesh create_cone(float radius = 0.5f, float height = 1.0f, int segments = 48);
Mesh create_torus(float major_r = 0.5f, float minor_r = 0.2f, int major_seg = 48, int minor_seg = 24);
Mesh create_plane(float size = 1.0f, int subdivisions = 1);
Mesh create_arrow(float length = 2.0f);
Mesh create_torus_knot(int p = 2, int q = 3, float major_r = 1.0f, float tube_r = 0.15f);
Mesh create_ico_sphere(float radius = 0.5f, int subdivisions = 2);

using PrimitiveCreator = std::function<Mesh(float)>;
const std::map<std::string, PrimitiveCreator>& get_primitive_creators();

} // namespace ocp
