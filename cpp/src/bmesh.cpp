#include "bmesh.h"
#include <map>
#include <set>
#include <queue>
#include <cassert>
#include <cmath>

namespace ocp {

BMesh::BMesh(BMesh&& o) noexcept
    : vert_count(o.vert_count), edge_count(o.edge_count), face_count(o.face_count),
      verts(std::move(o.verts)), edges(std::move(o.edges)), faces(std::move(o.faces))
{
    o.vert_count = o.edge_count = o.face_count = 0;
}

BMesh& BMesh::operator=(BMesh&& o) noexcept {
    if (this != &o) {
        clear();
        vert_count = o.vert_count; edge_count = o.edge_count; face_count = o.face_count;
        verts = std::move(o.verts); edges = std::move(o.edges); faces = std::move(o.faces);
        o.vert_count = o.edge_count = o.face_count = 0;
    }
    return *this;
}

BMVert* BMesh::add_vert_raw(const vec3& co) {
    auto* v = new BMVert;
    v->co = co;
    v->index = vert_count;
    verts.push_back(v);
    vert_count++;
    return v;
}

BMEdge* BMesh::add_edge_raw(BMVert* v1, BMVert* v2) {
    auto* e = new BMEdge;
    e->v1 = v1; e->v2 = v2;
    e->index = edge_count;
    edges.push_back(e);
    edge_count++;
    link_vert_edge(v1, e);
    link_vert_edge(v2, e);
    return e;
}

BMFace* BMesh::add_face_raw(const std::vector<BMVert*>& fv, const std::vector<BMEdge*>& fe) {
    auto* f = new BMFace;
    f->len = (int)fv.size();
    f->verts = new BMVert*[f->len];
    f->edges = new BMEdge*[f->len];
    for (int i = 0; i < f->len; i++) {
        f->verts[i] = fv[i];
        f->edges[i] = fe[i];
    }
    f->index = face_count;
    faces.push_back(f);
    face_count++;
    for (auto* e : fe) link_edge_face(e, f);
    recalc_face_normal(f);
    return f;
}

void BMesh::remove_vert_raw(BMVert* v) {
    v->index = -1;
    delete v;
}

void BMesh::remove_edge_raw(BMEdge* e) {
    e->index = -1;
    delete e;
}

void BMesh::remove_face_raw(BMFace* f) {
    delete[] f->verts;
    delete[] f->edges;
    f->index = -1;
    delete f;
}

BMVert* BMesh::vert_add(const vec3& co) { return add_vert_raw(co); }

BMEdge* BMesh::edge_add(BMVert* v1, BMVert* v2) {
    if (v1 == v2) return nullptr;
    BMEdge* existing = nullptr;
    BMEdge* eiter = v1->e;
    if (eiter) do {
        if ((eiter->v1 == v1 && eiter->v2 == v2) || (eiter->v1 == v2 && eiter->v2 == v1)) {
            existing = eiter; break;
        }
        eiter = (eiter->v1 == v1) ? eiter->v1_disk_next : eiter->v2_disk_next;
    } while (eiter != v1->e);
    if (existing) return existing;
    return add_edge_raw(v1, v2);
}

BMFace* BMesh::face_add(const std::vector<BMVert*>& fv) {
    int n = (int)fv.size();
    if (n < 3) return nullptr;
    std::vector<BMEdge*> fe(n);
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        BMEdge* e = nullptr;
        BMEdge* eiter = fv[i]->e;
        if (eiter) do {
            if ((eiter->v1 == fv[i] && eiter->v2 == fv[j]) ||
                (eiter->v1 == fv[j] && eiter->v2 == fv[i])) { e = eiter; break; }
            eiter = (eiter->v1 == fv[i]) ? eiter->v1_disk_next : eiter->v2_disk_next;
        } while (eiter != fv[i]->e);
        if (!e) e = add_edge_raw(fv[i], fv[j]);
        if (e->f1 && e->f2) return nullptr;
        fe[i] = e;
    }
    return add_face_raw(fv, fe);
}

