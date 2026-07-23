#include "mesh.h"
#include "bmesh.h"
#include <cmath>
#include <map>
#include <set>
#include <algorithm>
#include <array>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ocp {

struct SpatialHash {
    float cell_size;
    std::unordered_map<int64_t, std::vector<uint32_t>> grid;
    explicit SpatialHash(float cs) : cell_size(cs) {}
    static int64_t hash_cell(int x, int y, int z) {
        int64_t h = (int64_t)x * 73856093LL;
        h ^= (int64_t)y * 19349669LL;
        h ^= (int64_t)z * 83492791LL;
        return h;
    }
    void insert(uint32_t idx, const vec3& p) {
        int x = (int)std::floor(p.x / cell_size);
        int y = (int)std::floor(p.y / cell_size);
        int z = (int)std::floor(p.z / cell_size);
        grid[hash_cell(x, y, z)].push_back(idx);
    }
    void query(const vec3& p, std::vector<uint32_t>& out) const {
        int cx = (int)std::floor(p.x / cell_size);
        int cy = (int)std::floor(p.y / cell_size);
        int cz = (int)std::floor(p.z / cell_size);
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
                for (int dz = -1; dz <= 1; ++dz) {
                    auto it = grid.find(hash_cell(cx + dx, cy + dy, cz + dz));
                    if (it != grid.end())
                        out.insert(out.end(), it->second.begin(), it->second.end());
                }
    }
};

uint32_t Mesh::add_vertex(const vec3& pos, const vec3& n, const vec2& uv, const vec4& col) {
    uint32_t idx = (uint32_t)vertices.size();
    Vertex v; v.position = pos; v.normal = n; v.uv = uv; v.color = col;
    vertices.push_back(v);
    dirty = true;
    return idx;
}

uint32_t Mesh::add_vertex_spherical_uv(const vec3& pos, const vec4& col) {
    float r = glm::length(pos);
    float theta = (r > 1e-8f) ? std::atan2(pos.z, pos.x) : 0.0f;
    float phi   = (r > 1e-8f) ? std::acos(clamp(pos.y / r, -1.0f, 1.0f)) : 0.0f;
    vec2 uv(theta / (2.0f * (float)M_PI) + 0.5f, phi / (float)M_PI);
    vec3 n = (r > 1e-8f) ? pos / r : vec3(0, 1, 0);
    return add_vertex(pos, n, uv, col);
}

uint32_t Mesh::add_vertex_height_color(const vec3& pos, float h_min, float h_max,
                                       const vec3& n) {
    float t = clamp((pos.y - h_min) / (h_max - h_min), 0.0f, 1.0f);
    vec4 col;
    if (t < 0.5f)
        col = vec4(0.0f, t * 2.0f, 1.0f - t * 2.0f, 1.0f);
    else
        col = vec4((t - 0.5f) * 2.0f, 1.0f - (t - 0.5f) * 2.0f, 0.0f, 1.0f);
    return add_vertex(pos, n, vec2(0), col);
}

uint32_t Mesh::add_face(const std::vector<uint32_t>& vi, const vec3& n) {
    uint32_t idx = (uint32_t)faces.size();
    Face f; f.vertices = vi; f.normal = n;
    faces.push_back(f);
    dirty = true;
    return idx;
}

void Mesh::compute_normals(bool smooth) {
    for (auto& v : vertices) v.normal = vec3(0.0f);
    for (auto& f : faces) {
        if (f.vertices.size() < 3) continue;
        vec3 e1 = vertices[f.vertices[1]].position - vertices[f.vertices[0]].position;
        vec3 e2 = vertices[f.vertices[2]].position - vertices[f.vertices[0]].position;
        vec3 fn = glm::normalize(glm::cross(e1, e2));
        f.normal = fn;
        if (smooth) {
            for (auto& vi : f.vertices) vertices[vi].normal += fn;
        }
    }
    if (smooth) {
        for (auto& v : vertices) {
            float len = glm::length(v.normal);
            if (len > 1e-6f) v.normal /= len;
            else v.normal = vec3(0, 1, 0);
        }
    } else {
        for (auto& f : faces) {
            if (f.vertices.size() < 3) continue;
            for (auto& vi : f.vertices) vertices[vi].normal = f.normal;
        }
    }
    dirty = true;
}

void Mesh::recalculate_normals() { compute_normals(true); }

void Mesh::flip_normals() {
    for (auto& v : vertices) v.normal = -v.normal;
    for (auto& f : faces)
        if (f.vertices.size() >= 3) f.normal = -f.normal;
    dirty = true;
}

void Mesh::apply_transform(const mat4& m) {
    for (auto& v : vertices) {
        v.position = vec3(m * vec4(v.position, 1.0f));
        v.normal   = glm::normalize(vec3(m * vec4(v.normal, 0.0f)));
    }
    dirty = true;
}

void Mesh::center_pivot() {
    vec3 c = centroid();
    for (auto& v : vertices) v.position -= c;
    dirty = true;
}

void Mesh::scale_fit(float target_size) {
    vec3 bmin = get_bounding_box_min();
    vec3 bmax = get_bounding_box_max();
    vec3 ext  = bmax - bmin;
    float max_dim = ext.x;
    if (ext.y > max_dim) max_dim = ext.y;
    if (ext.z > max_dim) max_dim = ext.z;
    if (max_dim < 1e-6f) return;
    float s = target_size / max_dim;
    for (auto& v : vertices) v.position *= s;
    dirty = true;
}

