#pragma once
#include "scene.h"
#include <cstdint>
#include <unordered_map>
#include <functional>

namespace ocp {

class Renderer {
public:
    void initialize(int width, int height);
    void cleanup();
    void render_scene(Scene& scene, Camera& camera, int vp_x, int vp_y, int vp_w, int vp_h);
    void render_grid(float size, int subdivisions, const mat4& view, const mat4& proj);
    void render_gizmo(SceneNode* node, int tool, const mat4& view, const mat4& proj, const Camera& camera);
    void invalidate_mesh(Mesh* mesh);

    uint32_t read_pixel(int x, int y);
    void resize(int w, int h);

    struct MeshBuffers { uint32_t vao = 0, vbo = 0, ebo = 0; int index_count = 0; };

private:
    uint32_t shader_3d = 0, shader_line = 0, shader_grid = 0;
    uint32_t grid_vao = 0, grid_vbo = 0;
    int grid_vertex_count = 0;

    struct PtrHash { size_t operator()(Mesh* p) const { return std::hash<void*>()(p); } };
    std::unordered_map<Mesh*, MeshBuffers, PtrHash> mesh_cache;

    uint32_t sel_vao = 0, sel_vbo = 0;
    int sel_vertex_count = 0;

    uint32_t gizmo_vao = 0, gizmo_vbo = 0;
    int gizmo_vertex_count = 0;

    MeshBuffers get_or_create_buffers(Mesh* mesh);
    void build_grid(float size, int subdivs);
    void init_selection_buffers();
    void init_gizmo_buffers();

    uint32_t compile_shader(const char* vs, const char* fs);
    void use_shader(uint32_t shader);
    void set_mat4(uint32_t shader, const char* name, const mat4& m);
    void set_mat3(uint32_t shader, const char* name, const mat3& m);
    void set_vec3(uint32_t shader, const char* name, const vec3& v);
    void set_vec4(uint32_t shader, const char* name, const vec4& v);
    void set_float(uint32_t shader, const char* name, float v);
    void set_int(uint32_t shader, const char* name, int v);
    void render_node(SceneNode* node, uint32_t shader, const mat4& view, const mat4& proj);
    void render_selection_box(SceneNode* node, const mat4& view, const mat4& proj);
};

} // namespace ocp