void BMesh::vert_kill(BMVert* v) {
    std::vector<BMEdge*> to_kill;
    BMEdge* eiter = v->e;
    if (eiter) do {
        to_kill.push_back(eiter);
        eiter = (eiter->v1 == v) ? eiter->v1_disk_next : eiter->v2_disk_next;
    } while (eiter && eiter != v->e);
    for (auto* e : to_kill) edge_kill(e);
    remove_vert_raw(v);
}

void BMesh::edge_kill(BMEdge* e) {
    if (e->f1) face_kill(e->f1);
    if (e->f2) face_kill(e->f2);
    unlink_vert_edge(e->v1, e);
    unlink_vert_edge(e->v2, e);
    remove_edge_raw(e);
}

void BMesh::face_kill(BMFace* f) {
    for (int i = 0; i < f->len; i++) unlink_edge_face(f->edges[i], f);
    remove_face_raw(f);
}

void BMesh::link_vert_edge(BMVert* v, BMEdge* e) {
    if (!v->e) {
        v->e = e;
        if (e->v1 == v) { e->v1_disk_next = e; e->v1_disk_prev = e; }
        else { e->v2_disk_next = e; e->v2_disk_prev = e; }
    } else {
        if (e->v1 == v) {
            BMEdge* last = v->e;
            while (last->v1 == v ? last->v1_disk_next != v->e : last->v2_disk_next != v->e)
                last = (last->v1 == v) ? last->v1_disk_next : last->v2_disk_next;
            BMEdge* first_next = (last->v1 == v) ? last->v1_disk_next : last->v2_disk_next;
            e->v1_disk_next = first_next;
            e->v1_disk_prev = last;
            BMEdge* prev_of_first = (first_next->v1 == v) ? first_next->v1_disk_prev : first_next->v2_disk_prev;
            if (prev_of_first == last && first_next != e) {
                if (first_next->v1 == v) first_next->v1_disk_prev = e;
                else first_next->v2_disk_prev = e;
            }
            if (last->v1 == v) last->v1_disk_next = e;
            else last->v2_disk_next = e;
        } else {
            BMEdge* last = v->e;
            while (last->v1 == v ? last->v1_disk_next != v->e : last->v2_disk_next != v->e)
                last = (last->v1 == v) ? last->v1_disk_next : last->v2_disk_next;
            BMEdge* first_next = (last->v1 == v) ? last->v1_disk_next : last->v2_disk_next;
            e->v2_disk_next = first_next;
            e->v2_disk_prev = last;
            BMEdge* prev_of_first = (first_next->v1 == v) ? first_next->v1_disk_prev : first_next->v2_disk_prev;
            if (prev_of_first == last && first_next != e) {
                if (first_next->v1 == v) first_next->v1_disk_prev = e;
                else first_next->v2_disk_prev = e;
            }
            if (last->v1 == v) last->v1_disk_next = e;
            else last->v2_disk_next = e;
        }
    }
}

void BMesh::unlink_vert_edge(BMVert* v, BMEdge* e) {
    BMEdge* next = (e->v1 == v) ? e->v1_disk_next : e->v2_disk_next;
    BMEdge* prev = (e->v1 == v) ? e->v1_disk_prev : e->v2_disk_prev;
    if (next != e) {
        if (next->v1 == v) next->v1_disk_prev = prev;
        else next->v2_disk_prev = prev;
    }
    if (prev != e) {
        if (prev->v1 == v) prev->v1_disk_next = next;
        else prev->v2_disk_next = next;
    }
    if (v->e == e) {
        if (next != e) v->e = next;
        else v->e = nullptr;
    }
}

void BMesh::link_edge_face(BMEdge* e, BMFace* f) {
    if (!e->f1) e->f1 = f;
    else if (!e->f2) e->f2 = f;
}

void BMesh::unlink_edge_face(BMEdge* e, BMFace* f) {
    if (e->f1 == f) e->f1 = e->f2;
    e->f2 = nullptr;
}

