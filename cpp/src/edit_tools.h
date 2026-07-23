#pragma once
#include "bmesh.h"

namespace ocp {
namespace edit_tools {

struct ExtrudeResult {
    std::vector<BMVert*> new_verts;
    std::vector<BMEdge*> new_edges;
    std::vector<BMFace*> new_faces;
};

// Selection tools
void select_vertex_ring(BMesh& bm, BMVert* v, int steps = 1);
void select_edge_loop(BMesh& bm, BMEdge* e);
void select_face_loop(BMesh& bm, BMFace* f);
void select_island(BMesh& bm, BMVert* v);
void select_by_angle(BMesh& bm, float angle_threshold_deg = 30.0f);

// Modeling tools
ExtrudeResult extrude_region(BMesh& bm, const vec3& offset);
ExtrudeResult extrude_faces(BMesh& bm, const vec3& offset);
BMFace* inset_faces(BMesh& bm, float thickness);
void bevel_edges(BMesh& bm, float amount, int segments = 1);
void bevel_vertices(BMesh& bm, float amount, int segments = 1);
BMEdge* loop_cut(BMesh& bm, BMFace* f, BMVert* v1, BMVert* v2);
void knife_cut(BMesh& bm, const vec3& p1, const vec3& p2);
void bridge_edge_loops(BMesh& bm, BMEdge* e1, BMEdge* e2);
void fill_hole(BMesh& bm, const std::vector<BMVert*>& boundary);
void grid_fill(BMesh& bm, const std::vector<BMVert*>& boundary);
void merge_vertices(BMesh& bm, float threshold = 0.001f);
void merge_at_center(BMesh& bm);
void split_edge(BMesh& bm, BMEdge* e);
void weld_verts(BMesh& bm, BMVert* v1, BMVert* v2);
void rip_vertex(BMesh& bm, BMVert* v, const vec3& direction);
void spin(BMesh& bm, int steps, float angle, const vec3& center, const vec3& axis);
void screw(BMesh& bm, int steps, float pitch, const vec3& center, const vec3& axis);
void solidify(BMesh& bm, float thickness);
void symmetrize(BMesh& bm, int axis, float threshold = 0.001f);
void mirror(BMesh& bm, int axis);

// Modifier-like operations (can be applied destructively)
void subdivide(BMesh& bm, int cuts = 1);
void decimate(BMesh& bm, float ratio = 0.5f);
void triangulate(BMesh& bm);
void untriangulate(BMesh& bm, float angle_threshold_deg = 40.0f);
void relax(BMesh& bm, float factor = 0.5f, int iterations = 1);
void smooth(BMesh& bm, float factor = 0.5f, int iterations = 1);
void recalculate_normals(BMesh& bm);
void flip_normals(BMesh& bm);

} // namespace edit_tools
} // namespace ocp
