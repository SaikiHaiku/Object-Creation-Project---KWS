#pragma once
#include "math_utils.h"
#include <vector>
#include <cstdint>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ocp {

struct BMVert;
struct BMEdge;
struct BMFace;

struct BMVert {
    vec3 co{0.0f};
    vec3 no{0.0f, 1.0f, 0.0f};
    vec2 uv{0.0f};
    vec4 color{1.0f};
    BMEdge* e = nullptr;
    int index = -1;
    bool select = false;
    bool hide = false;
    void* tag = nullptr;
};

struct BMEdge {
    BMVert* v1 = nullptr;
    BMVert* v2 = nullptr;
    BMFace* f1 = nullptr;
    BMFace* f2 = nullptr;
    BMEdge* v1_disk_next = nullptr;
    BMEdge* v1_disk_prev = nullptr;
    BMEdge* v2_disk_next = nullptr;
    BMEdge* v2_disk_prev = nullptr;
    BMEdge* link_next = nullptr;
    int index = -1;
    bool select = false;
    bool seam = false;
    bool sharp = false;
    bool hide = false;
    bool wire = false;
};

struct BMFace {
    BMVert** verts = nullptr;
    BMEdge** edges = nullptr;
    int len = 0;
    vec3 no{0.0f};
    bool select = false;
    bool hide = false;
    int material_index = 0;
    int index = -1;
};

class BMesh {
public:
    BMesh() = default;
    ~BMesh() { clear(); }

    BMesh(const BMesh&) = delete;
    BMesh& operator=(const BMesh&) = delete;
    BMesh(BMesh&& o) noexcept;
    BMesh& operator=(BMesh&& o) noexcept;

    BMVert* vert_add(const vec3& co);
    BMEdge* edge_add(BMVert* v1, BMVert* v2);
    BMFace* face_add(const std::vector<BMVert*>& verts);

    void vert_kill(BMVert* v);
    void edge_kill(BMEdge* e);
    void face_kill(BMFace* f);

    void vert_splice(BMVert* v_kill, BMVert* v_target);
    bool edge_split(BMEdge* e, BMVert* v, float factor = 0.5f, BMEdge** e_out = nullptr);
    bool face_split(BMFace* f, BMVert* v1, BMVert* v2, BMFace** f1_out = nullptr, BMFace** f2_out = nullptr);
    BMFace* face_split_tri(BMFace* f, int i1, int i2);
    BMFace* face_split_quad(BMFace* f, int i1, int i2, int i3, int i4);

    BMVert* edge_split_middle(BMEdge* e);

    void dissolve_vert(BMVert* v);
    void dissolve_edge(BMEdge* e);
    void dissolve_face(BMFace* f);

    void recalc_face_normal(BMFace* f);
    void recalc_all_normals();
    void recalc_vertex_normal(BMVert* v);

    void select_all(bool state);
    void select_flush();
    bool select_is_single() const;

    BMVert* vert_find_nearest(const vec3& pos, float radius = 1e10f) const;
    BMEdge* edge_find_nearest(const vec3& pos, float& closest_dist) const;
    BMFace* face_pick(const vec3& ray_origin, const vec3& ray_dir) const;

    int vert_ring(BMVert* v, std::vector<BMEdge*>& ring) const;
    int vert_edge_ring(BMVert* v, std::vector<BMEdge*>& ring) const;
    bool face_loop_region(BMFace* f, std::vector<BMFace*>& region, int steps = 1) const;

    vec3 calc_face_center(BMFace* f) const;
    vec3 calc_edge_center(BMEdge* e) const;

    BMVert* get_vert(int i) const { return (i >= 0 && i < vert_count) ? verts[i] : nullptr; }
    BMEdge* get_edge(int i) const { return (i >= 0 && i < edge_count) ? edges[i] : nullptr; }
    BMFace* get_face(int i) const { return (i >= 0 && i < face_count) ? faces[i] : nullptr; }

    int vert_count = 0;
    int edge_count = 0;
    int face_count = 0;

    std::vector<BMVert*> verts;
    std::vector<BMEdge*> edges;
    std::vector<BMFace*> faces;

    void clear();
    void validate() const;

    void from_triangle_mesh(const std::vector<vec3>& positions,
                            const std::vector<vec3>& normals,
                            const std::vector<vec2>& uvs,
                            const std::vector<uint32_t>& indices);
    void to_triangle_mesh(std::vector<vec3>& positions,
                          std::vector<vec3>& normals,
                          std::vector<vec2>& uvs,
                          std::vector<vec4>& colors,
                          std::vector<uint32_t>& indices) const;

    BMesh clone() const;

private:
    void link_vert_edge(BMVert* v, BMEdge* e);
    void unlink_vert_edge(BMVert* v, BMEdge* e);
    void link_edge_face(BMEdge* e, BMFace* f);
    void unlink_edge_face(BMEdge* e, BMFace* f);
    void link_loop(BMEdge* e, BMFace* f);
    void unlink_loop(BMEdge* e, BMFace* f);

    BMVert* add_vert_raw(const vec3& co);
    BMEdge* add_edge_raw(BMVert* v1, BMVert* v2);
    BMFace* add_face_raw(const std::vector<BMVert*>& verts, const std::vector<BMEdge*>& edges);
    void remove_vert_raw(BMVert* v);
    void remove_edge_raw(BMEdge* e);
    void remove_face_raw(BMFace* f);

    struct EdgeKey {
        uint32_t a, b;
        bool operator==(const EdgeKey& o) const { return a == o.a && b == o.b; }
    };
    struct EdgeKeyHash {
        size_t operator()(const EdgeKey& k) const { return std::hash<uint32_t>()(k.a) ^ (std::hash<uint32_t>()(k.b) << 16); }
    };
};

}