void BMesh::link_loop(BMEdge* /*e*/, BMFace* /*f*/) {}
void BMesh::unlink_loop(BMEdge* /*e*/, BMFace* /*f*/) {}

BMVert* BMesh::edge_split_middle(BMEdge* e) {
    vec3 mid = (e->v1->co + e->v2->co) * 0.5f;
    BMVert* v = vert_add(mid);
    v->uv = (e->v1->uv + e->v2->uv) * 0.5f;
    v->color = (e->v1->color + e->v2->color) * 0.5f;
    BMEdge* e2 = add_edge_raw(v, e->v2);
    unlink_vert_edge(e->v2, e);
    e->v2 = v;
    link_vert_edge(v, e);
    if (e->f1) {
        unlink_edge_face(e, e->f1);
        link_edge_face(e2, e->f1);
        for (int i = 0; i < e->f1->len; i++) {
            if (e->f1->edges[i] == e) e->f1->edges[i] = e;
        }
    }
    if (e->f2) {
        unlink_edge_face(e, e->f2);
        link_edge_face(e2, e->f2);
    }
    return v;
}

bool BMesh::edge_split(BMEdge* e, BMVert* v, float factor, BMEdge** e_out) {
    if (!e || !v) return false;
    if (v != e->v1 && v != e->v2) return false;
    BMVert* v_other = (v == e->v1) ? e->v2 : e->v1;

    BMFace* f1 = e->f1;
    BMFace* f2 = e->f2;

    auto split_face = [&](BMFace* f) {
        if (!f) return;
        std::vector<BMVert*> fv;
        std::vector<BMEdge*> fe;
        bool found_v = false;
        for (int i = 0; i < f->len; i++) {
            fv.push_back(f->verts[i]);
            if (f->verts[i] == v) found_v = true;
            if (f->edges[i] == e && found_v) {
                fe.push_back(e);
                fv.push_back(v_other);
                found_v = false;
            } else {
                fe.push_back(f->edges[i]);
            }
        }
        unlink_edge_face(e, f);
        remove_face_raw(f);
        add_face_raw(fv, fe);
    };

    split_face(f1);
    split_face(f2);

    if (e_out) *e_out = e;
    return true;
}

void BMesh::vert_splice(BMVert* v_kill, BMVert* v_target) {
    if (v_kill == v_target) return;
    std::vector<BMEdge*> kill_edges;
    BMEdge* eiter = v_kill->e;
    if (eiter) do {
        BMEdge* next = (eiter->v1 == v_kill) ? eiter->v1_disk_next : eiter->v2_disk_next;
        if (eiter->v1 == v_kill) eiter->v1 = v_target;
        else eiter->v2 = v_target;
        kill_edges.push_back(eiter);
        eiter = next;
    } while (eiter && eiter != v_kill->e);

    for (auto* e : kill_edges) {
        unlink_vert_edge(v_kill, e);
        link_vert_edge(v_target, e);
        if (e->v1 == v_target && e->v2 == v_target) {
            if (e->f1) unlink_edge_face(e, e->f1);
            if (e->f2) unlink_edge_face(e, e->f2);
            remove_edge_raw(e);
        }
    }
    remove_vert_raw(v_kill);
}

