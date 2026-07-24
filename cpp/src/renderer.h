#pragma once
#include "scene.h"
#include "bmesh.h"
#include <cstdint>
#include <unordered_map>

namespace ocp {

class Renderer {
public:
    void initialize(int width, int height);
    void cleanup();
    void render_scene(Scene& scene, Camera& camera, int vp_x, int vp_y, int vp_w, int vp_h);
    void render_gizmo(SceneNode* node, int tool, const mat4& view, const mat4& proj, const Camera& camera);
    void render_manipulator_gizmo(SceneNode* node, int tool, const mat4& view, const mat4& proj, const Camera& camera, float snap);
    void render_edit_overlays(const BMesh& bm, const mat4& model, const mat4& view, const mat4& proj,
                              int select_mode, const Camera& camera);
    void render_sculpt_cursor(const vec3& world_pos, float radius, const mat4& view, const mat4& proj);
    void invalidate_mesh(Mesh* mesh);
    uint32_t read_pixel(int x, int y);
    void resize(int w, int h);

    int get_draw_calls() const { return draw_calls; }
    int get_triangles_rendered() const { return triangles_rendered; }
    float get_frame_time_ms() const { return frame_time_ms; }

    struct MeshBuffers { uint32_t vao=0, vbo=0, ebo=0; int index_count=0; };

private:
    uint32_t shader_3d=0, shader_line=0, shader_grid=0, shader_outline=0;

    uint32_t grid_vao=0, grid_vbo=0;
    int grid_vertex_count=0;
    float last_grid_size=0; int last_grid_subdiv=0;

    struct PtrHash { size_t operator()(Mesh* p) const { return std::hash<void*>()(p); } };
    std::unordered_map<Mesh*, MeshBuffers, PtrHash> mesh_cache;

    uint32_t sel_vao=0, sel_vbo=0;
    int sel_vertex_count=0;

    uint32_t gizmo_vao=0, gizmo_vbo=0;
    int gizmo_vertex_count=0;

    uint32_t edit_vao=0, edit_vbo=0;
    int edit_vertex_count=0;
    uint32_t sculpt_cursor_vao=0, sculpt_cursor_vbo=0;
    int sculpt_cursor_vertex_count=0;

    int draw_calls=0, triangles_rendered=0;
    float frame_time_ms=0;

    MeshBuffers get_or_create_buffers(Mesh* mesh);
    void build_grid(float size, int subdivs);
    void render_node(SceneNode* node, uint32_t shader, const mat4& view, const mat4& proj, bool global_wire, bool xray);
    void render_selection_box(SceneNode* node, const mat4& view, const mat4& proj);
    void render_selection_outline(SceneNode* node, const mat4& view, const mat4& proj, const Camera& camera);

    uint32_t compile_shader(const char* vs, const char* fs);
    void use_shader(uint32_t shader);
    void set_mat4(uint32_t shader, const char* name, const mat4& m);
    void set_mat3(uint32_t shader, const char* name, const mat3& m);
    void set_vec3(uint32_t shader, const char* name, const vec3& v);
    void set_vec4(uint32_t shader, const char* name, const vec4& v);
    void set_float(uint32_t shader, const char* name, float v);
    void set_int(uint32_t shader, const char* name, int v);
};

} // namespace ocp
