#include "renderer.h"
#include <glad/gl.h>
#include <cstdio>
#include <cstring>

namespace ocp {

static const char* VERT_3D = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aColor;
uniform mat4 uModel, uView, uProjection;
uniform mat3 uNormalMatrix;
out vec3 vWorldPos, vNormal;
out vec2 vUV;
out vec4 vColor;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vNormal = normalize(uNormalMatrix * aNormal);
    vUV = aUV;
    vColor = aColor;
    gl_Position = uProjection * uView * wp;
}
)";

static const char* FRAG_3D = R"(
#version 330 core
in vec3 vWorldPos, vNormal;
in vec2 vUV;
in vec4 vColor;
uniform vec3 uCameraPos;
uniform vec4 uDiffuse;
uniform vec3 uSpecular;
uniform float uShininess, uMetallic, uRoughness;
uniform int uLightCount;
struct Light { vec3 pos, dir, col, att; int type; float range, spotAngle, spotExp; };
uniform Light uLights[8];
out vec4 FragColor;

vec3 computeLight(Light L, vec3 N, vec3 V) {
    vec3 Ld;
    float att = 1.0;
    if (L.type == 0) { Ld = normalize(L.pos - vWorldPos); float d = length(L.pos - vWorldPos); att = 1.0 / (L.att.x + L.att.y*d + L.att.z*d*d); }
    else if (L.type == 1) { Ld = normalize(-L.dir); }
    else if (L.type == 2) { Ld = normalize(L.pos - vWorldPos); float d = length(L.pos - vWorldPos); att = 1.0 / (L.att.x + L.att.y*d + L.att.z*d*d);
        float ca = dot(Ld, normalize(-L.dir)); if (ca < cos(radians(L.spotAngle))) att = 0.0; else att *= pow(ca, L.spotExp);
    } else { Ld = normalize(L.pos - vWorldPos); }
    float NdL = max(dot(N, Ld), 0.0);
    vec3 diff = L.col * NdL * uDiffuse.rgb;
    vec3 H = normalize(Ld + V);
    float NdH = max(dot(N, H), 0.0);
    float sp = pow(NdH, uShininess);
    vec3 spec = L.col * sp * uSpecular * (1.0 - uRoughness);
    vec3 md = diff * (1.0 - uMetallic);
    vec3 ms = mix(spec, L.col * sp, uMetallic);
    return (md + ms) * att;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 result = vec3(0.1) * uDiffuse.rgb;
    for (int i = 0; i < uLightCount && i < 8; i++)
        result += computeLight(uLights[i], N, V);
    FragColor = vec4(result, uDiffuse.a * vColor.a);
}
)";

static const char* VERT_GRID = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
uniform mat4 uVP;
out vec3 vWorldPos;
out vec4 vColor;
void main() { vWorldPos = aPos; vColor = aColor; gl_Position = uVP * vec4(aPos, 1.0); }
)";

static const char* FRAG_GRID = R"(
#version 330 core
in vec3 vWorldPos;
in vec4 vColor;
out vec4 FragColor;
void main() { FragColor = vColor; }
)";

static const char* VERT_LINE = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
uniform mat4 uMVP;
out vec4 vColor;
void main() { vColor = aColor; gl_Position = uMVP * vec4(aPos, 1.0); }
)";

static const char* FRAG_LINE = R"(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() { FragColor = vColor; }
)";

uint32_t Renderer::compile_shader(const char* vs_src, const char* fs_src) {
    auto compile = [](GLenum type, const char* src) -> uint32_t {
        uint32_t s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        int ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) { char log[512]; glGetShaderInfoLog(s, 512, nullptr, log); fprintf(stderr, "Shader error: %s\n", log); }
        return s;
    };
    uint32_t vs = compile(GL_VERTEX_SHADER, vs_src);
    uint32_t fs = compile(GL_FRAGMENT_SHADER, fs_src);
    uint32_t prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    int ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { char log[512]; glGetProgramInfoLog(prog, 512, nullptr, log); fprintf(stderr, "Link error: %s\n", log); }
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

void Renderer::use_shader(uint32_t s) { glUseProgram(s); }

void Renderer::set_mat4(uint32_t s, const char* n, const mat4& m) {
    GLint loc = glGetUniformLocation(s, n); if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}
void Renderer::set_mat3(uint32_t s, const char* n, const mat3& m) {
    GLint loc = glGetUniformLocation(s, n); if (loc >= 0) glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}