bool BMesh::face_split(BMFace* f, BMVert* v1, BMVert* v2, BMFace** f1_out, BMFace** f2_out) {
    if (!f || !v1 || !v2) return false;
    int i1 = -1, i2 = -1;
    for (int i = 0; i < f->len; ++i) {
        if (f->verts[i] == v1) i1 = i;
        if (f->verts[i] == v2) i2 = i;
    }
    if (i1 < 0 || i2 < 0 || i1 == i2) return false;
    BMEdge* new_edge = edge_add(v1, v2);
    std::vector<BMVert*> f1v, f2v;
    std::vector<BMEdge*> f1e, f2e;
    int start = i2;
    for (int c = 0; c < f->len; ++c) {
        int idx = (start + c) % f->len;
        f1v.push_back(f->verts[idx]);
        f1e.push_back(f->edges[idx]);
    }
    f1e.back() = new_edge;
    start = i1;
    for (int c = 0; c < f->len; ++c) {
        int idx = (start + c) % f->len;
        f2v.push_back(f->verts[idx]);
        f2e.push_back(f->edges[idx]);
    }
    f2e.back() = new_edge;
    BMFace* nf1 = add_face_raw(f1v, f1e);
    BMFace* nf2 = add_face_raw(f2v, f2e);
    unlink_edge_face(new_edge, f);
    link_edge_face(new_edge, nf1);
    link_edge_face(new_edge, nf2);
    remove_face_raw(f);
    if (f1_out) *f1_out = nf1;
    if (f2_out) *f2_out = nf2;
    return true;
}

BMFace* BMesh::face_split_tri(BMFace* f, int i1, int i2) {
    if (!f || i1 < 0 || i2 < 0 || i1 >= f->len || i2 >= f->len) return nullptr;
    if (i1 == i2) return nullptr;
    std::vector<BMVert*> f1v, f2v;
    std::vector<BMEdge*> f1e, f2e;
    int start = i2;
    for (int c = 0; c <= i1 - i2 + (i1 < i2 ? f->len : 0); ++c) {
        int idx = (start + c) % f->len;
        f1v.push_back(f->verts[idx]);
        f1e.push_back(f->edges[idx]);
    }
    BMEdge* new_e = edge_add(f->verts[i1], f->verts[i2]);
    f1e.back() = new_e;
    start = i1;
    for (int c = 0; c <= i2 - i1 + (i2 < i1 ? f->len : 0); ++c) {
        int idx = (start + c) % f->len;
        f2v.push_back(f->verts[idx]);
        f2e.push_back(f->edges[idx]);
    }
    f2e.back() = new_e;
    BMFace* nf1 = nullptr;
    BMFace* nf2 = nullptr;
    if (f1v.size() >= 3) nf1 = add_face_raw(f1v, f1e);
    if (f2v.size() >= 3) nf2 = add_face_raw(f2v, f2e);
    if (nf1) { link_edge_face(new_e, nf1); }
    if (nf2) { link_edge_face(new_e, nf2); }
    remove_face_raw(f);
    return nf1;
}

BMFace* BMesh::face_split_quad(BMFace* f, int i1, int i2, int i3, int i4) {
    (void)i3; (void)i4;
    return face_split_tri(f, i1, i2);
}

void BMesh::dissolve_vert(BMVert* v) {
    std::vector<BMEdge*> edges_to_dissolve;
    BMEdge* eiter = v->e;
    if (eiter) do {
        BMEdge* next = (eiter->v1 == v) ? eiter->v1_disk_next : eiter->v2_disk_next;
        BMEdge* other = (eiter->v1 == v) ? eiter : eiter;
        bool is_boundary = (eiter->f1 && !eiter->f2);
        if (is_boundary) edges_to_dissolve.push_back(eiter);
        eiter = next;
    } while (eiter && eiter != v->e);

    for (auto* e : edges_to_dissolve) {
        BMVert* v_keep = (e->v1 == v) ? e->v2 : e->v1;
        vert_splice(v, v_keep);
        return;
    }
}

void BMesh::dissolve_edge(BMEdge* e) {
    if (!e || (!e->f1 && !e->f2)) return;
    if (e->f1 && e->f2) return;
    BMVert* v_keep = e->v1;
    BMVert* v_kill = e->v2;
    vert_splice(v_kill, v_keep);
}

void BMesh::dissolve_face(BMFace* f) {
    if (!f) return;
}

