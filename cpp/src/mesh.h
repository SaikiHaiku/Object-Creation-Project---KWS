#pragma once
#include "math_utils.h"
#include <vector>
#include <string>
#include <cstdint>

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
    uint32_t add_face(const std::vector<uint32_t>& vi, const vec3& n = vec3(0));

    void compute_normals();
    void apply_transform(const mat4& m);
    void subdivide();
    void invert_faces();
    void weld_vertices(float threshold = 1e-4f);
    Mesh clone() const;
    void clear();

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

private:
    std::vector<float> cached_vertex_data;
    std::vector<uint32_t> cached_index_data;
    void rebuild_cache();
};

} // namespace ocp