void Renderer::set_vec3(uint32_t s, const char* n, const vec3& v) {
    GLint loc = glGetUniformLocation(s, n); if (loc >= 0) glUniform3fv(loc, 1, glm::value_ptr(v));
}
void Renderer::set_vec4(uint32_t s, const char* n, const vec4& v) {
    GLint loc = glGetUniformLocation(s, n); if (loc >= 0) glUniform4fv(loc, 1, glm::value_ptr(v));
}
void Renderer::set_float(uint32_t s, const char* n, float v) {
    GLint loc = glGetUniformLocation(s, n); if (loc >= 0) glUniform1f(loc, v);
}
void Renderer::set_int(uint32_t s, const char* n, int v) {
    GLint loc = glGetUniformLocation(s, n); if (loc >= 0) glUniform1i(loc, v);
}

void Renderer::initialize(int w, int h) {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.15f, 0.15f, 0.2f, 1.0f);

    shader_3d = compile_shader(VERT_3D, FRAG_3D);
    shader_line = compile_shader(VERT_LINE, FRAG_LINE);
    shader_grid = compile_shader(VERT_GRID, FRAG_GRID);
    build_grid(20.0f, 20);
}

void Renderer::build_grid(float size, int subdiv) {
    std::vector<float> data;
    float step = size / subdiv;
    float hs = size * 0.5f;
    for (int i = 0; i <= subdiv; i++) {
        float pos = -hs + i * step;
        float alpha = (fabsf(pos) < 0.05f) ? 0.8f : 0.4f;
        float cr = (fabsf(pos) < 0.05f) ? 0.5f : 0.35f;
        float cg = (fabsf(pos) < 0.05f) ? 0.5f : 0.35f;
        float cb = (fabsf(pos) < 0.05f) ? 0.7f : 0.35f;
        data.insert(data.end(), {pos, 0, -hs, cr, cg, cb, alpha});
        data.insert(data.end(), {pos, 0, hs, cr, cg, cb, alpha * 0.3f});
        data.insert(data.end(), {-hs, 0, pos, cr, cg, cb, alpha});
        data.insert(data.end(), {hs, 0, pos, cr, cg, cb, alpha * 0.3f});
    }
    grid_vertex_count = (int)data.size() / 7;
    glGenVertexArrays(1, &grid_vao);
    glGenBuffers(1, &grid_vbo);
    glBindVertexArray(grid_vao);
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void Renderer::render_grid(float size, int subdivs, const mat4& view, const mat4& proj) {
    use_shader(shader_grid);
    set_mat4(shader_grid, "uVP", proj * view);
    glBindVertexArray(grid_vao);
    glDrawArrays(GL_LINES, 0, grid_vertex_count);
    glBindVertexArray(0);
}

Renderer::MeshBuffers Renderer::get_or_create_buffers(Mesh* mesh) {
    uint32_t key = (uint32_t)(uintptr_t)mesh;
    auto it = mesh_cache.find(key);
    if (it != mesh_cache.end() && !mesh->dirty) return it->second;
    if (it != mesh_cache.end()) {
        glDeleteBuffers(1, &it->second.vbo);
        glDeleteBuffers(1, &it->second.ebo);
        glDeleteVertexArrays(1, &it->second.vao);
    }
    MeshBuffers mb;
    const float* vd = mesh->get_vertex_data();
    const uint32_t* id = mesh->get_index_data();
    int vc = mesh->get_vertex_count();
    int ic = mesh->get_index_count();
    glGenVertexArrays(1, &mb.vao);
    glGenBuffers(1, &mb.vbo);
    glGenBuffers(1, &mb.ebo);
    glBindVertexArray(mb.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mb.vbo);
    glBufferData(GL_ARRAY_BUFFER, vc * 12 * sizeof(float), vd, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mb.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ic * sizeof(uint32_t), id, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glBindVertexArray(0);
    mb.index_count = ic;
    mesh_cache[key] = mb;
    mesh->dirty = false;
    return mb;
}

void Renderer::render_node(SceneNode* node, uint32_t shader, const mat4& view, const mat4& proj) {
    if (!node->visible) return;
    node->update_matrices(view != mat4(0) ? mat4(1) : mat4(1));

    if (node->mesh && node->material && node->mesh->get_vertex_count() > 0) {
        MeshBuffers mb = get_or_create_buffers(node->mesh);
        set_mat4(shader, "uModel", node->get_world_matrix());
        set_mat3(shader, "uNormalMatrix", mat3_normal_from_mat4(node->get_world_matrix()));
        Material& mat = *node->material;
        set_vec4(shader, "uDiffuse", mat.diffuse);
        set_vec3(shader, "uSpecular", vec3(mat.specular));
        set_float(shader, "uShininess", mat.shininess);
        set_float(shader, "uMetallic", mat.metallic);
        set_float(shader, "uRoughness", mat.roughness);

        if (mat.backface_culling) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, mat.wireframe ? GL_LINE : GL_FILL);

        glBindVertexArray(mb.vao);
        glDrawElements(GL_TRIANGLES, mb.index_count, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_CULL_FACE);
    }
    for (auto* c : node->children) render_node(c, shader, view, proj);
}

void Renderer::render_selection_box(SceneNode* node, const mat4& view, const mat4& proj) {
    if (!node->mesh) return;
    vec3 bmin = node->get_bounding_box_min() - vec3(0.02f);
    vec3 bmax = node->get_bounding_box_max() + vec3(0.02f);
    vec3 corners[8] = {
        {bmin.x,bmin.y,bmin.z},{bmax.x,bmin.y,bmin.z},{bmax.x,bmax.y,bmin.z},{bmin.x,bmax.y,bmin.z},
        {bmin.x,bmin.y,bmax.z},{bmax.x,bmin.y,bmax.z},{bmax.x,bmax.y,bmax.z},{bmin.x,bmax.y,bmax.z}
    };
    int edges[] = {0,1,1,2,2,3,3,0,4,5,5,6,6,7,7,4,0,4,1,5,2,6,3,7};
    std::vector<float> data;
    for (int i = 0; i < 24; i++) {
        vec3 p = corners[edges[i]];
        data.insert(data.end(), {p.x, p.y, p.z, 1.0f, 1.0f, 0.0f, 1.0f});
    }
    uint32_t vao, vbo;
    glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    use_shader(shader_line);
    set_mat4(shader_line, "uMVP", proj * view);
    glDrawArrays(GL_LINES, 0, 24);
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo); glDeleteVertexArrays(1, &vao);
}