void BMesh::recalc_face_normal(BMFace* f) {
    if (!f || f->len < 3) return;
    vec3 n(0.0f);
    for (int i = 0; i < f->len; i++) {
        vec3& p0 = f->verts[i]->co;
        vec3& p1 = f->verts[(i + 1) % f->len]->co;
        vec3& p2 = f->verts[(i + 2) % f->len]->co;
        n += glm::cross(p1 - p0, p2 - p0);
    }
    f->no = glm::length(n) > 1e-8f ? glm::normalize(n) : vec3(0, 1, 0);
}

void BMesh::recalc_all_normals() {
    for (int i = 0; i < face_count; i++) recalc_face_normal(faces[i]);
    for (auto* v : verts) {
        if (v->hide || v->index < 0) continue;
        vec3 n(0.0f);
        BMEdge* eiter = v->e;
        if (eiter) do {
            if (eiter->f1) n += eiter->f1->no;
            if (eiter->f2) n += eiter->f2->no;
            eiter = (eiter->v1 == v) ? eiter->v1_disk_next : eiter->v2_disk_next;
        } while (eiter && eiter != v->e);
        v->no = glm::length(n) > 1e-8f ? glm::normalize(n) : vec3(0, 1, 0);
    }
}

void BMesh::recalc_vertex_normal(BMVert* v) {
    if (!v) return;
    vec3 n(0.0f);
    BMEdge* eiter = v->e;
    if (eiter) do {
        if (eiter->f1) n += eiter->f1->no;
        if (eiter->f2) n += eiter->f2->no;
        eiter = (eiter->v1 == v) ? eiter->v1_disk_next : eiter->v2_disk_next;
    } while (eiter && eiter != v->e);
    v->no = glm::length(n) > 1e-8f ? glm::normalize(n) : vec3(0, 1, 0);
}

void BMesh::select_all(bool state) {
    for (auto* v : verts) v->select = state;
    for (auto* e : edges) e->select = state;
    for (auto* f : faces) f->select = state;
}

void BMesh::select_flush() {}

bool BMesh::select_is_single() const {
    int count = 0;
    for (auto* v : verts) if (v->select) count++;
    return count == 1;
}

BMVert* BMesh::vert_find_nearest(const vec3& pos, float radius) const {
    BMVert* best = nullptr;
    float best_dist = radius;
    for (auto* v : verts) {
        if (v->index < 0 || v->hide) continue;
        float d = glm::length(v->co - pos);
        if (d < best_dist) { best_dist = d; best = v; }
    }
    return best;
}

BMEdge* BMesh::edge_find_nearest(const vec3& pos, float& closest_dist) const {
    BMEdge* best = nullptr;
    closest_dist = 1e10f;
    for (auto* e : edges) {
        if (e->index < 0 || e->hide) continue;
        vec3 a = e->v1->co, b = e->v2->co;
        vec3 ab = b - a;
        float len2 = glm::dot(ab, ab);
        float t = (len2 > 1e-10f) ? glm::clamp(glm::dot(pos - a, ab) / len2, 0.0f, 1.0f) : 0.5f;
        vec3 closest = a + ab * t;
        float d = glm::length(pos - closest);
        if (d < closest_dist) { closest_dist = d; best = e; }
    }
    return best;
}

BMFace* BMesh::face_pick(const vec3& ray_origin, const vec3& ray_dir) const {
    BMFace* best = nullptr;
    float best_t = 1e10f;
    for (auto* f : faces) {
        if (f->index < 0 || f->hide || f->len < 3) continue;
        for (int i = 1; i < f->len - 1; i++) {
            vec3 v0 = f->verts[0]->co, v1 = f->verts[i]->co, v2 = f->verts[i + 1]->co;
            vec3 e1 = v1 - v0, e2 = v2 - v0;
            vec3 h = glm::cross(ray_dir, e2);
            float a = glm::dot(e1, h);
            if (std::abs(a) < 1e-8f) continue;
            float f_r = 1.0f / a;
            vec3 s = ray_origin - v0;
            float u = f_r * glm::dot(s, h);
            if (u < 0.0f || u > 1.0f) continue;
            vec3 q = glm::cross(s, e1);
            float v = f_r * glm::dot(ray_dir, q);
            if (v < 0.0f || u + v > 1.0f) continue;
            float t = f_r * glm::dot(e2, q);
            if (t > 0.0f && t < best_t) { best_t = t; best = f; }
        }
    }
    return best;
}