void Mesh::flatten_to_plane(const vec3& normal, float offset) {
    vec3 n = glm::normalize(normal);
    for (auto& v : vertices) {
        float d = glm::dot(v.position, n) - offset;
        v.position -= n * d;
    }
    dirty = true;
}

void Mesh::mirror(int axis) {
    if (vertices.empty() || axis < 0 || axis > 2) return;
    size_t orig = vertices.size();
    for (size_t i = 0; i < orig; ++i) {
        Vertex v = vertices[i];
        (&v.position.x)[axis] = -(&v.position.x)[axis];
        (&v.normal.x)[axis]   = -(&v.normal.x)[axis];
        vertices.push_back(v);
    }
    size_t orig_f = faces.size();
    for (size_t i = 0; i < orig_f; ++i) {
        Face f = faces[i];
        std::reverse(f.vertices.begin(), f.vertices.end());
        for (auto& vi : f.vertices) vi += (uint32_t)orig;
        faces.push_back(f);
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
        mid.normal   = glm::normalize((vertices[i0].normal + vertices[i1].normal) * 0.5f);
        mid.uv       = (vertices[i0].uv + vertices[i1].uv) * 0.5f;
        mid.color    = (vertices[i0].color + vertices[i1].color) * 0.5f;
        uint32_t idx = (uint32_t)new_verts.size();
        new_verts.push_back(mid);
        mid_cache[key] = idx;
        return idx;
    };
    for (auto& f : faces) {
        if (f.vertices.size() == 3) {
            uint32_t a = f.vertices[0], b = f.vertices[1], c = f.vertices[2];
            uint32_t ab = get_midpoint(a, b), bc = get_midpoint(b, c), ca = get_midpoint(c, a);
            new_faces.push_back({{a, ab, ca}});
            new_faces.push_back({{b, bc, ab}});
            new_faces.push_back({{c, ca, bc}});
            new_faces.push_back({{ab, bc, ca}});
        } else if (f.vertices.size() == 4) {
            uint32_t a = f.vertices[0], b = f.vertices[1], c = f.vertices[2], d = f.vertices[3];
            uint32_t ab = get_midpoint(a, b), bc = get_midpoint(b, c);
            uint32_t cd = get_midpoint(c, d), da = get_midpoint(d, a);
            uint32_t center = (uint32_t)new_verts.size();
            Vertex cv;
            cv.position = (new_verts[a].position + new_verts[b].position +
                           new_verts[c].position + new_verts[d].position) * 0.25f;
            cv.normal = glm::normalize(
                (new_verts[a].normal + new_verts[b].normal +
                 new_verts[c].normal + new_verts[d].normal) * 0.25f);
            cv.uv    = (new_verts[a].uv + new_verts[b].uv +
                        new_verts[c].uv + new_verts[d].uv) * 0.25f;
            cv.color = (new_verts[a].color + new_verts[b].color +
                        new_verts[c].color + new_verts[d].color) * 0.25f;
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
    faces    = new_faces;
    compute_normals(true);
}

void Mesh::subdivide_catmull_clark() {
    if (faces.empty()) return;
    size_t orig_n = vertices.size();
    size_t face_n = faces.size();
    using edge_key = std::pair<uint32_t, uint32_t>;
    auto ecmp = [](const edge_key& a, const edge_key& b) {
        if (a.first != b.first) return a.first < b.first;
        return a.second < b.second;
    };
    std::map<edge_key, uint32_t, decltype(ecmp)> edge_map(ecmp);
    std::vector<edge_key> edges;
    std::vector<std::vector<uint32_t>> edge_adj_faces;
    std::vector<std::vector<uint32_t>> vert_faces(orig_n);
    std::vector<std::vector<uint32_t>> vert_edges(orig_n);
    auto get_edge = [&](uint32_t a, uint32_t b) -> uint32_t {
        edge_key k{std::min(a, b), std::max(a, b)};
        auto it = edge_map.find(k);
        if (it != edge_map.end()) return it->second;
        uint32_t idx = (uint32_t)edges.size();
        edge_map[k] = idx;
        edges.push_back(k);
        edge_adj_faces.push_back({});
        return idx;
    };
    for (uint32_t fi = 0; fi < (uint32_t)face_n; ++fi) {
        auto& f = faces[fi];
        size_t n = f.vertices.size();
        for (size_t i = 0; i < n; ++i) {
            uint32_t a = f.vertices[i];
            uint32_t b = f.vertices[(i + 1) % n];
            uint32_t ei = get_edge(a, b);
            edge_adj_faces[ei].push_back(fi);
            vert_faces[a].push_back(fi);
            vert_edges[a].push_back(ei);
        }
    }
    std::vector<vec3> fc(face_n);
    for (uint32_t fi = 0; fi < (uint32_t)face_n; ++fi) {
        vec3 c(0.0f);
        for (auto vi : faces[fi].vertices) c += vertices[vi].position;
        fc[fi] = c / (float)faces[fi].vertices.size();
    }
    size_t edge_n = edges.size();
    std::vector<Vertex> nv(orig_n + face_n + edge_n);
    for (size_t i = 0; i < orig_n; ++i) nv[i] = vertices[i];
    for (size_t fi = 0; fi < face_n; ++fi) nv[orig_n + fi].position = fc[fi];
    for (size_t ei = 0; ei < edge_n; ++ei)
        nv[orig_n + face_n + ei].position =
            (vertices[edges[ei].first].position +
             vertices[edges[ei].second].position) * 0.5f;
    for (size_t vi = 0; vi < orig_n; ++vi) {
        int nf = (int)vert_faces[vi].size();
        if (nf == 0) continue;
        vec3 F(0.0f);
        for (auto fi : vert_faces[vi]) F += fc[fi];
        F /= (float)nf;
        vec3 R(0.0f);
        for (auto ei : vert_edges[vi])
            R += (vertices[edges[ei].first].position +
                  vertices[edges[ei].second].position) * 0.5f;
        R /= (float)vert_edges[vi].size();
        vec3 P = vertices[vi].position;
        float n = (float)nf;
        nv[vi].position = (F + 2.0f * R + (n - 3.0f) * P) / n;
    }
    for (size_t ei = 0; ei < edge_n; ++ei) {
        auto& adj = edge_adj_faces[ei];
        vec3 P1 = vertices[edges[ei].first].position;
        vec3 P2 = vertices[edges[ei].second].position;
        if (adj.size() == 2)
            nv[orig_n + face_n + ei].position = (fc[adj[0]] + fc[adj[1]] + P1 + P2) * 0.25f;
    }
    std::vector<Face> new_faces;
    for (uint32_t fi = 0; fi < (uint32_t)face_n; ++fi) {
        auto& f = faces[fi];
        size_t n = f.vertices.size();
        uint32_t fci = (uint32_t)(orig_n + fi);
        for (size_t i = 0; i < n; ++i) {
            uint32_t a = f.vertices[i];
            uint32_t b = f.vertices[(i + 1) % n];
            uint32_t ei = get_edge(a, b);
            uint32_t em = (uint32_t)(orig_n + face_n + ei);
            uint32_t prev_ei = get_edge(f.vertices[(i + n - 1) % n], a);
            uint32_t prev_em = (uint32_t)(orig_n + face_n + prev_ei);
            new_faces.push_back({{a, em, fci, prev_em}});
        }
    }
    vertices = nv;
    faces    = new_faces;
    compute_normals(true);
}

void Mesh::invert_faces() {
    for (auto& f : faces) std::reverse(f.vertices.begin(), f.vertices.end());
    for (auto& v : vertices) v.normal = -v.normal;
    dirty = true;
}

void Mesh::weld_vertices(float threshold) {
    if (vertices.empty()) return;
    float cs = threshold * 2.0f;
    if (cs < 1e-8f) cs = 1e-4f;
    SpatialHash sh(cs);
    for (uint32_t i = 0; i < (uint32_t)vertices.size(); ++i)
        sh.insert(i, vertices[i].position);
    std::vector<uint32_t> remap(vertices.size());
    std::vector<Vertex> new_verts;
    std::vector<bool> claimed(vertices.size(), false);
    for (uint32_t i = 0; i < (uint32_t)vertices.size(); ++i) {
        std::vector<uint32_t> cands;
        sh.query(vertices[i].position, cands);
        uint32_t best = i;
        for (uint32_t j : cands) {
            if (j < i && claimed[j] &&
                glm::length(vertices[i].position - vertices[j].position) < threshold) {
                best = j;
                break;
            }
        }
        if (best == i) {
            remap[i] = (uint32_t)new_verts.size();
            new_verts.push_back(vertices[i]);
            claimed[i] = true;
        } else {
            remap[i] = remap[best];
        }
    }
    for (auto& f : faces)
        for (auto& vi : f.vertices) vi = remap[vi];
    faces.erase(
        std::remove_if(faces.begin(), faces.end(),
            [](const Face& f) {
                if (f.vertices.size() < 3) return true;
                for (size_t i = 0; i < f.vertices.size(); ++i)
                    for (size_t j = i + 1; j < f.vertices.size(); ++j)
                        if (f.vertices[i] == f.vertices[j]) return true;
                return false;
            }), faces.end());
    vertices = new_verts;
    dirty = true;
}

void Mesh::merge_vertices(float threshold) { weld_vertices(threshold); }

void Mesh::smooth_vertices(float factor, int iterations) {
    if (vertices.empty()) return;
    std::vector<std::vector<uint32_t>> adj(vertices.size());
    for (auto& f : faces) {
        size_t n = f.vertices.size();
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i + 1; j < n; ++j) {
                adj[f.vertices[i]].push_back(f.vertices[j]);
                adj[f.vertices[j]].push_back(f.vertices[i]);
            }
    }
    for (int iter = 0; iter < iterations; ++iter) {
        std::vector<vec3> new_pos(vertices.size());
        for (size_t i = 0; i < vertices.size(); ++i) {
            if (adj[i].empty()) { new_pos[i] = vertices[i].position; continue; }
            vec3 avg(0.0f);
            for (auto j : adj[i]) avg += vertices[j].position;
            avg /= (float)adj[i].size();
            new_pos[i] = vertices[i].position + (avg - vertices[i].position) * factor;
        }
        for (size_t i = 0; i < vertices.size(); ++i)
            vertices[i].position = new_pos[i];
    }
    compute_normals(true);
}

void Mesh::extrude_faces(const std::vector<uint32_t>& face_indices, float depth) {
    for (uint32_t fi : face_indices) {
        if (fi >= (uint32_t)faces.size()) continue;
        Face& f = faces[fi];
        if (f.vertices.size() < 3) continue;
        vec3 fn = f.normal;
        if (glm::length(fn) < 1e-6f) {
            vec3 e1 = vertices[f.vertices[1]].position - vertices[f.vertices[0]].position;
            vec3 e2 = vertices[f.vertices[2]].position - vertices[f.vertices[0]].position;
            fn = glm::normalize(glm::cross(e1, e2));
        }
        vec3 offset = fn * depth;
        size_t n = f.vertices.size();
        std::vector<uint32_t> new_vi(n);
        for (size_t i = 0; i < n; ++i) {
            Vertex v = vertices[f.vertices[i]];
            v.position += offset;
            new_vi[i] = (uint32_t)vertices.size();
            vertices.push_back(v);
        }
        for (size_t i = 0; i < n; ++i) {
            size_t j = (i + 1) % n;
            faces.push_back({{f.vertices[i], f.vertices[j], new_vi[j], new_vi[i]}});
        }
        f.vertices = new_vi;
    }
    compute_normals(true);
}

void Mesh::bevel_edges(float amount, int segments) {
    if (faces.empty() || amount <= 0.0f || segments < 1) return;
    using ek = std::pair<uint32_t, uint32_t>;
    std::map<ek, std::vector<uint32_t>> edge_faces;
    for (uint32_t fi = 0; fi < (uint32_t)faces.size(); ++fi) {
        auto& f = faces[fi];
        size_t n = f.vertices.size();
        for (size_t i = 0; i < n; ++i) {
            uint32_t a = f.vertices[i], b = f.vertices[(i + 1) % n];
            edge_faces[{std::min(a, b), std::max(a, b)}].push_back(fi);
        }
    }
    for (auto& [ek_key, adj] : edge_faces) {
        if (adj.size() != 2) continue;
        uint32_t V0 = ek_key.first, V1 = ek_key.second;
        vec3 n1 = glm::normalize(faces[adj[0]].normal);
        vec3 n2 = glm::normalize(faces[adj[1]].normal);
        vec3 B  = glm::normalize(n1 + n2);
        std::vector<uint32_t> ring0(segments + 1), ring1(segments + 1);
        ring0[0] = V0; ring1[0] = V1;
        for (int s = 1; s <= segments; ++s) {
            float t = (float)s / (float)segments;
            vec3 off = B * amount * t;
            ring0[s] = (uint32_t)vertices.size();
            vertices.push_back({vertices[V0].position + off, vertices[V0].normal,
                                vertices[V0].uv, vertices[V0].color});
            ring1[s] = (uint32_t)vertices.size();
            vertices.push_back({vertices[V1].position + off, vertices[V1].normal,
                                vertices[V1].uv, vertices[V1].color});
        }
        for (int s = 0; s < segments; ++s)
            faces.push_back({{ring0[s], ring1[s], ring1[s + 1], ring0[s + 1]}});
        uint32_t nv0 = ring0[segments], nv1 = ring1[segments];
        for (auto fi : {adj[0], adj[1]})
            for (auto& vi : faces[fi].vertices) {
                if (vi == V0) vi = nv0;
                else if (vi == V1) vi = nv1;
            }
    }
    compute_normals(true);
}

void Mesh::loop_cut(int axis, float position) {
    std::vector<Face> new_faces;
    for (auto& f : faces) {
        size_t n = f.vertices.size();
        if (n < 3) { new_faces.push_back(f); continue; }
        std::vector<float> vals(n);
        for (size_t i = 0; i < n; ++i)
            vals[i] = (&vertices[f.vertices[i]].position.x)[axis];
        std::vector<int> side(n);
        for (size_t i = 0; i < n; ++i) {
            float d = vals[i] - position;
            if (std::abs(d) < 1e-6f) side[i] = 0;
            else if (d < 0.0f) side[i] = -1;
            else side[i] = 1;
        }
        std::vector<std::pair<size_t, uint32_t>> cuts;
        for (size_t i = 0; i < n; ++i) {
            size_t j = (i + 1) % n;
            if (side[i] * side[j] < 0) {
                float frac = (position - vals[i]) / (vals[j] - vals[i]);
                vec3  pos = glm::mix(vertices[f.vertices[i]].position,
                                     vertices[f.vertices[j]].position, frac);
                vec3  nrm = glm::mix(vertices[f.vertices[i]].normal,
                                     vertices[f.vertices[j]].normal, frac);
                vec2  uv  = glm::mix(vertices[f.vertices[i]].uv,
                                     vertices[f.vertices[j]].uv, frac);
                vec4  col = glm::mix(vertices[f.vertices[i]].color,
                                     vertices[f.vertices[j]].color, frac);
                uint32_t vi = (uint32_t)vertices.size();
                vertices.push_back({pos, nrm, uv, col});
                cuts.push_back({i, vi});
            }
        }
        if (cuts.size() == 0) {
            new_faces.push_back(f);
        } else if (cuts.size() == 2) {
            size_t e0 = cuts[0].first, e1 = cuts[1].first;
            std::vector<uint32_t> loop1, loop2;
            loop1.push_back(cuts[0].second);
            { size_t idx = (e0 + 1) % n;
              while (idx != (e1 + 1) % n) { loop1.push_back(f.vertices[idx]); idx = (idx + 1) % n; } }
            loop1.push_back(cuts[1].second);
            loop2.push_back(cuts[1].second);
            { size_t idx = (e1 + 1) % n;
              while (idx != (e0 + 1) % n) { loop2.push_back(f.vertices[idx]); idx = (idx + 1) % n; } }
            loop2.push_back(cuts[0].second);
            if (loop1.size() >= 3) new_faces.push_back({loop1});
            if (loop2.size() >= 3) new_faces.push_back({loop2});
        } else {
            new_faces.push_back(f);
        }
    }
    faces = new_faces;
    compute_normals(true);
}

void Mesh::make_solid(float thickness) {
    if (faces.empty()) return;
    size_t orig = vertices.size();
    for (size_t i = 0; i < orig; ++i) {
        Vertex v = vertices[i];
        v.position += v.normal * thickness;
        vertices.push_back(v);
    }
    size_t orig_f = faces.size();
    for (size_t i = 0; i < orig_f; ++i) {
        Face f = faces[i];
        for (auto& vi : f.vertices) vi += (uint32_t)orig;
        std::reverse(f.vertices.begin(), f.vertices.end());
        faces.push_back(f);
    }
    using ek = std::pair<uint32_t, uint32_t>;
    std::map<ek, int> edge_count;
    for (auto& f : faces) {
        size_t n = f.vertices.size();
        for (size_t i = 0; i < n; ++i) {
            uint32_t a = f.vertices[i], b = f.vertices[(i + 1) % n];
            edge_count[{std::min(a, b), std::max(a, b)}]++;
        }
    }
    for (auto& f : faces) {
        size_t n = f.vertices.size();
        for (size_t i = 0; i < n; ++i) {
            uint32_t a = f.vertices[i], b = f.vertices[(i + 1) % n];
            ek key{std::min(a, b), std::max(a, b)};
            if (edge_count[key] == 1)
                faces.push_back({{a, b, b + (uint32_t)orig, a + (uint32_t)orig}});
        }
    }
    compute_normals(true);
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
    for (auto& f : faces)
        if (f.vertices.size() >= 3) count += (int)f.vertices.size() - 2;
    return count;
}

int Mesh::get_index_count() const {
    int count = 0;
    for (auto& f : faces) {
        if (f.vertices.size() >= 3)
            count += ((int)f.vertices.size() - 2) * 3;
    }
    return count;
}

float Mesh::surface_area() const {
    float total = 0.0f;
    for (auto& f : faces) {
        if (f.vertices.size() < 3) continue;
        for (size_t i = 2; i < f.vertices.size(); ++i) {
            vec3 a = vertices[f.vertices[0]].position;
            vec3 b = vertices[f.vertices[i - 1]].position;
            vec3 c = vertices[f.vertices[i]].position;
            total += 0.5f * glm::length(glm::cross(b - a, c - a));
        }
    }
    return total;
}

vec3 Mesh::centroid() const {
    if (vertices.empty()) return vec3(0.0f);
    vec3 c(0.0f);
    for (auto& v : vertices) c += v.position;
    return c / (float)vertices.size();
}

vec3 Mesh::get_bounding_box_min() const {
    if (vertices.empty()) return vec3(0.0f);
    vec3 mn = vertices[0].position;
    for (auto& v : vertices) mn = glm::min(mn, v.position);
    return mn;
}

vec3 Mesh::get_bounding_box_max() const {
    if (vertices.empty()) return vec3(0.0f);
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
        if (f.vertices.size() < 3) continue;
        uint32_t v0 = f.vertices[0];
        for (size_t i = 1; i + 1 < f.vertices.size(); ++i) {
            cached_index_data.push_back(v0);
            cached_index_data.push_back(f.vertices[i]);
            cached_index_data.push_back(f.vertices[i + 1]);
        }
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

Mesh Mesh::create_arrow(float length, float shaft_radius) {
    return create_arrow_3d(length * 0.7f, shaft_radius, length * 0.3f, shaft_radius * 3.0f);
}

Mesh Mesh::create_arrow_3d(float shaft_length, float shaft_radius,
                            float head_length, float head_radius) {
    Mesh m;
    m.name = "arrow_3d";
    const int seg = 16;
    const float TWO_PI = (float)M_PI * 2.0f;
    for (int i = 0; i < seg; ++i) {
        float a0 = (float)i / seg * TWO_PI;
        float a1 = (float)(i + 1) / seg * TWO_PI;
        float c0 = std::cos(a0), s0 = std::sin(a0);
        float c1 = std::cos(a1), s1 = std::sin(a1);
        float u0 = (float)i / seg, u1 = (float)(i + 1) / seg;
        uint32_t b0 = m.add_vertex(vec3(c0*shaft_radius, 0, s0*shaft_radius), vec3(c0,0,s0), vec2(u0,0));
        uint32_t b1 = m.add_vertex(vec3(c1*shaft_radius, 0, s1*shaft_radius), vec3(c1,0,s1), vec2(u1,0));
        uint32_t t0 = m.add_vertex(vec3(c0*shaft_radius, shaft_length, s0*shaft_radius), vec3(c0,0,s0), vec2(u0,1));
        uint32_t t1 = m.add_vertex(vec3(c1*shaft_radius, shaft_length, s1*shaft_radius), vec3(c1,0,s1), vec2(u1,1));
        m.add_face({b0, b1, t1, t0});
    }
    for (int i = 0; i < seg; ++i) {
        float a0 = (float)i / seg * TWO_PI;
        float a1 = (float)(i + 1) / seg * TWO_PI;
        float c0 = std::cos(a0), s0 = std::sin(a0);
        float c1 = std::cos(a1), s1 = std::sin(a1);
        float u0 = (float)i / seg, u1 = (float)(i + 1) / seg;
        uint32_t c0v = m.add_vertex(vec3(c0*head_radius, shaft_length, s0*head_radius), vec3(c0,0,s0), vec2(u0,0));
        uint32_t c1v = m.add_vertex(vec3(c1*head_radius, shaft_length, s1*head_radius), vec3(c1,0,s1), vec2(u1,0));
        uint32_t ti = m.add_vertex(vec3(0, shaft_length + head_length, 0), vec3(0,1,0), vec2((u0+u1)*0.5f,1));
        m.add_face({c0v, c1v, ti});
    }
    m.compute_normals(true);
    return m;
}

Mesh Mesh::create_tube(float inner_r, float outer_r, float height, int segments) {
    Mesh m;
    m.name = "tube";
    const float TWO_PI = (float)M_PI * 2.0f;
    for (int i = 0; i < segments; ++i) {
        float a0 = (float)i / segments * TWO_PI;
        float a1 = (float)(i + 1) / segments * TWO_PI;
        float co0 = std::cos(a0), si0 = std::sin(a0);
        float co1 = std::cos(a1), si1 = std::sin(a1);
        float u0 = (float)i / segments, u1 = (float)(i + 1) / segments;
        uint32_t ob0 = m.add_vertex(vec3(co0*outer_r,0,si0*outer_r), vec3(co0,0,si0), vec2(u0,0));
        uint32_t ob1 = m.add_vertex(vec3(co1*outer_r,0,si1*outer_r), vec3(co1,0,si1), vec2(u1,0));
        uint32_t ot0 = m.add_vertex(vec3(co0*outer_r,height,si0*outer_r), vec3(co0,0,si0), vec2(u0,1));
        uint32_t ot1 = m.add_vertex(vec3(co1*outer_r,height,si1*outer_r), vec3(co1,0,si1), vec2(u1,1));
        uint32_t ib0 = m.add_vertex(vec3(co0*inner_r,0,si0*inner_r), vec3(-co0,0,-si0), vec2(u0,0));
        uint32_t ib1 = m.add_vertex(vec3(co1*inner_r,0,si1*inner_r), vec3(-co1,0,-si1), vec2(u1,0));
        uint32_t it0 = m.add_vertex(vec3(co0*inner_r,height,si0*inner_r), vec3(-co0,0,-si0), vec2(u0,1));
        uint32_t it1 = m.add_vertex(vec3(co1*inner_r,height,si1*inner_r), vec3(-co1,0,-si1), vec2(u1,1));
        m.add_face({ob0, ob1, ot1, ot0});
        m.add_face({it0, it1, ib1, ib0});
        m.add_face({ot0, ot1, it1, it0});
        m.add_face({ib0, ib1, ob1, ob0});
    }
    m.compute_normals(true);
    return m;
}

Mesh Mesh::create_capsule(float radius, float height, int segments) {
    Mesh m;
    m.name = "capsule";
    const float TWO_PI = (float)M_PI * 2.0f;
    const float HALF_PI = (float)M_PI * 0.5f;
    int half_segs = segments / 2;
    float hh = height * 0.5f;
    for (int i = 0; i < segments; ++i) {
        float a0 = (float)i / segments * TWO_PI;
        float a1 = (float)(i + 1) / segments * TWO_PI;
        float co0 = std::cos(a0), si0 = std::sin(a0);
        float co1 = std::cos(a1), si1 = std::sin(a1);
        float u0 = (float)i / segments, u1 = (float)(i + 1) / segments;
        uint32_t b0 = m.add_vertex(vec3(co0*radius,-hh,si0*radius), vec3(co0,0,si0), vec2(u0,0));
        uint32_t b1 = m.add_vertex(vec3(co1*radius,-hh,si1*radius), vec3(co1,0,si1), vec2(u1,0));
        uint32_t t0 = m.add_vertex(vec3(co0*radius,hh,si0*radius), vec3(co0,0,si0), vec2(u0,1));
        uint32_t t1 = m.add_vertex(vec3(co1*radius,hh,si1*radius), vec3(co1,0,si1), vec2(u1,1));
        m.add_face({b0, b1, t1, t0});
    }
    auto add_hemi = [&](float sign) {
        for (int i = 0; i < segments; ++i) {
            float a0 = (float)i / segments * TWO_PI;
            float a1 = (float)(i + 1) / segments * TWO_PI;
            float co0 = std::cos(a0), si0 = std::sin(a0);
            float co1 = std::cos(a1), si1 = std::sin(a1);
            for (int j = 0; j < half_segs; ++j) {
                float p0 = (float)j / half_segs * HALF_PI;
                float p1 = (float)(j + 1) / half_segs * HALF_PI;
                float r0 = std::cos(p0), h0 = std::sin(p0);
                float r1 = std::cos(p1), h1 = std::sin(p1);
                float y0 = sign * hh + sign * radius * h0;
                float y1 = sign * hh + sign * radius * h1;
                uint32_t v00 = m.add_vertex(vec3(co0*radius*r0,y0,si0*radius*r0),
                                            glm::normalize(vec3(co0*r0,sign*h0,si0*r0)));
                uint32_t v10 = m.add_vertex(vec3(co1*radius*r0,y0,si1*radius*r0),
                                            glm::normalize(vec3(co1*r0,sign*h0,si1*r0)));
                uint32_t v01 = m.add_vertex(vec3(co0*radius*r1,y1,si0*radius*r1),
                                            glm::normalize(vec3(co0*r1,sign*h1,si0*r1)));
                uint32_t v11 = m.add_vertex(vec3(co1*radius*r1,y1,si1*radius*r1),
                                            glm::normalize(vec3(co1*r1,sign*h1,si1*r1)));
                if (sign > 0) m.add_face({v00, v10, v11, v01});
                else          m.add_face({v01, v11, v10, v00});
            }
        }
    };
    add_hemi(1.0f);
    add_hemi(-1.0f);
    m.compute_normals(true);
    return m;
}

Mesh Mesh::create_grid(int subdiv_x, int subdiv_z, float size) {
    Mesh m;
    m.name = "grid";
    float half = size * 0.5f;
    float dx = size / subdiv_x;
    float dz = size / subdiv_z;
    for (int iz = 0; iz <= subdiv_z; ++iz) {
        for (int ix = 0; ix <= subdiv_x; ++ix) {
            float x = -half + ix * dx;
            float z = -half + iz * dz;
            float u = (float)ix / subdiv_x;
            float v = (float)iz / subdiv_z;
            m.add_vertex(vec3(x, 0, z), vec3(0, 1, 0), vec2(u, v));
        }
    }
    for (int iz = 0; iz < subdiv_z; ++iz) {
        for (int ix = 0; ix < subdiv_x; ++ix) {
            uint32_t a = iz * (subdiv_x + 1) + ix;
            uint32_t b = a + 1;
            uint32_t c = a + (subdiv_x + 1);
            uint32_t d = c + 1;
            m.add_face({a, b, d, c});
        }
    }
    return m;
}

Mesh Mesh::create_torus_knot(int p, int q, float radius, float tube, int segments) {
    Mesh m;
    m.name = "torus_knot";
    int tube_segs = 12;
    const float TWO_PI = (float)M_PI * 2.0f;
    auto point_on = [&](float t) -> vec3 {
        float r = radius * (2.0f + std::cos((float)q * t)) / 3.0f;
        return vec3(r * std::cos((float)p * t), r * std::sin((float)p * t),
                    radius * std::sin((float)q * t) / 3.0f);
    };
    for (int i = 0; i < segments; ++i) {
        float t0 = (float)i / segments * TWO_PI;
        float t1 = (float)(i + 1) / segments * TWO_PI;
        vec3 P0 = point_on(t0), P1 = point_on(t1);
        vec3 T = glm::normalize(P1 - P0);
        vec3 up(0, 1, 0);
        if (std::abs(glm::dot(T, up)) > 0.99f) up = vec3(1, 0, 0);
        vec3 R = glm::normalize(glm::cross(up, T));
        vec3 B = glm::cross(T, R);
        for (int j = 0; j < tube_segs; ++j) {
            float a0 = (float)j / tube_segs * TWO_PI;
            float a1 = (float)(j + 1) / tube_segs * TWO_PI;
            vec3 q0 = P0 + (R * std::cos(a0) + B * std::sin(a0)) * tube;
            vec3 q1 = P0 + (R * std::cos(a1) + B * std::sin(a1)) * tube;
            vec3 q2 = P1 + (R * std::cos(a1) + B * std::sin(a1)) * tube;
            vec3 q3 = P1 + (R * std::cos(a0) + B * std::sin(a0)) * tube;
            uint32_t v0 = m.add_vertex(q0, glm::normalize(q0 - P0));
            uint32_t v1 = m.add_vertex(q1, glm::normalize(q1 - P0));
            uint32_t v2 = m.add_vertex(q2, glm::normalize(q2 - P1));
            uint32_t v3 = m.add_vertex(q3, glm::normalize(q3 - P1));
            m.add_face({v0, v1, v2, v3});
        }
    }
    m.compute_normals(true);
    return m;
}

Mesh Mesh::create_icosphere(float radius, int subdivisions) {
    Mesh m;
    m.name = "icosphere";
    float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
    auto add_norm = [&](vec3 p) -> uint32_t {
        vec3 n = glm::normalize(p);
        return m.add_vertex(n * radius, n);
    };
    std::array<uint32_t, 12> iv;
    iv[0]  = add_norm(vec3(-1, t, 0));   iv[1]  = add_norm(vec3(1, t, 0));
    iv[2]  = add_norm(vec3(-1, -t, 0));  iv[3]  = add_norm(vec3(1, -t, 0));
    iv[4]  = add_norm(vec3(0, -1, t));   iv[5]  = add_norm(vec3(0, 1, t));
    iv[6]  = add_norm(vec3(0, -1, -t));  iv[7]  = add_norm(vec3(0, 1, -t));
    iv[8]  = add_norm(vec3(t, 0, -1));   iv[9]  = add_norm(vec3(t, 0, 1));
    iv[10] = add_norm(vec3(-t, 0, -1));  iv[11] = add_norm(vec3(-t, 0, 1));
    std::array<std::array<uint32_t, 3>, 20> ico_f = {{
        {{iv[0],iv[11],iv[5]}},  {{iv[0],iv[5],iv[1]}},  {{iv[0],iv[1],iv[7]}},
        {{iv[0],iv[7],iv[10]}}, {{iv[0],iv[10],iv[11]}}, {{iv[1],iv[5],iv[9]}},
        {{iv[5],iv[11],iv[4]}}, {{iv[11],iv[10],iv[2]}}, {{iv[10],iv[7],iv[6]}},
        {{iv[7],iv[1],iv[8]}},  {{iv[3],iv[9],iv[4]}},   {{iv[3],iv[4],iv[2]}},
        {{iv[3],iv[2],iv[6]}},  {{iv[3],iv[6],iv[8]}},   {{iv[3],iv[8],iv[9]}},
        {{iv[4],iv[9],iv[5]}},  {{iv[2],iv[4],iv[11]}},  {{iv[6],iv[2],iv[10]}},
        {{iv[8],iv[6],iv[7]}},  {{iv[9],iv[8],iv[1]}}
    }};
    for (auto& f : ico_f) m.add_face({f[0], f[1], f[2]});
    for (int s = 0; s < subdivisions; ++s) {
        std::vector<Face> new_faces;
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> mc;
        auto get_mid = [&](uint32_t a, uint32_t b) -> uint32_t {
            if (a > b) std::swap(a, b);
            auto key = std::make_pair(a, b);
            auto it = mc.find(key);
            if (it != mc.end()) return it->second;
            vec3 pos = glm::normalize(
                (m.vertices[a].position + m.vertices[b].position) * 0.5f) * radius;
            uint32_t idx = (uint32_t)m.vertices.size();
            m.add_vertex(pos, glm::normalize(pos));
            mc[key] = idx;
            return idx;
        };
        for (auto& f : m.faces) {
            if (f.vertices.size() != 3) { new_faces.push_back(f); continue; }
            uint32_t a = f.vertices[0], b = f.vertices[1], c = f.vertices[2];
            uint32_t ab = get_mid(a, b), bc = get_mid(b, c), ca = get_mid(c, a);
            new_faces.push_back({{a, ab, ca}});
            new_faces.push_back({{b, bc, ab}});
            new_faces.push_back({{c, ca, bc}});
            new_faces.push_back({{ab, bc, ca}});
        }
        m.faces = new_faces;
    }
    m.dirty = true;
    m.compute_normals(true);
    return m;
}

Mesh Mesh::create_hemisphere(float radius, int segments) {
    Mesh m;
    m.name = "hemisphere";
    const float TWO_PI = (float)M_PI * 2.0f;
    const float HALF_PI = (float)M_PI * 0.5f;
    int rings = segments / 2;
    uint32_t top = m.add_vertex(vec3(0, radius, 0), vec3(0, 1, 0), vec2(0.5f, 1.0f));
    for (int j = 1; j <= rings; ++j) {
        float phi = (float)j / rings * HALF_PI;
        float y = radius * std::cos(phi);
        float r = radius * std::sin(phi);
        for (int i = 0; i < segments; ++i) {
            float theta = (float)i / segments * TWO_PI;
            vec3 pos = vec3(r * std::cos(theta), y, r * std::sin(theta));
            vec3 nrm = glm::normalize(pos);
            vec2 uv((float)i / segments, 1.0f - (float)j / rings);
            m.add_vertex(pos, nrm, uv);
        }
    }
    for (int i = 0; i < segments; ++i)
        m.add_face({top, (uint32_t)(1 + i), (uint32_t)(1 + (i + 1) % segments)});
    for (int j = 0; j < rings - 1; ++j)
        for (int i = 0; i < segments; ++i) {
            uint32_t next = (i + 1) % segments;
            uint32_t a = 1 + j * segments + i;
            uint32_t b = 1 + j * segments + next;
            uint32_t c = 1 + (j + 1) * segments + next;
            uint32_t d = 1 + (j + 1) * segments + i;
            m.add_face({a, b, c, d});
        }
    m.compute_normals(true);
    return m;
}

void Mesh::bmesh_from_mesh(BMesh& bm) const {
    bm.clear();
    std::vector<BMVert*> bverts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        BMVert* v = bm.vert_add(vertices[i].position);
        v->no = vertices[i].normal;
        v->uv = vertices[i].uv;
        v->color = vertices[i].color;
        bverts[i] = v;
    }
    for (auto& f : faces) {
        if (f.vertices.size() < 3) continue;
        std::vector<BMVert*> fverts;
        for (auto vi : f.vertices) {
            if (vi < (uint32_t)bverts.size()) fverts.push_back(bverts[vi]);
        }
        if (fverts.size() >= 3) {
            BMFace* bf = bm.face_add(fverts);
            if (bf) bf->no = f.normal;
        }
    }
    bm.recalc_all_normals();
}

void Mesh::mesh_from_bmesh(const BMesh& bm) {
    vertices.clear();
    faces.clear();
    cached_vertex_data.clear();
    cached_index_data.clear();
    vertices.reserve(bm.vert_count);
    for (int i = 0; i < bm.vert_count; ++i) {
        BMVert* v = bm.verts[i];
        Vertex vert;
        vert.position = v->co;
        vert.normal = v->no;
        vert.uv = v->uv;
        vert.color = v->color;
        vertices.push_back(vert);
    }
    std::unordered_map<BMVert*, uint32_t> vert_index;
    for (int i = 0; i < bm.vert_count; ++i) {
        vert_index[bm.verts[i]] = (uint32_t)i;
    }
    faces.reserve(bm.face_count);
    for (int i = 0; i < bm.face_count; ++i) {
        BMFace* f = bm.faces[i];
        Face face;
        face.normal = f->no;
        face.vertices.reserve(f->len);
        for (int j = 0; j < f->len; ++j) {
            auto it = vert_index.find(f->verts[j]);
            if (it != vert_index.end()) face.vertices.push_back(it->second);
        }
        if (face.vertices.size() >= 3) faces.push_back(face);
    }
    dirty = true;
}

} // namespace ocp
