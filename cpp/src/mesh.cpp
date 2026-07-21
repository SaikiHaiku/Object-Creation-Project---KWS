#include "mesh.h"
#include <cmath>
#include <map>
#include <set>

namespace ocp {

uint32_t Mesh::add_vertex(const vec3& pos, const vec3& n, const vec2& uv, const vec4& col) {
    uint32_t idx = (uint32_t)vertices.size();
    Vertex v; v.position = pos; v.normal = n; v.uv = uv; v.color = col;
    vertices.push_back(v);
    dirty = true;
    return idx;
}

uint32_t Mesh::add_face(const std::vector<uint32_t>& vi, const vec3& n) {
    uint32_t idx = (uint32_t)faces.size();
    Face f; f.vertices = vi; f.normal = n;
    faces.push_back(f);
    dirty = true;
    return idx;
}

void Mesh::compute_normals() {
    for (auto& v : vertices) v.normal = vec3(0);
    for (auto& f : faces) {
        if (f.vertices.size() < 3) continue;
        vec3 e1 = vertices[f.vertices[1]].position - vertices[f.vertices[0]].position;
        vec3 e2 = vertices[f.vertices[2]].position - vertices[f.vertices[0]].position;
        vec3 fn = glm::normalize(glm::cross(e1, e2));
        for (auto& vi : f.vertices) vertices[vi].normal += fn;
    }
    for (auto& v : vertices) {
        float len = glm::length(v.normal);
        if (len > 1e-6f) v.normal /= len;
        else v.normal = vec3(0, 1, 0);
    }
    dirty = true;
}

void Mesh::apply_transform(const mat4& m) {
    for (auto& v : vertices) {
        v.position = vec3(m * vec4(v.position, 1.0f));
        v.normal = glm::normalize(vec3(m * vec4(v.normal, 0.0f)));
    }
    dirty = true;
}

void Mesh::subdivide() {
    std::vector<Vertex> new_verts = vertices;
    std::vector<Face> new_faces;
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> mid_cache;

    auto get_midpoint = [&](uint32_t i0, uint32_t i1) -> uint32_t {
        if (i0 > i1) std::swap(i0, i1);
        auto key = std::make_pair(i0, i1);
        auto it = mid_cache.find(key);
        if (it != mid_cache.end()) return it->second;
        Vertex mid;
        mid.position = (vertices[i0].position + vertices[i1].position) * 0.5f;
        mid.normal = glm::normalize((vertices[i0].normal + vertices[i1].normal) * 0.5f);
        mid.uv = (vertices[i0].uv + vertices[i1].uv) * 0.5f;
        mid.color = (vertices[i0].color + vertices[i1].color) * 0.5f;
        uint32_t idx = (uint32_t)new_verts.size();
        new_verts.push_back(mid);
        mid_cache[key] = idx;
        return idx;
    };

    for (auto& f : faces) {
        if (f.vertices.size() == 3) {
            uint32_t a = f.vertices[0], b = f.vertices[1], c = f.vertices[2];
            uint32_t ab = get_midpoint(a, b);
            uint32_t bc = get_midpoint(b, c);
            uint32_t ca = get_midpoint(c, a);
            new_faces.push_back({{a, ab, ca}});
            new_faces.push_back({{b, bc, ab}});
            new_faces.push_back({{c, ca, bc}});
            new_faces.push_back({{ab, bc, ca}});
        } else if (f.vertices.size() == 4) {
            uint32_t a = f.vertices[0], b = f.vertices[1], c = f.vertices[2], d = f.vertices[3];
            uint32_t ab = get_midpoint(a, b);
            uint32_t bc = get_midpoint(b, c);
            uint32_t cd = get_midpoint(c, d);
            uint32_t da = get_midpoint(d, a);
            uint32_t center = (uint32_t)new_verts.size();
            Vertex cv;
            cv.position = (new_verts[a].position + new_verts[b].position + new_verts[c].position + new_verts[d].position) * 0.25f;
            cv.normal = glm::normalize((new_verts[a].normal + new_verts[b].normal + new_verts[c].normal + new_verts[d].normal) * 0.25f);
            cv.uv = (new_verts[a].uv + new_verts[b].uv + new_verts[c].uv + new_verts[d].uv) * 0.25f;
            cv.color = (new_verts[a].color + new_verts[b].color + new_verts[c].color + new_verts[d].color) * 0.25f;
            new_verts.push_back(cv);
            new_faces.push_back({{a, ab, center, da}});
            new_faces.push_back({{b, bc, center, ab}});
            new_faces.push_back({{c, cd, center, bc}});
            new_faces.push_back({{d, da, center, cd}});
        } else {
            new_faces.push_back(f);
        }
    }
    vertices = new_verts;
    faces = new_faces;
    compute_normals();
}

void Mesh::invert_faces() {
    for (auto& f : faces) std::reverse(f.vertices.begin(), f.vertices.end());
    for (auto& v : vertices) v.normal = -v.normal;
    dirty = true;
}

void Mesh::weld_vertices(float threshold) {
    std::vector<Vertex> new_verts;
    std::vector<uint32_t> remap(vertices.size());
    for (uint32_t i = 0; i < (uint32_t)vertices.size(); i++) {
        bool found = false;
        for (uint32_t j = 0; j < (uint32_t)new_verts.size(); j++) {
            if (glm::length(vertices[i].position - new_verts[j].position) < threshold) {
                remap[i] = j;
                found = true;
                break;
            }
        }
        if (!found) {
            remap[i] = (uint32_t)new_verts.size();
            new_verts.push_back(vertices[i]);
        }
    }
    for (auto& f : faces) {
        for (auto& vi : f.vertices) vi = remap[vi];
    }
    vertices = new_verts;
    dirty = true;
}

Mesh Mesh::clone() const {
    Mesh m;
    m.name = name + "_copy";
    m.vertices = vertices;
    m.faces = faces;
    m.dirty = true;
    return m;
}

void Mesh::clear() {
    vertices.clear();
    faces.clear();
    cached_vertex_data.clear();
    cached_index_data.clear();
    dirty = true;
}

int Mesh::get_triangle_count() const {
    int count = 0;
    for (auto& f : faces) {
        if (f.vertices.size() >= 3) count += (int)f.vertices.size() - 2;
    }
    return count;
}

int Mesh::get_index_count() const {
    int count = 0;
    for (auto& f : faces) count += (int)f.vertices.size();
    return count;
}

vec3 Mesh::get_bounding_box_min() const {
    if (vertices.empty()) return vec3(0);
    vec3 mn = vertices[0].position;
    for (auto& v : vertices) mn = glm::min(mn, v.position);
    return mn;
}

vec3 Mesh::get_bounding_box_max() const {
    if (vertices.empty()) return vec3(0);
    vec3 mx = vertices[0].position;
    for (auto& v : vertices) mx = glm::max(mx, v.position);
    return mx;
}

vec3 Mesh::get_center() const {
    return (get_bounding_box_min() + get_bounding_box_max()) * 0.5f;
}

void Mesh::rebuild_cache() {
    cached_vertex_data.clear();
    cached_index_data.clear();
    cached_vertex_data.reserve(vertices.size() * 12);
    for (auto& v : vertices) {
        cached_vertex_data.push_back(v.position.x);
        cached_vertex_data.push_back(v.position.y);
        cached_vertex_data.push_back(v.position.z);
        cached_vertex_data.push_back(v.normal.x);
        cached_vertex_data.push_back(v.normal.y);
        cached_vertex_data.push_back(v.normal.z);
        cached_vertex_data.push_back(v.uv.x);
        cached_vertex_data.push_back(v.uv.y);
        cached_vertex_data.push_back(v.color.r);
        cached_vertex_data.push_back(v.color.g);
        cached_vertex_data.push_back(v.color.b);
        cached_vertex_data.push_back(v.color.a);
    }
    for (auto& f : faces) {
        for (auto& vi : f.vertices) cached_index_data.push_back(vi);
    }
    dirty = false;
}

const float* Mesh::get_vertex_data() {
    if (dirty) rebuild_cache();
    return cached_vertex_data.data();
}

const uint32_t* Mesh::get_index_data() {
    if (dirty) rebuild_cache();
    return cached_index_data.data();
}

} // namespace ocp
