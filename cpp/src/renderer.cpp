#include "renderer.h"
#include "bmesh.h"
#include <glad/gl.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
uniform float uMetallic, uRoughness;
uniform int uLightCount;
uniform int uFlatShading;
struct Light { vec3 pos, dir, col, att; int type; float range, spotAngle, spotExp; };
uniform Light uLights[16];
out vec4 FragColor;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float r) {
    float a = r * r;
    float a2 = a * a;
    float NdH = max(dot(N, H), 0.0);
    float d = NdH * NdH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + 0.0001);
}

float GeometrySchlickGGX(float NdV, float r) {
    float k = (r + 1.0) * (r + 1.0) / 8.0;
    return NdV / (NdV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float r) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), r) *
           GeometrySchlickGGX(max(dot(N, L), 0.0), r);
}

vec3 fresnelSchlick(float cosT, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosT, 0.0, 1.0), 5.0);
}

vec3 acesToneMap(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    if (uFlatShading == 1) {
        FragColor = vec4(uDiffuse.rgb * vColor.rgb, uDiffuse.a * vColor.a);
        return;
    }
    vec3 albedo = uDiffuse.rgb * vColor.rgb;
    float metal = clamp(uMetallic, 0.0, 1.0);
    float rough = clamp(uRoughness, 0.04, 1.0);
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metal);
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < uLightCount && i < 16; i++) {
        Light L = uLights[i];
        vec3 Ld;
        float att = 1.0;
        if (L.type == 0) {
            vec3 diff = L.pos - vWorldPos;
            float d = length(diff);
            Ld = diff / d;
            att = 1.0 / (L.att.x + L.att.y * d + L.att.z * d * d);
            att *= clamp(1.0 - pow(d / max(L.range, 0.01), 4.0), 0.0, 1.0);
        } else if (L.type == 1) {
            Ld = normalize(-L.dir);
        } else if (L.type == 2) {
            vec3 diff = L.pos - vWorldPos;
            float d = length(diff);
            Ld = diff / d;
            att = 1.0 / (L.att.x + L.att.y * d + L.att.z * d * d);
            float ca = dot(Ld, normalize(-L.dir));
            if (ca < cos(radians(L.spotAngle))) att = 0.0;
            else att *= pow(ca, L.spotExp);
        } else {
            Ld = normalize(L.pos - vWorldPos);
        }
        vec3 radiance = L.col * att;
        vec3 H = normalize(Ld + V);
        float NdL = max(dot(N, Ld), 0.0);
        float D = DistributionGGX(N, H, rough);
        float G = GeometrySmith(N, V, Ld, rough);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 spec = (D * G * F) / (4.0 * max(dot(N, V), 0.0) * NdL + 0.0001);
        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metal);
        Lo += (kD * albedo / PI + spec) * radiance * NdL;
    }
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 skyColor = vec3(0.15, 0.18, 0.25);
    vec3 groundColor = vec3(0.08, 0.06, 0.05);
    float hemi = dot(N, up) * 0.5 + 0.5;
    vec3 hemiAmbient = mix(groundColor, skyColor, hemi);
    vec3 ambient = hemiAmbient * albedo * 0.35;
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    ambient *= mix(1.0, 0.6, fresnel * 0.5);
    vec3 R = reflect(-V, N);
    float envMip = rough * 4.0;
    vec3 envColor = mix(skyColor, vec3(0.5), 0.3) * (1.0 - metal * 0.7);
    vec3 envSpec = envColor * F0 * 0.15 * (1.0 - rough);
    Lo += envSpec * (1.0 - metal * 0.3);
    vec3 color = ambient + Lo;
    color = acesToneMap(color);
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, uDiffuse.a * vColor.a);
}
)";

static const char* VERT_GRID = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
uniform mat4 uVP;
out vec3 vWorldPos;
out vec4 vColor;
void main() {
    vWorldPos = aPos;
    vColor = aColor;
    gl_Position = uVP * vec4(aPos, 1.0);
}
)";

static const char* FRAG_GRID = R"(
#version 330 core
in vec3 vWorldPos;
in vec4 vColor;
uniform float uGridSize;
out vec4 FragColor;
void main() {
    float dist = length(vWorldPos.xz);
    float halfGrid = uGridSize * 0.5;
    float fade = 1.0 - smoothstep(halfGrid * 0.6, halfGrid, dist);
    FragColor = vec4(vColor.rgb, vColor.a * fade);
}
)";

static const char* VERT_LINE = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
uniform mat4 uMVP;
out vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* FRAG_LINE = R"(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)";

static const char* VERT_OUTLINE = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aColor;
uniform mat4 uModel, uView, uProjection;
out vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
)";

static const char* FRAG_OUTLINE = R"(
#version 330 core
in vec4 vColor;
uniform vec4 uOutlineColor;
out vec4 FragColor;
void main() {
    FragColor = uOutlineColor;
}
)";

static const char* VERT_SKYBOX = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uVP;
out vec3 vDir;
void main() {
    vDir = aPos;
    vec4 p = uVP * vec4(aPos, 1.0);
    gl_Position = p.xyww;
}
)";

