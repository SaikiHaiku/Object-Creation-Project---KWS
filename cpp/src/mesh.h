#pragma once
#include "math_utils.h"
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

namespace ocp { class BMesh; }

namespace ocp {

struct Vertex {
    vec3 position{0.0f};
    vec3 normal{0.0f, 1.0f, 0.0f};
    vec2 uv{0.0f};
    vec4 color{1.0f};
};

struct Face {
    std::vector<uint32_t> vertices;
    vec3 normal{0.0f};
};

class Mesh {
public:
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<Face> faces;
    bool dirty = true;
    uint32_t vao = 0, vbo = 0, ebo = 0;

    uint32_t add_vertex(const vec3& pos, const vec3& n = vec3(0,1,0),
                        const vec2& uv = vec2(0), const vec4& col = vec4(1));
    uint32_t add_vertex_spherical_uv(const vec3& pos, const vec4& col = vec4(1));
    uint32_t add_vertex_height_color(const vec3& pos, float h_min, float h_max,
                                     const vec3& n = vec3(0,1,0));
    uint32_t add_face(const std::vector<uint32_t>& vi, const vec3& n = vec3(0));

    void compute_normals(bool smooth = true);
    void apply_transform(const mat4& m);
    void subdivide();
    void subdivide_catmull_clark();
    void invert_faces();
    void weld_vertices(float threshold = 1e-4f);
    void merge_vertices(float threshold = 0.001f);
    void smooth_vertices(float factor = 0.5f, int iterations = 1);
    void extrude_faces(const std::vector<uint32_t>& face_indices, float depth);
    void bevel_edges(float amount, int segments = 3);
    void loop_cut(int axis, float position);
    void flatten_to_plane(const vec3& normal, float offset = 0.0f);
    void mirror(int axis);
    void recalculate_normals();
    void flip_normals();
    void center_pivot();
    void scale_fit(float target_size);
    void make_solid(float thickness);
    Mesh clone() const;
    void clear();

    float surface_area() const;
    vec3 centroid() const;

    int get_triangle_count() const;
    int get_vertex_count() const { return (int)vertices.size(); }
    int get_index_count() const;

    vec3 get_bounding_box_min() const;
    vec3 get_bounding_box_max() const;
    vec3 get_center() const;

    const float* get_vertex_data();
    const uint32_t* get_index_data();
    int get_vertex_data_size() const { return (int)cached_vertex_data.size() * sizeof(float); }
    int get_index_data_size() const { return (int)cached_index_data.size() * sizeof(uint32_t); }

    static Mesh create_arrow(float length = 1.0f, float shaft_radius = 0.05f);
    static Mesh create_arrow_3d(float shaft_length = 1.0f, float shaft_radius = 0.03f,
                                float head_length = 0.3f, float head_radius = 0.1f);
    static Mesh create_tube(float inner_r, float outer_r, float height, int segments = 32);
    static Mesh create_capsule(float radius, float height, int segments = 16);
    static Mesh create_grid(int subdiv_x, int subdiv_z, float size = 1.0f);
    static Mesh create_torus_knot(int p, int q, float radius = 0.4f,
                                  float tube = 0.1f, int segments = 64);
    static Mesh create_icosphere(float radius = 0.5f, int subdivisions = 3);
    static Mesh create_hemisphere(float radius = 0.5f, int segments = 16);

    void bmesh_from_mesh(BMesh& bm) const;
    void mesh_from_bmesh(const BMesh& bm);

private:
    std::vector<float> cached_vertex_data;
    std::vector<uint32_t> cached_index_data;
    void rebuild_cache();
};

} // namespace ocp