int BMesh::vert_ring(BMVert* v, std::vector<BMEdge*>& ring) const {
    ring.clear();
    if (!v || !v->e) return 0;
    BMEdge* start = v->e;
    BMEdge* e = start;
    do {
        ring.push_back(e);
        BMEdge* next = (e->v1 == v) ? e->v1_disk_next : e->v2_disk_next;
        if (!next || next == start) break;
        e = next;
    } while (e != start);
    return (int)ring.size();
}

int BMesh::vert_edge_ring(BMVert* v, std::vector<BMEdge*>& ring) const {
    return vert_ring(v, ring);
}

bool BMesh::face_loop_region(BMFace* f, std::vector<BMFace*>& region, int steps) const {
    if (!f) return false;
    region.clear();
    std::unordered_set<int> visited;
    std::queue<std::pair<BMFace*, int>> queue;
    queue.push({f, 0});
    visited.insert(f->index);
    while (!queue.empty()) {
        auto [cf, depth] = queue.front();
        queue.pop();
        region.push_back(cf);
        if (depth >= steps) continue;
        for (int i = 0; i < cf->len; i++) {
            BMEdge* e = cf->edges[i];
            BMFace* adj = (e->f1 == cf) ? e->f2 : e->f1;
            if (adj && adj->index >= 0 && visited.find(adj->index) == visited.end()) {
                visited.insert(adj->index);
                queue.push({adj, depth + 1});
            }
        }
    }
    return !region.empty();
}

vec3 BMesh::calc_face_center(BMFace* f) const {
    if (!f || f->len == 0) return vec3(0.0f);
    vec3 c(0.0f);
    for (int i = 0; i < f->len; i++) c += f->verts[i]->co;
    return c / (float)f->len;
}

vec3 BMesh::calc_edge_center(BMEdge* e) const {
    return e ? (e->v1->co + e->v2->co) * 0.5f : vec3(0.0f);
}

void BMesh::clear() {
    for (auto* v : verts) if (v->index >= 0) remove_vert_raw(v);
    for (auto* e : edges) if (e->index >= 0) remove_edge_raw(e);
    for (auto* f : faces) if (f->index >= 0) remove_face_raw(f);
    verts.clear(); edges.clear(); faces.clear();
    vert_count = edge_count = face_count = 0;
}

void BMesh::validate() const {
    for (auto* v : verts) {
        if (v->index < 0) continue;
        BMEdge* eiter = v->e;
        if (eiter) do {
            assert(eiter->v1 == v || eiter->v2 == v);
            eiter = (eiter->v1 == v) ? eiter->v1_disk_next : eiter->v2_disk_next;
        } while (eiter && eiter != v->e);
    }
}