static const char* FRAG_SKYBOX = R"(
#version 330 core
in vec3 vDir;
uniform vec3 uTopColor;
uniform vec3 uBottomColor;
out vec4 FragColor;
void main() {
    vec3 dir = normalize(vDir);
    float t = dir.y * 0.5 + 0.5;
    FragColor = vec4(mix(uBottomColor, uTopColor, t), 1.0);
}
)";

uint32_t Renderer::compile_shader(const char* vs_src, const char* fs_src) {
    auto compile = [](GLenum type, const char* src) -> uint32_t {
        uint32_t s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        int ok;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(s, sizeof(log), nullptr, log);
            fprintf(stderr, "Shader compile error: %s\n", log);
            glDeleteShader(s);
            return 0;
        }
        return s;
    };
    uint32_t vs = compile(GL_VERTEX_SHADER, vs_src);
    uint32_t fs = compile(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    uint32_t prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    int ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        fprintf(stderr, "Shader link error: %s\n", log);
        glDeleteProgram(prog);
        prog = 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

void Renderer::use_shader(uint32_t s) { if (s) glUseProgram(s); }

void Renderer::set_mat4(uint32_t s, const char* n, const mat4& m) {
    GLint loc = glGetUniformLocation(s, n);
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}
void Renderer::set_mat3(uint32_t s, const char* n, const mat3& m) {
    GLint loc = glGetUniformLocation(s, n);
    if (loc >= 0) glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}
void Renderer::set_vec3(uint32_t s, const char* n, const vec3& v) {
    GLint loc = glGetUniformLocation(s, n);
    if (loc >= 0) glUniform3fv(loc, 1, glm::value_ptr(v));
}
void Renderer::set_vec4(uint32_t s, const char* n, const vec4& v) {
    GLint loc = glGetUniformLocation(s, n);
    if (loc >= 0) glUniform4fv(loc, 1, glm::value_ptr(v));
}
void Renderer::set_float(uint32_t s, const char* n, float v) {
    GLint loc = glGetUniformLocation(s, n);
    if (loc >= 0) glUniform1f(loc, v);
}
void Renderer::set_int(uint32_t s, const char* n, int v) {
    GLint loc = glGetUniformLocation(s, n);
    if (loc >= 0) glUniform1i(loc, v);
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
    shader_outline = compile_shader(VERT_OUTLINE, FRAG_OUTLINE);
    shader_skybox = compile_shader(VERT_SKYBOX, FRAG_SKYBOX);

    last_grid_size = 20.0f;
    last_grid_subdiv = 20;
    build_grid(last_grid_size, last_grid_subdiv);

    glGenVertexArrays(1, &sel_vao);
    glGenBuffers(1, &sel_vbo);
    sel_vertex_count = 0;

    glGenVertexArrays(1, &gizmo_vao);
    glGenBuffers(1, &gizmo_vbo);
    gizmo_vertex_count = 0;

    glGenVertexArrays(1, &edit_vao);
    glGenBuffers(1, &edit_vbo);
    edit_vertex_count = 0;

    glGenVertexArrays(1, &sculpt_cursor_vao);
    glGenBuffers(1, &sculpt_cursor_vbo);
    sculpt_cursor_vertex_count = 0;

    glGenFramebuffers(1, &outline_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, outline_fbo);
    glGenTextures(1, &outline_tex);
    glBindTexture(GL_TEXTURE_2D, outline_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outline_tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::resize(int w, int h) {
    glViewport(0, 0, w, h);
    if (outline_tex) {
        glBindTexture(GL_TEXTURE_2D, outline_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
}

void Renderer::build_grid(float size, int subdiv) {
    std::vector<float> data;
    float step = size / subdiv;
    float hs = size * 0.5f;
    for (int i = 0; i <= subdiv; i++) {
        float pos = -hs + i * step;
        bool is_axis = (fabsf(pos) < 0.05f);
        float alpha = is_axis ? 0.9f : 0.45f;
        float r = is_axis ? 0.45f : 0.32f;
        float g = is_axis ? 0.45f : 0.32f;
        float b = is_axis ? 0.65f : 0.32f;
        data.insert(data.end(), {pos, 0, -hs, r, g, b, alpha});
        data.insert(data.end(), {pos, 0, hs, r, g, b, alpha * 0.15f});
        data.insert(data.end(), {-hs, 0, pos, r, g, b, alpha});
        data.insert(data.end(), {hs, 0, pos, r, g, b, alpha * 0.15f});
    }
    grid_vertex_count = (int)data.size() / 7;
    if (grid_vao == 0) glGenVertexArrays(1, &grid_vao);
    if (grid_vbo == 0) glGenBuffers(1, &grid_vbo);
    glBindVertexArray(grid_vao);
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

Renderer::MeshBuffers Renderer::get_or_create_buffers(Mesh* mesh) {
    auto it = mesh_cache.find(mesh);
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
    if (vc == 0 || ic == 0) return mb;
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
    mesh_cache[mesh] = mb;
    mesh->dirty = false;
    return mb;
}

void Renderer::render_node(SceneNode* node, uint32_t shader, const mat4& view, const mat4& proj, bool global_wire, bool xray) {
    if (!node || !node->visible) return;
    if (node->mesh && node->material && node->mesh->get_vertex_count() > 0) {
        MeshBuffers mb = get_or_create_buffers(node->mesh.get());
        if (mb.vao == 0) return;
        set_mat4(shader, "uModel", node->get_world_matrix());
        set_mat3(shader, "uNormalMatrix", mat3_normal_from_mat4(node->get_world_matrix()));
        Material& mat = *node->material;
        set_vec4(shader, "uDiffuse", mat.diffuse);
        set_float(shader, "uMetallic", mat.metallic);
        set_float(shader, "uRoughness", mat.roughness);
        set_int(shader, "uFlatShading", 0);
        bool do_wireframe = mat.wireframe || global_wire;
        if (xray) { glDisable(GL_CULL_FACE); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
        if (mat.backface_culling && !xray) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, do_wireframe ? GL_LINE : GL_FILL);
        glBindVertexArray(mb.vao);
        glDrawElements(GL_TRIANGLES, mb.index_count, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        draw_calls++;
        triangles_rendered += mb.index_count / 3;
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_CULL_FACE);
        if (xray) glDisable(GL_BLEND);
    }
    for (auto& c : node->children) render_node(c.get(), shader, view, proj, global_wire, xray);
}

void Renderer::render_selection_box(SceneNode* node, const mat4& view, const mat4& proj) {
    if (!node || !node->mesh) return;
    vec3 bmin = node->get_bounding_box_min() - vec3(0.03f);
    vec3 bmax = node->get_bounding_box_max() + vec3(0.03f);
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
    sel_vertex_count = 24;
    glBindVertexArray(sel_vao);
    glBindBuffer(GL_ARRAY_BUFFER, sel_vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    use_shader(shader_line);
    set_mat4(shader_line, "uMVP", proj * view);
    glDrawArrays(GL_LINES, 0, sel_vertex_count);
    glBindVertexArray(0);
    draw_calls++;
}

void Renderer::render_selection_outline(SceneNode* node, const mat4& view, const mat4& proj, const Camera& camera) {
    if (!node || !node->mesh || node->mesh->get_vertex_count() == 0) return;
    MeshBuffers mb = get_or_create_buffers(node->mesh.get());
    if (mb.vao == 0) return;
    mat4 model = glm::scale(node->get_world_matrix(), vec3(1.06f));
    use_shader(shader_outline);
    set_mat4(shader_outline, "uModel", model);
    set_mat4(shader_outline, "uView", view);
    set_mat4(shader_outline, "uProjection", proj);
    set_vec4(shader_outline, "uOutlineColor", vec4(1.0f, 0.55f, 0.0f, 1.0f));
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(mb.vao);
    glDrawElements(GL_TRIANGLES, mb.index_count, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    draw_calls++;
    triangles_rendered += mb.index_count / 3;
    glEnable(GL_CULL_FACE);
}

void Renderer::render_scene(Scene& scene, Camera& camera, int vp_x, int vp_y, int vp_w, int vp_h) {
    auto frame_start = std::chrono::high_resolution_clock::now();
    draw_calls = 0;
    triangles_rendered = 0;

    glViewport(vp_x, vp_y, vp_w, vp_h);
    glClearColor(scene.background_color.r, scene.background_color.g, scene.background_color.b, scene.background_color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4 view = camera.get_view_matrix();
    mat4 proj = camera.get_projection_matrix();

    if (scene.grid_enabled) {
        if (scene.grid_size != last_grid_size || scene.grid_subdivisions != last_grid_subdiv) {
            last_grid_size = scene.grid_size;
            last_grid_subdiv = scene.grid_subdivisions;
            build_grid(scene.grid_size, scene.grid_subdivisions);
        }
        use_shader(shader_grid);
        set_mat4(shader_grid, "uVP", proj * view);
        set_float(shader_grid, "uGridSize", scene.grid_size);
        glBindVertexArray(grid_vao);
        glDrawArrays(GL_LINES, 0, grid_vertex_count);
        glBindVertexArray(0);
        draw_calls++;
    }

    use_shader(shader_3d);
    set_mat4(shader_3d, "uView", view);
    set_mat4(shader_3d, "uProjection", proj);
    set_vec3(shader_3d, "uCameraPos", camera.position);
    set_int(shader_3d, "uFlatShading", 0);

    int light_count = 0;
    for (auto& l : scene.lights) {
        if (!l.enabled || light_count >= 16) continue;
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

    for (auto& c : scene.root.children)
        render_node(c.get(), shader_3d, view, proj, scene.global_wireframe, scene.xray_mode);

    for (auto* sel : scene.multi_selection)
        render_selection_box(sel, view, proj);

    if (!scene.multi_selection.empty()) {
        glBindFramebuffer(GL_FRAMEBUFFER, outline_fbo);
        glViewport(0, 0, vp_w, vp_h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        for (auto* sel : scene.multi_selection)
            render_selection_outline(sel, view, proj, camera);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(vp_x, vp_y, vp_w, vp_h);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        use_shader(shader_outline);
        set_mat4(shader_outline, "uView", view);
        set_mat4(shader_outline, "uProjection", proj);
        set_vec4(shader_outline, "uOutlineColor", vec4(1.0f, 0.55f, 0.0f, 1.0f));
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        for (auto* sel : scene.multi_selection) {
            if (!sel || !sel->mesh || sel->mesh->get_vertex_count() == 0) continue;
            MeshBuffers mb = get_or_create_buffers(sel->mesh.get());
            if (mb.vao == 0) continue;
            mat4 model = glm::scale(sel->get_world_matrix(), vec3(1.06f));
            set_mat4(shader_outline, "uModel", model);
            glBindVertexArray(mb.vao);
            glDrawElements(GL_TRIANGLES, mb.index_count, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
            draw_calls++;
            triangles_rendered += mb.index_count / 3;
        }

        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }

    auto frame_end = std::chrono::high_resolution_clock::now();
    frame_time_ms = std::chrono::duration<float, std::milli>(frame_end - frame_start).count();
}

void Renderer::render_gizmo(SceneNode* node, int tool, const mat4& view, const mat4& proj, const Camera& camera) {
    if (!node) return;
    vec3 center = node->get_world_position();
    std::vector<float> data;
    float len = 1.5f;

    if (tool == 0) {
        auto add_arrow = [&](const vec3& dir, const vec3& col) {
            vec3 end = center + dir * len;
            data.insert(data.end(), {center.x, center.y, center.z, col.r, col.g, col.b, 1.0f});
            data.insert(data.end(), {end.x, end.y, end.z, col.r, col.g, col.b, 1.0f});
            vec3 perp1 = fabsf(glm::dot(dir, vec3(0,1,0))) < 0.9f
                ? glm::normalize(glm::cross(dir, vec3(0,1,0)))
                : glm::normalize(glm::cross(dir, vec3(1,0,0)));
            vec3 perp2 = glm::cross(dir, perp1);
            float cone_len = 0.22f;
            float cone_r = 0.07f;
            int segs = 12;
            vec3 cone_base = end - dir * cone_len;
            for (int i = 0; i < segs; i++) {
                float a0 = (float)i / segs * 2.0f * (float)M_PI;
                float a1 = (float)(i + 1) / segs * 2.0f * (float)M_PI;
                vec3 p0 = cone_base + (perp1 * cosf(a0) + perp2 * sinf(a0)) * cone_r;
                vec3 p1 = cone_base + (perp1 * cosf(a1) + perp2 * sinf(a1)) * cone_r;
                data.insert(data.end(), {p0.x, p0.y, p0.z, col.r, col.g, col.b, 1.0f});
                data.insert(data.end(), {p1.x, p1.y, p1.z, col.r, col.g, col.b, 1.0f});
                data.insert(data.end(), {p1.x, p1.y, p1.z, col.r, col.g, col.b, 1.0f});
                data.insert(data.end(), {end.x, end.y, end.z, col.r, col.g, col.b, 1.0f});
            }
            for (int i = 0; i < segs; i++) {
                float a0 = (float)i / segs * 2.0f * (float)M_PI;
                float a1 = (float)(i + 1) / segs * 2.0f * (float)M_PI;
                vec3 p0 = cone_base + (perp1 * cosf(a0) + perp2 * sinf(a0)) * cone_r;
                vec3 p1 = cone_base + (perp1 * cosf(a1) + perp2 * sinf(a1)) * cone_r;
                data.insert(data.end(), {p0.x, p0.y, p0.z, col.r, col.g, col.b, 0.7f});
                data.insert(data.end(), {p1.x, p1.y, p1.z, col.r, col.g, col.b, 0.7f});
            }
        };
        add_arrow(vec3(1,0,0), vec3(1.0f, 0.2f, 0.2f));
        add_arrow(vec3(0,1,0), vec3(0.2f, 1.0f, 0.2f));
        add_arrow(vec3(0,0,1), vec3(0.2f, 0.2f, 1.0f));
    } else if (tool == 1) {
        auto add_ring = [&](const vec3& axis, const vec3& col) {
            int segs = 48;
            float radius = len * 0.8f;
            vec3 perp1 = fabsf(glm::dot(axis, vec3(0,1,0))) < 0.9f
                ? glm::normalize(glm::cross(axis, vec3(0,1,0)))
                : glm::normalize(glm::cross(axis, vec3(1,0,0)));
            vec3 perp2 = glm::cross(axis, perp1);
            for (int i = 0; i < segs; i++) {
                if (i % 4 == 0) continue;
                float a0 = (float)i / segs * 2.0f * (float)M_PI;
                float a1 = (float)(i + 1) / segs * 2.0f * (float)M_PI;
                vec3 p0 = center + (perp1 * cosf(a0) + perp2 * sinf(a0)) * radius;
                vec3 p1 = center + (perp1 * cosf(a1) + perp2 * sinf(a1)) * radius;
                data.insert(data.end(), {p0.x, p0.y, p0.z, col.r, col.g, col.b, 0.85f});
                data.insert(data.end(), {p1.x, p1.y, p1.z, col.r, col.g, col.b, 0.85f});
            }
            for (int d = 0; d < 4; d++) {
                float a = (float)d / 4.0f * 2.0f * (float)M_PI;
                vec3 p = center + (perp1 * cosf(a) + perp2 * sinf(a)) * radius;
                vec3 outward = glm::normalize(p - center);
                vec3 tick_inner = p - outward * 0.06f;
                vec3 tick_outer = p + outward * 0.06f;
                data.insert(data.end(), {tick_inner.x, tick_inner.y, tick_inner.z, col.r, col.g, col.b, 1.0f});
                data.insert(data.end(), {tick_outer.x, tick_outer.y, tick_outer.z, col.r, col.g, col.b, 1.0f});
            }
        };
        add_ring(vec3(1,0,0), vec3(1.0f, 0.3f, 0.3f));
        add_ring(vec3(0,1,0), vec3(0.3f, 1.0f, 0.3f));
        add_ring(vec3(0,0,1), vec3(0.3f, 0.3f, 1.0f));
    } else if (tool == 2) {
        auto add_scale = [&](const vec3& axis, const vec3& col) {
            vec3 end = center + axis * len;
            data.insert(data.end(), {center.x, center.y, center.z, col.r, col.g, col.b, 1.0f});
            data.insert(data.end(), {end.x, end.y, end.z, col.r, col.g, col.b, 1.0f});
            vec3 perp1 = fabsf(glm::dot(axis, vec3(0,1,0))) < 0.9f
                ? glm::normalize(glm::cross(axis, vec3(0,1,0)))
                : glm::normalize(glm::cross(axis, vec3(1,0,0)));
            vec3 perp2 = glm::cross(axis, perp1);
            float hs = 0.08f;
            vec3 corners2[8];
            for (int i = 0; i < 8; i++) {
                corners2[i] = end + perp1 * ((i & 1) ? hs : -hs) + perp2 * ((i & 2) ? hs : -hs) + axis * ((i & 4) ? hs : -hs);
            }
            int edges2[] = {0,1,1,3,3,2,2,0,4,5,5,7,7,6,6,4,0,4,1,5,3,7,2,6};
            for (int i = 0; i < 24; i++) {
                data.insert(data.end(), {corners2[edges2[i]].x, corners2[edges2[i]].y, corners2[edges2[i]].z, col.r, col.g, col.b, 0.9f});
            }
        };
        add_scale(vec3(1,0,0), vec3(1.0f, 0.3f, 0.3f));
        add_scale(vec3(0,1,0), vec3(0.3f, 1.0f, 0.3f));
        add_scale(vec3(0,0,1), vec3(0.3f, 0.3f, 1.0f));
    }

    if (data.empty()) return;
    gizmo_vertex_count = (int)data.size() / 7;
    glBindVertexArray(gizmo_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gizmo_vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glDisable(GL_DEPTH_TEST);
    use_shader(shader_line);
    set_mat4(shader_line, "uMVP", proj * view);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, gizmo_vertex_count);
    glLineWidth(1.0f);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::render_manipulator_gizmo(SceneNode* node, int tool, const mat4& view, const mat4& proj, const Camera& camera, float snap) {
    if (!node) return;
    vec3 center = node->get_world_position();
    std::vector<float> data;
    float len = 1.5f;

    if (tool == 0) {
        auto add_arrow = [&](const vec3& dir, const vec3& col) {
            vec3 end = center + dir * len;
            data.insert(data.end(), {center.x, center.y, center.z, col.r, col.g, col.b, 1.0f});
            data.insert(data.end(), {end.x, end.y, end.z, col.r, col.g, col.b, 1.0f});
            vec3 perp1 = fabsf(glm::dot(dir, vec3(0,1,0))) < 0.9f
                ? glm::normalize(glm::cross(dir, vec3(0,1,0)))
                : glm::normalize(glm::cross(dir, vec3(1,0,0)));
            vec3 perp2 = glm::cross(dir, perp1);
            float cone_len = 0.22f;
            float cone_r = 0.07f;
            int segs = 12;
            vec3 cone_base = end - dir * cone_len;
            for (int i = 0; i < segs; i++) {
                float a0 = (float)i / segs * 2.0f * (float)M_PI;
                float a1 = (float)(i + 1) / segs * 2.0f * (float)M_PI;
                vec3 p0 = cone_base + (perp1 * cosf(a0) + perp2 * sinf(a0)) * cone_r;
                vec3 p1 = cone_base + (perp1 * cosf(a1) + perp2 * sinf(a1)) * cone_r;
                data.insert(data.end(), {p0.x, p0.y, p0.z, col.r, col.g, col.b, 1.0f});
                data.insert(data.end(), {p1.x, p1.y, p1.z, col.r, col.g, col.b, 1.0f});
                data.insert(data.end(), {p1.x, p1.y, p1.z, col.r, col.g, col.b, 1.0f});
                data.insert(data.end(), {end.x, end.y, end.z, col.r, col.g, col.b, 1.0f});
            }
            for (int i = 0; i < segs; i++) {
                float a0 = (float)i / segs * 2.0f * (float)M_PI;
                float a1 = (float)(i + 1) / segs * 2.0f * (float)M_PI;
                vec3 p0 = cone_base + (perp1 * cosf(a0) + perp2 * sinf(a0)) * cone_r;
                vec3 p1 = cone_base + (perp1 * cosf(a1) + perp2 * sinf(a1)) * cone_r;
                data.insert(data.end(), {p0.x, p0.y, p0.z, col.r, col.g, col.b, 0.7f});
                data.insert(data.end(), {p1.x, p1.y, p1.z, col.r, col.g, col.b, 0.7f});
            }
            if (snap > 0.0f) {
                int tick_count = (int)(len / snap);
                for (int t = 1; t <= tick_count && t <= 20; t++) {
                    vec3 tp = center + dir * (snap * t);
                    vec3 th1 = tp - perp1 * 0.04f;
                    vec3 th2 = tp + perp1 * 0.04f;
                    data.insert(data.end(), {th1.x, th1.y, th1.z, col.r, col.g, col.b, 0.5f});
                    data.insert(data.end(), {th2.x, th2.y, th2.z, col.r, col.g, col.b, 0.5f});
                }
            }
        };
        add_arrow(vec3(1,0,0), vec3(1.0f, 0.2f, 0.2f));
        add_arrow(vec3(0,1,0), vec3(0.2f, 1.0f, 0.2f));
        add_arrow(vec3(0,0,1), vec3(0.2f, 0.2f, 1.0f));
    } else if (tool == 1) {
        auto add_ring = [&](const vec3& axis, const vec3& col) {
            int segs = 48;
            float radius = len * 0.8f;
            vec3 perp1 = fabsf(glm::dot(axis, vec3(0,1,0))) < 0.9f
                ? glm::normalize(glm::cross(axis, vec3(0,1,0)))
                : glm::normalize(glm::cross(axis, vec3(1,0,0)));
            vec3 perp2 = glm::cross(axis, perp1);
            for (int i = 0; i < segs; i++) {
                if (i % 4 == 0) continue;
                float a0 = (float)i / segs * 2.0f * (float)M_PI;
                float a1 = (float)(i + 1) / segs * 2.0f * (float)M_PI;
                vec3 p0 = center + (perp1 * cosf(a0) + perp2 * sinf(a0)) * radius;
                vec3 p1 = center + (perp1 * cosf(a1) + perp2 * sinf(a1)) * radius;
                data.insert(data.end(), {p0.x, p0.y, p0.z, col.r, col.g, col.b, 0.85f});
                data.insert(data.end(), {p1.x, p1.y, p1.z, col.r, col.g, col.b, 0.85f});
            }
            for (int d = 0; d < 4; d++) {
                float a = (float)d / 4.0f * 2.0f * (float)M_PI;
                vec3 p = center + (perp1 * cosf(a) + perp2 * sinf(a)) * radius;
                vec3 outward = glm::normalize(p - center);
                vec3 tick_inner = p - outward * 0.06f;
                vec3 tick_outer = p + outward * 0.06f;
                data.insert(data.end(), {tick_inner.x, tick_inner.y, tick_inner.z, col.r, col.g, col.b, 1.0f});
                data.insert(data.end(), {tick_outer.x, tick_outer.y, tick_outer.z, col.r, col.g, col.b, 1.0f});
            }
            if (snap > 0.0f) {
                float snap_rad = deg2rad(snap);
                int count = (int)(2.0f * (float)M_PI / snap_rad);
                for (int t = 1; t <= count && t <= 72; t++) {
                    float a = snap_rad * t;
                    vec3 tp = center + (perp1 * cosf(a) + perp2 * sinf(a)) * radius;
                    vec3 outward = glm::normalize(tp - center);
                    vec3 ti = tp - outward * 0.03f;
                    vec3 to = tp + outward * 0.03f;
                    data.insert(data.end(), {ti.x, ti.y, ti.z, col.r, col.g, col.b, 0.6f});
                    data.insert(data.end(), {to.x, to.y, to.z, col.r, col.g, col.b, 0.6f});
                }
            }
        };
        add_ring(vec3(1,0,0), vec3(1.0f, 0.3f, 0.3f));
        add_ring(vec3(0,1,0), vec3(0.3f, 1.0f, 0.3f));
        add_ring(vec3(0,0,1), vec3(0.3f, 0.3f, 1.0f));
    } else if (tool == 2) {
        auto add_scale = [&](const vec3& axis, const vec3& col) {
            vec3 end = center + axis * len;
            data.insert(data.end(), {center.x, center.y, center.z, col.r, col.g, col.b, 1.0f});
            data.insert(data.end(), {end.x, end.y, end.z, col.r, col.g, col.b, 1.0f});
            vec3 perp1 = fabsf(glm::dot(axis, vec3(0,1,0))) < 0.9f
                ? glm::normalize(glm::cross(axis, vec3(0,1,0)))
                : glm::normalize(glm::cross(axis, vec3(1,0,0)));
            vec3 perp2 = glm::cross(axis, perp1);
            float hs = 0.08f;
            vec3 corners2[8];
            for (int i = 0; i < 8; i++) {
                corners2[i] = end + perp1 * ((i & 1) ? hs : -hs) + perp2 * ((i & 2) ? hs : -hs) + axis * ((i & 4) ? hs : -hs);
            }
            int edges2[] = {0,1,1,3,3,2,2,0,4,5,5,7,7,6,6,4,0,4,1,5,3,7,2,6};
            for (int i = 0; i < 24; i++) {
                data.insert(data.end(), {corners2[edges2[i]].x, corners2[edges2[i]].y, corners2[edges2[i]].z, col.r, col.g, col.b, 0.9f});
            }
            if (snap > 0.0f) {
                int tick_count = (int)(len / snap);
                for (int t = 1; t <= tick_count && t <= 20; t++) {
                    vec3 tp = center + axis * (snap * t);
                    vec3 th1 = tp - perp1 * 0.03f;
                    vec3 th2 = tp + perp1 * 0.03f;
                    vec3 th3 = tp - perp2 * 0.03f;
                    vec3 th4 = tp + perp2 * 0.03f;
                    data.insert(data.end(), {th1.x, th1.y, th1.z, col.r, col.g, col.b, 0.5f});
                    data.insert(data.end(), {th2.x, th2.y, th2.z, col.r, col.g, col.b, 0.5f});
                    data.insert(data.end(), {th3.x, th3.y, th3.z, col.r, col.g, col.b, 0.5f});
                    data.insert(data.end(), {th4.x, th4.y, th4.z, col.r, col.g, col.b, 0.5f});
                }
            }
        };
        add_scale(vec3(1,0,0), vec3(1.0f, 0.3f, 0.3f));
        add_scale(vec3(0,1,0), vec3(0.3f, 1.0f, 0.3f));
        add_scale(vec3(0,0,1), vec3(0.3f, 0.3f, 1.0f));
    }

    if (data.empty()) return;
    gizmo_vertex_count = (int)data.size() / 7;
    glBindVertexArray(gizmo_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gizmo_vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glDisable(GL_DEPTH_TEST);
    use_shader(shader_line);
    set_mat4(shader_line, "uMVP", proj * view);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, gizmo_vertex_count);
    glLineWidth(1.0f);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

uint32_t Renderer::read_pixel(int x, int y) {
    unsigned char pixel[4];
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    return (pixel[0] << 24) | (pixel[1] << 16) | (pixel[2] << 8) | pixel[3];
}

void Renderer::invalidate_mesh(Mesh* mesh) {
    auto it = mesh_cache.find(mesh);
    if (it != mesh_cache.end()) {
        glDeleteBuffers(1, &it->second.vbo);
        glDeleteBuffers(1, &it->second.ebo);
        glDeleteVertexArrays(1, &it->second.vao);
        mesh_cache.erase(it);
    }
}

void Renderer::cleanup() {
    if (shader_3d) glDeleteProgram(shader_3d);
    if (shader_line) glDeleteProgram(shader_line);
    if (shader_grid) glDeleteProgram(shader_grid);
    if (shader_outline) glDeleteProgram(shader_outline);
    if (shader_skybox) glDeleteProgram(shader_skybox);
    glDeleteBuffers(1, &grid_vbo); glDeleteVertexArrays(1, &grid_vao);
    glDeleteBuffers(1, &sel_vbo); glDeleteVertexArrays(1, &sel_vao);
    glDeleteBuffers(1, &gizmo_vbo); glDeleteVertexArrays(1, &gizmo_vao);
    glDeleteBuffers(1, &edit_vbo); glDeleteVertexArrays(1, &edit_vao);
    glDeleteBuffers(1, &sculpt_cursor_vbo); glDeleteVertexArrays(1, &sculpt_cursor_vao);
    if (outline_fbo) glDeleteFramebuffers(1, &outline_fbo);
    if (outline_tex) glDeleteTextures(1, &outline_tex);
    for (auto& [k, mb] : mesh_cache) {
        glDeleteBuffers(1, &mb.vbo); glDeleteBuffers(1, &mb.ebo); glDeleteVertexArrays(1, &mb.vao);
    }
    mesh_cache.clear();
}

void Renderer::render_edit_overlays(const BMesh& bm, const mat4& model, const mat4& view, const mat4& proj,
                                    int select_mode, const Camera& camera) {
    mat4 mvp = proj * view * model;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    if (select_mode == 0) {
        std::vector<float> data;
        float pt_size = std::max(3.0f, 6.0f / glm::length(camera.position - camera.target));
        for (int i = 0; i < bm.vert_count; ++i) {
            BMVert* v = bm.verts[i];
            if (v->hide) continue;
            vec4 wp = model * vec4(v->co, 1.0f);
            float r = v->select ? 1.0f : 0.2f;
            float g = v->select ? 0.5f : 0.6f;
            float b = v->select ? 0.0f : 1.0f;
            data.insert(data.end(), {wp.x, wp.y, wp.z, r, g, b, 1.0f});
        }
        if (!data.empty()) {
            edit_vertex_count = (int)data.size() / 7;
            glBindVertexArray(edit_vao);
            glBindBuffer(GL_ARRAY_BUFFER, edit_vbo);
            glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            use_shader(shader_line);
            set_mat4(shader_line, "uMVP", mat4(1.0f));
            glPointSize(pt_size);
            glDrawArrays(GL_POINTS, 0, edit_vertex_count);
            glBindVertexArray(0);
            draw_calls++;
        }
    }

    if (select_mode == 0 || select_mode == 1) {
        std::vector<float> data;
        for (int i = 0; i < bm.edge_count; ++i) {
            BMEdge* e = bm.edges[i];
            if (e->hide) continue;
            if (e->v1->hide || e->v2->hide) continue;
            vec4 p1 = model * vec4(e->v1->co, 1.0f);
            vec4 p2 = model * vec4(e->v2->co, 1.0f);
            float r = e->select ? 1.0f : 0.3f;
            float g = e->select ? 0.5f : 0.5f;
            float b = e->select ? 0.0f : 0.7f;
            float a = e->select ? 1.0f : 0.5f;
            data.insert(data.end(), {p1.x, p1.y, p1.z, r, g, b, a});
            data.insert(data.end(), {p2.x, p2.y, p2.z, r, g, b, a});
        }
        if (!data.empty()) {
            edit_vertex_count = (int)data.size() / 7;
            glBindVertexArray(edit_vao);
            glBindBuffer(GL_ARRAY_BUFFER, edit_vbo);
            glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            use_shader(shader_line);
            set_mat4(shader_line, "uMVP", mat4(1.0f));
            glLineWidth(1.5f);
            glDrawArrays(GL_LINES, 0, edit_vertex_count);
            glLineWidth(1.0f);
            glBindVertexArray(0);
            draw_calls++;
        }
    }

    if (select_mode == 2) {
        std::vector<float> data;
        for (int i = 0; i < bm.face_count; ++i) {
            BMFace* f = bm.faces[i];
            if (f->hide) continue;
            if (!f->select) continue;
            for (int j = 0; j < f->len; ++j) {
                int k = (j + 1) % f->len;
                vec4 p1 = model * vec4(f->verts[j]->co, 1.0f);
                vec4 p2 = model * vec4(f->verts[k]->co, 1.0f);
                data.insert(data.end(), {p1.x, p1.y, p1.z, 1.0f, 0.5f, 0.0f, 1.0f});
                data.insert(data.end(), {p2.x, p2.y, p2.z, 1.0f, 0.5f, 0.0f, 1.0f});
            }
        }
        if (!data.empty()) {
            edit_vertex_count = (int)data.size() / 7;
            glBindVertexArray(edit_vao);
            glBindBuffer(GL_ARRAY_BUFFER, edit_vbo);
            glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            use_shader(shader_line);
            set_mat4(shader_line, "uMVP", mat4(1.0f));
            glLineWidth(2.0f);
            glDrawArrays(GL_LINES, 0, edit_vertex_count);
            glLineWidth(1.0f);
            glBindVertexArray(0);
            draw_calls++;
        }
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void Renderer::render_sculpt_cursor(const vec3& world_pos, float radius, const mat4& view, const mat4& proj) {
    std::vector<float> data;
    int segs = 64;
    for (int i = 0; i < segs; ++i) {
        float a0 = (float)i / segs * 2.0f * (float)M_PI;
        float a1 = (float)(i + 1) / segs * 2.0f * (float)M_PI;
        vec3 p0 = world_pos + vec3(cosf(a0) * radius, 0.0f, sinf(a0) * radius);
        vec3 p1 = world_pos + vec3(cosf(a1) * radius, 0.0f, sinf(a1) * radius);
        data.insert(data.end(), {p0.x, p0.y, p0.z, 0.0f, 1.0f, 0.5f, 0.8f});
        data.insert(data.end(), {p1.x, p1.y, p1.z, 0.0f, 1.0f, 0.5f, 0.8f});
    }
    if (data.empty()) return;
    sculpt_cursor_vertex_count = (int)data.size() / 7;
    glBindVertexArray(sculpt_cursor_vao);
    glBindBuffer(GL_ARRAY_BUFFER, sculpt_cursor_vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glDisable(GL_DEPTH_TEST);
    use_shader(shader_line);
    set_mat4(shader_line, "uMVP", proj * view);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, sculpt_cursor_vertex_count);
    glLineWidth(1.0f);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    draw_calls++;
}

} // namespace ocp