void Renderer::render_scene(Scene& scene, Camera& camera, int vp_x, int vp_y, int vp_w, int vp_h) {
    glViewport(vp_x, vp_y, vp_w, vp_h);
    glClearColor(scene.background_color.r, scene.background_color.g, scene.background_color.b, scene.background_color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (scene.grid_enabled) render_grid(scene.grid_size, scene.grid_subdivisions, camera.get_view_matrix(), camera.get_projection_matrix());

    mat4 view = camera.get_view_matrix();
    mat4 proj = camera.get_projection_matrix();
    use_shader(shader_3d);
    set_mat4(shader_3d, "uView", view);
    set_mat4(shader_3d, "uProjection", proj);
    set_vec3(shader_3d, "uCameraPos", camera.position);

    int light_count = 0;
    for (auto& l : scene.lights) {
        if (!l.enabled || light_count >= 8) continue;
        std::string prefix = "uLights[" + std::to_string(light_count) + "].";
        set_vec3(shader_3d, (prefix + "pos").c_str(), l.position);
        set_vec3(shader_3d, (prefix + "dir").c_str(), l.direction);
        set_vec3(shader_3d, (prefix + "col").c_str(), vec3(l.color) * l.intensity);
        set_int(shader_3d, (prefix + "type").c_str(), (int)l.type);
        set_float(shader_3d, (prefix + "range").c_str(), l.range);
        set_float(shader_3d, (prefix + "spotAngle").c_str(), l.spot_angle);
        set_float(shader_3d, (prefix + "spotExp").c_str(), l.spot_exponent);
        set_vec3(shader_3d, (prefix + "att").c_str(), l.attenuation);
        light_count++;
    }
    set_int(shader_3d, "uLightCount", light_count);

    for (auto* c : scene.root.children) render_node(c, shader_3d, view, proj);

    if (scene.selected_node) render_selection_box(scene.selected_node, view, proj);
}

uint32_t Renderer::read_pixel(int x, int y) {
    unsigned char pixel[4];
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    return (pixel[0] << 24) | (pixel[1] << 16) | (pixel[2] << 8) | pixel[3];
}

void Renderer::resize(int w, int h) { glViewport(0, 0, w, h); }

void Renderer::invalidate_mesh(Mesh* mesh) {
    uint32_t key = (uint32_t)(uintptr_t)mesh;
    auto it = mesh_cache.find(key);
    if (it != mesh_cache.end()) {
        glDeleteBuffers(1, &it->second.vbo);
        glDeleteBuffers(1, &it->second.ebo);
        glDeleteVertexArrays(1, &it->second.vao);
        mesh_cache.erase(it);
    }
}

void Renderer::cleanup() {
    glDeleteProgram(shader_3d); glDeleteProgram(shader_line); glDeleteProgram(shader_grid);
    glDeleteBuffers(1, &grid_vbo); glDeleteVertexArrays(1, &grid_vao);
    for (auto& [k, mb] : mesh_cache) {
        glDeleteBuffers(1, &mb.vbo); glDeleteBuffers(1, &mb.ebo); glDeleteVertexArrays(1, &mb.vao);
    }
    mesh_cache.clear();
}

} // namespace ocp