void BMesh::from_triangle_mesh(const std::vector<vec3>& positions,
                                const std::vector<vec3>& normals,
                                const std::vector<vec2>& uvs,
                                const std::vector<uint32_t>& indices) {
    clear();
    if (positions.empty() || indices.size() < 3) return;

    struct VertKey {
        vec3 pos;
        vec3 norm;
        vec2 uv;
        bool operator==(const VertKey& o) const {
            return glm::length(pos - o.pos) < 1e-5f &&
                   glm::length(norm - o.norm) < 1e-5f &&
                   glm::length(uv - o.uv) < 1e-5f;
        }
    };
    struct VertKeyHash {
        size_t operator()(const VertKey& k) const {
            size_t h = 0;
            auto f = [&](float v) { h = h * 31 + std::hash<int>()((int)(v * 10000)); };
            f(k.pos.x); f(k.pos.y); f(k.pos.z);
            f(k.norm.x); f(k.norm.y); f(k.norm.z);
            f(k.uv.x); f(k.uv.y);
            return h;
        }
    };

    std::unordered_map<VertKey, int, VertKeyHash> vert_map;
    auto get_vert = [&](uint32_t idx) -> int {
        VertKey key;
        key.pos = positions[idx];
        key.norm = (idx < normals.size()) ? normals[idx] : vec3(0, 1, 0);
        key.uv = (idx < uvs.size()) ? uvs[idx] : vec2(0);
        auto it = vert_map.find(key);
        if (it != vert_map.end()) return it->second;
        int vi = (int)verts.size();
        BMVert* v = vert_add(key.pos);
        v->no = key.norm;
        v->uv = key.uv;
        vert_map[key] = vi;
        return vi;
    };

    struct EdgeKey2 { uint32_t a, b; bool operator==(const EdgeKey2& o) const { return a==o.a && b==o.b; } };
    struct EdgeKeyHash2 { size_t operator()(const EdgeKey2& k) const { return std::hash<uint32_t>()(k.a) ^ (std::hash<uint32_t>()(k.b) << 16); } };
    std::unordered_map<EdgeKey2, int, EdgeKeyHash2> edge_map;

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        std::vector<BMVert*> fv;
        for (int j = 0; j < 3; j++) fv.push_back(verts[get_vert(indices[i + j])]);
        std::vector<BMEdge*> fe(3);
        for (int j = 0; j < 3; j++) {
            uint32_t a = indices[i + j], b = indices[i + (j + 1) % 3];
            uint32_t ka = std::min(a, b), kb = std::max(a, b);
            EdgeKey2 ek{ka, kb};
            auto it = edge_map.find(ek);
            if (it != edge_map.end()) {
                fe[j] = edges[it->second];
            } else {
                fe[j] = add_edge_raw(fv[j], fv[(j + 1) % 3]);
                edge_map[ek] = fe[j]->index;
            }
        }
        bool found_face = false;
        for (auto* f : faces) {
            if (f->index < 0 || f->len != 3) continue;
            if (f->verts[0] == fv[0] && f->verts[1] == fv[1] && f->verts[2] == fv[2]) { found_face = true; break; }
        }
        if (!found_face) add_face_raw(fv, fe);
    }
    recalc_all_normals();
}

void BMesh::to_triangle_mesh(std::vector<vec3>& positions,
                              std::vector<vec3>& normals,
                              std::vector<vec2>& uvs,
                              std::vector<vec4>& colors,
                              std::vector<uint32_t>& indices) const {
    positions.clear(); normals.clear(); uvs.clear(); colors.clear(); indices.clear();
    std::unordered_map<int, uint32_t> vert_idx_map;
    for (auto* f : faces) {
        if (f->index < 0 || f->hide || f->len < 3) continue;
        for (int i = 1; i < f->len - 1; i++) {
            BMVert* tri[3] = { f->verts[0], f->verts[i], f->verts[i + 1] };
            for (auto* v : tri) {
                if (vert_idx_map.find(v->index) == vert_idx_map.end()) {
                    vert_idx_map[v->index] = (uint32_t)positions.size();
                    positions.push_back(v->co);
                    normals.push_back(v->no);
                    uvs.push_back(v->uv);
                    colors.push_back(v->color);
                }
                indices.push_back(vert_idx_map[v->index]);
            }
        }
    }
}

BMesh BMesh::clone() const {
    BMesh bm;
    bm.vert_count = vert_count;
    bm.edge_count = edge_count;
    bm.face_count = face_count;
    for (auto* v : verts) {
        auto* nv = bm.vert_add(v->co);
        nv->no = v->no; nv->uv = v->uv; nv->color = v->color; nv->select = v->select;
    }
    for (auto* f : faces) {
        if (f->index < 0) continue;
        std::vector<BMVert*> fv;
        for (int i = 0; i < f->len; i++) fv.push_back(bm.verts[f->verts[i]->index]);
        bm.face_add(fv);
    }
    bm.recalc_all_normals();
    return bm;
}

}
