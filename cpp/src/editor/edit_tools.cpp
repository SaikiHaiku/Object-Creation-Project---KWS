#include "edit_tools.h"
#include "math_utils.h"
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace ocp {
namespace edit_tools {

// ======================== Internal Helpers ========================

static BMEdge* disk_next(BMVert* v, BMEdge* e) {
    if (!e || !v) return nullptr;
    return (e->v1 == v) ? e->v1_disk_next : e->v2_disk_next;
}

static BMVert* edge_other_vert(BMEdge* e, BMVert* v) {
    if (!e || !v) return nullptr;
    return (e->v1 == v) ? e->v2 : e->v1;
}

static bool edge_has_vert(BMEdge* e, BMVert* v) {
    return e && v && (e->v1 == v || e->v2 == v);
}

static bool is_boundary_edge(BMEdge* e) {
    return e && (!e->f1 || !e->f2);
}

static bool is_boundary_vert(BMVert* v) {
    if (!v || !v->e) return true;
    BMEdge* e = v->e;
    do {
        if (is_boundary_edge(e)) return true;
        e = disk_next(v, e);
    } while (e && e != v->e);
    return false;
}

static void vert_disk_edges(BMVert* v, std::vector<BMEdge*>& out) {
    out.clear();
    if (!v || !v->e) return;
    BMEdge* e = v->e;
    do {
        out.push_back(e);
        e = disk_next(v, e);
    } while (e && e != v->e);
}

static void vert_all_faces(BMVert* v, std::vector<BMFace*>& out) {
    out.clear();
    std::vector<BMEdge*> edgs;
    vert_disk_edges(v, edgs);
    for (BMEdge* e : edgs) {
        if (e->f1) out.push_back(e->f1);
        if (e->f2) out.push_back(e->f2);
    }
}

static BMFace* edge_other_face(BMEdge* e, BMFace* f) {
    if (!e) return nullptr;
    if (e->f1 == f) return e->f2;
    if (e->f2 == f) return e->f1;
    return nullptr;
}

static int face_vert_index(BMFace* f, BMVert* v) {
    if (!f || !v) return -1;
    for (int i = 0; i < f->len; i++) {
        if (f->verts[i] == v) return i;
    }
    return -1;
}

static int face_edge_index(BMFace* f, BMEdge* e) {
    if (!f || !e) return -1;
    for (int i = 0; i < f->len; i++) {
        if (f->edges[i] == e) return i;
    }
    return -1;
}

static BMEdge* find_edge(BMVert* v1, BMVert* v2) {
    if (!v1 || !v2 || !v1->e) return nullptr;
    BMEdge* e = v1->e;
    do {
        if ((e->v1 == v1 && e->v2 == v2) || (e->v1 == v2 && e->v2 == v1))
            return e;
        e = disk_next(v1, e);
    } while (e && e != v1->e);
    return nullptr;
}

static std::vector<BMVert*> collect_sel_verts(BMesh& bm) {
    std::vector<BMVert*> r;
    for (int i = 0; i < bm.vert_count; i++)
        if (bm.verts[i]->select) r.push_back(bm.verts[i]);
    return r;
}

static std::vector<BMEdge*> collect_sel_edges(BMesh& bm) {
    std::vector<BMEdge*> r;
    for (int i = 0; i < bm.edge_count; i++)
        if (bm.edges[i]->select) r.push_back(bm.edges[i]);
    return r;
}

static std::vector<BMFace*> collect_sel_faces(BMesh& bm) {
    std::vector<BMFace*> r;
    for (int i = 0; i < bm.face_count; i++)
        if (bm.faces[i]->select) r.push_back(bm.faces[i]);
    return r;
}

static bool face_has_vert(BMFace* f, BMVert* v) {
    return f && v && face_vert_index(f, v) >= 0;
}

static bool face_has_edge(BMFace* f, BMEdge* e) {
    return f && e && face_edge_index(f, e) >= 0;
}

static vec3 face_center(BMesh& bm, BMFace* f) {
    return bm.calc_face_center(f);
}

static BMVert* shared_vert(BMEdge* e1, BMEdge* e2) {
    if (!e1 || !e2) return nullptr;
    if (edge_has_vert(e1, e2->v1)) return e2->v1;
    if (edge_has_vert(e1, e2->v2)) return e2->v2;
    return nullptr;
}

// Spatial hash for merge operations
static const float MERGE_CELL = 0.1f;

struct SpatialHash {
    float cell_size;
    std::unordered_map<int64_t, std::vector<BMVert*>> grid;

    explicit SpatialHash(float cs = MERGE_CELL) : cell_size(cs) {}

    int64_t key(float x, float y, float z) const {
        int64_t ix = (int64_t)std::floor(x / cell_size);
        int64_t iy = (int64_t)std::floor(y / cell_size);
        int64_t iz = (int64_t)std::floor(z / cell_size);
        return ix * 73856093LL ^ iy * 19349669LL ^ iz * 83492791LL;
    }

    void insert(BMVert* v) {
        grid[key(v->co.x, v->co.y, v->co.z)].push_back(v);
    }

    void find_near(const vec3& pos, float radius, std::vector<BMVert*>& out) const {
        float r2 = radius * radius;
        int64_t cx = (int64_t)std::floor(pos.x / cell_size);
        int64_t cy = (int64_t)std::floor(pos.y / cell_size);
        int64_t cz = (int64_t)std::floor(pos.z / cell_size);
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    int64_t k = (cx + dx) * 73856093LL ^ (cy + dy) * 19349669LL ^ (cz + dz) * 83492791LL;
                    auto it = grid.find(k);
                    if (it != grid.end()) {
                        for (BMVert* v : it->second) {
                            vec3 diff = v->co - pos;
                            float d2 = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
                            if (d2 <= r2)
                                out.push_back(v);
                        }
                    }
                }
            }
        }
    }
};

// Find the ordered boundary loop of a set of faces
static void find_boundary_loop(BMesh& bm, const std::vector<BMFace*>& faces,
                                std::vector<BMVert*>& loop) {
    loop.clear();
    if (faces.empty()) return;

    std::unordered_set<int> face_set;
    for (BMFace* f : faces) face_set.insert(f->index);

    // Find a boundary edge to start
    BMEdge* start_edge = nullptr;
    for (BMFace* f : faces) {
        for (int i = 0; i < f->len; i++) {
            BMEdge* e = f->edges[i];
            BMFace* other = edge_other_face(e, f);
            if (!other || face_set.find(other->index) == face_set.end()) {
                start_edge = e;
                break;
            }
        }
        if (start_edge) break;
    }
    if (!start_edge) return;

    // Walk the boundary loop
    BMVert* start_v = start_edge->v1;
    BMVert* cur_v = start_v;
    BMEdge* cur_e = start_edge;

    do {
        loop.push_back(cur_v);

        // Find the next boundary edge at cur_v
        BMVert* next_v = edge_other_vert(cur_e, cur_v);
        BMEdge* next_e = nullptr;

        std::vector<BMEdge*> ring;
        vert_disk_edges(next_v, ring);
        for (BMEdge* re : ring) {
            if (re == cur_e) continue;
            BMFace* adj = nullptr;
            if (re->f1 && face_set.find(re->f1->index) != face_set.end()) adj = re->f1;
            if (re->f2 && face_set.find(re->f2->index) != face_set.end()) adj = re->f2;
            BMFace* adj2 = edge_other_face(re, adj);
            if (!adj2 || face_set.find(adj2->index) == face_set.end()) {
                next_e = re;
                break;
            }
        }

        if (!next_e) break;
        cur_e = next_e;
        cur_v = next_v;
    } while (cur_v != start_v);
}

// ======================== Selection Tools ========================

void select_vertex_ring(BMesh& bm, BMVert* v, int steps) {
    if (!v) return;
    bm.select_all(false);
    v->select = true;

    // BFS outward from v, each step expands one ring of connected vertices
    std::unordered_set<int> visited;
    visited.insert(v->index);

    std::vector<BMVert*> current_ring;
    current_ring.push_back(v);

    for (int step = 0; step < steps; step++) {
        std::vector<BMVert*> next_ring;
        for (BMVert* cv : current_ring) {
            std::vector<BMEdge*> edgs;
            vert_disk_edges(cv, edgs);
            for (BMEdge* e : edgs) {
                BMVert* ov = edge_other_vert(e, cv);
                if (ov && ov->index >= 0 && visited.find(ov->index) == visited.end()) {
                    visited.insert(ov->index);
                    ov->select = true;
                    next_ring.push_back(ov);
                }
            }
        }
        current_ring = std::move(next_ring);
    }
    bm.select_flush();
}

void select_edge_loop(BMesh& bm, BMEdge* e) {
    if (!e) return;
    bm.select_all(false);

    auto walk = [&](BMEdge* start, BMVert* from_v, BMVert* to_v) {
        BMEdge* cur = start;
        BMVert* prev_v = from_v;
        BMVert* cur_v = to_v;

        while (cur && !is_boundary_edge(cur)) {
            cur->select = true;

            // At cur_v, find the edge that continues the loop
            // It must NOT share a face with cur, but share cur_v
            std::vector<BMEdge*> ring;
            vert_disk_edges(cur_v, ring);

            BMEdge* next = nullptr;
            for (BMEdge* re : ring) {
                if (re == cur) continue;
                bool shares_face = false;
                if (cur->f1 && (cur->f1 == re->f1 || cur->f1 == re->f2)) shares_face = true;
                if (cur->f2 && (cur->f2 == re->f1 || cur->f2 == re->f2)) shares_face = true;
                if (!shares_face) {
                    if (next) { next = nullptr; break; } // ambiguous
                    next = re;
                }
            }

            if (!next || next == start) break;
            if (next->select) break; // loop closed

            prev_v = cur_v;
            cur_v = edge_other_vert(next, cur_v);
            cur = next;
        }
    };

    walk(e, e->v1, e->v2);
    walk(e, e->v2, e->v1);
    bm.select_flush();
}

void select_face_loop(BMesh& bm, BMFace* f) {
    if (!f) return;
    bm.select_all(false);
    f->select = true;

    // BFS through face-adjacent faces, following the loop pattern
    std::unordered_set<int> visited;
    visited.insert(f->index);
    std::queue<BMFace*> queue;
    queue.push(f);

    while (!queue.empty()) {
        BMFace* cur = queue.front();
        queue.pop();

        for (int i = 0; i < cur->len; i++) {
            BMEdge* e = cur->edges[i];
            BMFace* adj = edge_other_face(e, cur);
            if (!adj || adj->index < 0 || visited.count(adj->index)) continue;

            // For a face loop, check that the adjacent face shares exactly one edge
            // and the continuation faces share edges in a ring pattern
            int shared = 0;
            for (int j = 0; j < adj->len; j++) {
                for (int k = 0; k < cur->len; k++) {
                    if (adj->edges[j] == cur->edges[k]) { shared++; break; }
                }
            }
            if (shared == 1) {
                visited.insert(adj->index);
                adj->select = true;
                queue.push(adj);
            }
        }
    }
    bm.select_flush();
}

void select_island(BMesh& bm, BMVert* v) {
    if (!v) return;
    bm.select_all(false);

    // BFS from v through all connected geometry
    std::unordered_set<int> visited;
    std::queue<BMVert*> queue;
    queue.push(v);
    visited.insert(v->index);

    while (!queue.empty()) {
        BMVert* cur = queue.front();
        queue.pop();
        cur->select = true;

        std::vector<BMEdge*> edgs;
        vert_disk_edges(cur, edgs);
        for (BMEdge* e : edgs) {
            e->select = true;
            if (e->f1) e->f1->select = true;
            if (e->f2) e->f2->select = true;

            BMVert* ov = edge_other_vert(e, cur);
            if (ov && ov->index >= 0 && visited.count(ov->index) == 0) {
                visited.insert(ov->index);
                queue.push(ov);
            }
        }
    }
    bm.select_flush();
}

void select_by_angle(BMesh& bm, float angle_threshold_deg) {
    auto sel_faces = collect_sel_faces(bm);
    if (sel_faces.empty()) return;

    float cos_thresh = std::cos(deg2rad(angle_threshold_deg));

    // Collect reference normals from currently selected faces
    std::vector<vec3> refs;
    for (BMFace* f : sel_faces) {
        if (glm::length(f->no) > 1e-8f)
            refs.push_back(glm::normalize(f->no));
    }
    if (refs.empty()) return;

    for (int i = 0; i < bm.face_count; i++) {
        BMFace* f = bm.faces[i];
        if (f->index < 0) continue;
        vec3 n = glm::normalize(f->no);
        for (const vec3& r : refs) {
            if (glm::dot(n, r) >= cos_thresh) {
                f->select = true;
                break;
            }
        }
    }
    bm.select_flush();
}

// ======================== Extrude ========================

ExtrudeResult extrude_region(BMesh& bm, const vec3& offset) {
    ExtrudeResult result;
    auto sel_faces = collect_sel_faces(bm);
    if (sel_faces.empty()) return result;

    // Build face index set
    std::unordered_set<int> face_set;
    for (BMFace* f : sel_faces) face_set.insert(f->index);

    // Map old vertex index -> new vertex
    std::unordered_map<int, BMVert*> vmap;

    auto get_new_vert = [&](BMVert* ov) -> BMVert* {
        auto it = vmap.find(ov->index);
        if (it != vmap.end()) return it->second;
        BMVert* nv = bm.vert_add(ov->co + offset);
        nv->uv = ov->uv;
        nv->color = ov->color;
        vmap[ov->index] = nv;
        result.new_verts.push_back(nv);
        return nv;
    };

    // Find boundary edges of the selection
    std::vector<BMEdge*> boundary;
    for (BMFace* f : sel_faces) {
        for (int i = 0; i < f->len; i++) {
            BMEdge* e = f->edges[i];
            BMFace* other = edge_other_face(e, f);
            if (!other || !face_set.count(other->index)) {
                bool found = false;
                for (BMEdge* be : boundary) {
                    if (be == e) { found = true; break; }
                }
                if (!found) boundary.push_back(e);
            }
        }
    }

    // Create new faces for each selected face
    for (BMFace* f : sel_faces) {
        std::vector<BMVert*> nvs;
        for (int i = 0; i < f->len; i++) {
            nvs.push_back(get_new_vert(f->verts[i]));
        }
        BMFace* nf = bm.face_add(nvs);
        if (nf) {
            nf->select = true;
            result.new_faces.push_back(nf);
        }
    }

    // Create side faces for boundary edges
    for (BMEdge* be : boundary) {
        BMVert* nv1 = get_new_vert(be->v1);
        BMVert* nv2 = get_new_vert(be->v2);
        // Side quad: (v1, v2, nv2, nv1) - but winding must match the selected face
        // Find the selected face on this edge to get winding
        BMFace* sf = nullptr;
        if (be->f1 && face_set.count(be->f1->index)) sf = be->f1;
        else if (be->f2 && face_set.count(be->f2->index)) sf = be->f2;

        if (sf) {
            int i1 = face_vert_index(sf, be->v1);
            int i2 = face_vert_index(sf, be->v2);
            // Ensure winding follows the face
            if ((i1 + 1) % sf->len == i2) {
                std::vector<BMVert*> sv = {be->v1, be->v2, nv2, nv1};
                BMFace* side = bm.face_add(sv);
                if (side) { side->select = true; result.new_faces.push_back(side); }
            } else {
                std::vector<BMVert*> sv = {be->v2, be->v1, nv1, nv2};
                BMFace* side = bm.face_add(sv);
                if (side) { side->select = true; result.new_faces.push_back(side); }
            }
        }
    }

    bm.recalc_all_normals();
    return result;
}

ExtrudeResult extrude_faces(BMesh& bm, const vec3& offset) {
    ExtrudeResult result;
    auto sel_faces = collect_sel_faces(bm);
    if (sel_faces.empty()) return result;

    for (BMFace* f : sel_faces) {
        std::vector<BMVert*> new_verts;
        for (int i = 0; i < f->len; i++) {
            BMVert* nv = bm.vert_add(f->verts[i]->co + offset);
            nv->uv = f->verts[i]->uv;
            nv->color = f->verts[i]->color;
            new_verts.push_back(nv);
            result.new_verts.push_back(nv);
        }

        // Top face
        BMFace* nf = bm.face_add(new_verts);
        if (nf) { nf->select = true; result.new_faces.push_back(nf); }

        // Side faces
        for (int i = 0; i < f->len; i++) {
            int j = (i + 1) % f->len;
            std::vector<BMVert*> sv = {f->verts[i], f->verts[j], new_verts[j], new_verts[i]};
            BMFace* side = bm.face_add(sv);
            if (side) { side->select = true; result.new_faces.push_back(side); }
        }
    }

    bm.recalc_all_normals();
    return result;
}

// ======================== Inset ========================

BMFace* inset_faces(BMesh& bm, float thickness) {
    auto sel_faces = collect_sel_faces(bm);
    if (sel_faces.empty()) return nullptr;

    BMFace* last = nullptr;

    for (BMFace* f : sel_faces) {
        vec3 center = bm.calc_face_center(f);

        std::vector<BMVert*> inner;
        for (int i = 0; i < f->len; i++) {
            vec3 dir = center - f->verts[i]->co;
            float dist = glm::length(dir);
            if (dist < 1e-8f) {
                inner.push_back(f->verts[i]);
                continue;
            }
            float move = std::min(thickness, dist * 0.49f);
            BMVert* nv = bm.vert_add(f->verts[i]->co + glm::normalize(dir) * move);
            nv->uv = f->verts[i]->uv;
            nv->color = f->verts[i]->color;
            inner.push_back(nv);
        }

        // Create inner face
        BMFace* inner_f = bm.face_add(inner);
        if (inner_f) { inner_f->select = true; last = inner_f; }

        // Create connecting quads
        for (int i = 0; i < f->len; i++) {
            int j = (i + 1) % f->len;
            if (f->verts[i] == inner[i] && f->verts[j] == inner[j]) continue;
            std::vector<BMVert*> qv = {f->verts[i], f->verts[j], inner[j], inner[i]};
            // Degeneracy check
            std::set<BMVert*> unique(qv.begin(), qv.end());
            if (unique.size() >= 3) {
                BMFace* qf = bm.face_add(qv);
                if (qf) qf->select = true;
            }
        }

        f->select = false;
    }

    bm.recalc_all_normals();
    return last;
}

// ======================== Bevel ========================

void bevel_edges(BMesh& bm, float amount, int segments) {
    auto sel_edges = collect_sel_edges(bm);
    if (sel_edges.empty() || amount <= 0.0f) return;
    if (segments < 1) segments = 1;

    for (BMEdge* e : sel_edges) {
        if (e->index < 0) continue;

        BMVert* v1 = e->v1;
        BMVert* v2 = e->v2;
        BMFace* f1 = e->f1;
        BMFace* f2 = e->f2;

        if (!f1 && !f2) continue;

        vec3 edge_vec = v2->co - v1->co;
        float edge_len = glm::length(edge_vec);
        if (edge_len < 1e-8f) continue;
        vec3 edge_dir = edge_vec / edge_len;

        // Compute bevel offset direction (perpendicular to edge, toward faces)
        vec3 bevel_normal(0.0f);
        if (f1 && f2) {
            vec3 n1 = glm::normalize(f1->no);
            vec3 n2 = glm::normalize(f2->no);
            bevel_normal = n1 + n2;
        } else if (f1) {
            bevel_normal = f1->no;
        } else {
            bevel_normal = f2->no;
        }
        bevel_normal -= edge_dir * glm::dot(bevel_normal, edge_dir);
        float bn_len = glm::length(bevel_normal);
        if (bn_len < 1e-8f) {
            vec3 up = (std::abs(glm::dot(edge_dir, vec3(0, 1, 0))) < 0.99f)
                ? vec3(0, 1, 0) : vec3(1, 0, 0);
            bevel_normal = glm::cross(edge_dir, up);
        }
        bevel_normal = glm::normalize(bevel_normal);

        // Save face data before edge_kill destroys them
        struct FaceData {
            std::vector<BMVert*> verts;
            bool select_state;
        };
        FaceData fd1, fd2;
        if (f1) {
            fd1.verts.assign(f1->verts, f1->verts + f1->len);
            fd1.select_state = f1->select;
        }
        if (f2) {
            fd2.verts.assign(f2->verts, f2->verts + f2->len);
            fd2.select_state = f2->select;
        }

        // Create rings of vertices at each end of the edge
        int ring_size = segments + 1;
        std::vector<BMVert*> ring1, ring2;

        for (int s = 0; s < ring_size; s++) {
            float t = (segments > 0) ? (float)s / segments : 0.5f;
            float angle = t * 3.14159265f;
            vec3 offset = bevel_normal * (std::sin(angle) * amount);

            BMVert* nv1 = bm.vert_add(v1->co + offset);
            BMVert* nv2 = bm.vert_add(v2->co + offset);
            ring1.push_back(nv1);
            ring2.push_back(nv2);
        }

        e->select = false;

        // Kill the original edge (kills f1 and f2 too)
        bm.edge_kill(e);

        // Rebuild the two original faces, replacing v1/v2 with ring endpoints
        auto rebuild_face = [&](FaceData& fd, BMVert* rv1, BMVert* rv2) {
            if (fd.verts.empty()) return;
            std::vector<BMVert*> new_verts;
            for (BMVert* fv : fd.verts) {
                if (fv == v1) new_verts.push_back(rv1);
                else if (fv == v2) new_verts.push_back(rv2);
                else new_verts.push_back(fv);
            }
            if (new_verts.size() >= 3) {
                BMFace* nf = bm.face_add(new_verts);
                if (nf) nf->select = fd.select_state;
            }
        };

        if (f1) rebuild_face(fd1, ring1[0], ring2[0]);
        if (f2) rebuild_face(fd2, ring1[segments], ring2[segments]);

        // Create edges along the rings
        for (int s = 0; s < ring_size - 1; s++) {
            bm.edge_add(ring1[s], ring1[s + 1]);
            bm.edge_add(ring2[s], ring2[s + 1]);
        }

        // Create edges connecting the two rings
        for (int s = 0; s < ring_size; s++) {
            bm.edge_add(ring1[s], ring2[s]);
        }

        // Create bevel strip faces
        for (int s = 0; s < ring_size - 1; s++) {
            std::vector<BMVert*> fv = {ring1[s], ring2[s], ring2[s + 1], ring1[s + 1]};
            BMFace* sf = bm.face_add(fv);
            if (sf) sf->select = true;
        }

        // Create end cap faces at v1 and v2
        if (ring_size > 1) {
            std::vector<BMVert*> cap1(ring1.rbegin(), ring1.rend());
            BMFace* capf1 = bm.face_add(cap1);
            if (capf1) capf1->select = true;

            std::vector<BMVert*> cap2(ring2.begin(), ring2.end());
            BMFace* capf2 = bm.face_add(cap2);
            if (capf2) capf2->select = true;
        }
    }

    bm.recalc_all_normals();
}

void bevel_vertices(BMesh& bm, float amount, int segments) {
    auto sel_verts = collect_sel_verts(bm);
    if (sel_verts.empty() || amount <= 0.0f) return;
    if (segments < 1) segments = 1;

    for (BMVert* v : sel_verts) {
        if (v->index < 0) continue;

        std::vector<BMFace*> faces;
        vert_all_faces(v, faces);
        if (faces.empty()) continue;

        std::vector<BMEdge*> edgs;
        vert_disk_edges(v, edgs);
        if (edgs.size() < 2) continue;

        int n = (int)edgs.size();
        int ring_size = segments + 1;

        // For each incident edge, create offset vertices
        // Create a ring of vertices around the beveled vertex
        vec3 center = v->co;

        // Create new vertices in a fan around the original position
        std::vector<std::vector<BMVert*>> edge_rings(n);

        for (int i = 0; i < n; i++) {
            BMEdge* ei = edgs[i];
            BMVert* ov = edge_other_vert(ei, v);
            vec3 to_other = glm::normalize(ov->co - center);
            float edge_len = glm::length(ov->co - center);
            float bevel_amt = std::min(amount, edge_len * 0.49f);

            for (int s = 0; s < ring_size; s++) {
                float t = (segments > 0) ? (float)s / segments : 0.5f;
                vec3 pos = center + to_other * (bevel_amt * (0.5f + 0.5f * t));
                BMVert* nv = bm.vert_add(pos);
                edge_rings[i].push_back(nv);
            }
        }

        // Create the bevel fan faces
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            for (int s = 0; s < ring_size - 1; s++) {
                std::vector<BMVert*> fv = {
                    edge_rings[i][s], edge_rings[i][s + 1],
                    edge_rings[j][s + 1], edge_rings[j][s]
                };
                std::set<BMVert*> unique(fv.begin(), fv.end());
                if (unique.size() >= 3) {
                    BMFace* nf = bm.face_add(fv);
                    if (nf) nf->select = true;
                }
            }
        }

        // Kill the original vertex (this kills all its edges and faces)
        bm.vert_kill(v);
    }

    bm.recalc_all_normals();
}

// ======================== Loop Cut ========================

BMEdge* loop_cut(BMesh& bm, BMFace* f, BMVert* v1, BMVert* v2) {
    if (!f || !v1 || !v2) return nullptr;

    // Find the edge ring that goes from v1 through the mesh toward v2
    // Walk from v1 to v2 through the face ring

    // First, split the face at the midpoint of v1-v2
    BMEdge* cut_edge = find_edge(v1, v2);
    if (!cut_edge) return nullptr;

    // Find all faces in the ring by walking perpendicular to v1-v2
    std::vector<BMEdge*> ring_edges;
    std::unordered_set<int> visited_faces;
    std::unordered_set<int> visited_edges;

    BMFace* cur_face = f;
    BMEdge* cur_edge = cut_edge;

    while (cur_face && !visited_faces.count(cur_face->index)) {
        visited_faces.insert(cur_face->index);

        // Find the edge in this face that's opposite to cur_edge
        int ci = face_edge_index(cur_face, cur_edge);
        if (ci < 0) break;

        int opp_i = (ci + cur_face->len / 2) % cur_face->len;
        BMEdge* opp_edge = cur_face->edges[opp_i];

        if (visited_edges.count(opp_edge->index)) break;
        visited_edges.insert(opp_edge->index);
        ring_edges.push_back(opp_edge);

        // Move to the face on the other side of the opposite edge
        BMFace* next_face = edge_other_face(opp_edge, cur_face);
        if (!next_face || visited_faces.count(next_face->index)) break;

        cur_face = next_face;
        cur_edge = opp_edge;
    }

    // Split all edges in the ring
    std::vector<BMVert*> new_verts;
    for (BMEdge* re : ring_edges) {
        BMVert* nv = bm.edge_split_middle(re);
        if (nv) new_verts.push_back(nv);
    }

    bm.recalc_all_normals();
    if (new_verts.size() >= 2) {
        return find_edge(new_verts[0], new_verts[1]);
    }
    return new_verts.empty() ? nullptr : nullptr;
}

// ======================== Knife Cut ========================

void knife_cut(BMesh& bm, const vec3& p1, const vec3& p2) {
    vec3 dir = p2 - p1;
    float dir_len = glm::length(dir);
    if (dir_len < 1e-8f) return;
    dir /= dir_len;

    // For each face, check if the line p1->p2 intersects it
    std::vector<BMFace*> all_faces;
    for (int i = 0; i < bm.face_count; i++) {
        if (bm.faces[i]->index >= 0) all_faces.push_back(bm.faces[i]);
    }

    // Collect intersection points per edge
    std::unordered_map<int, std::vector<BMVert*>> edge_intersections;

    for (BMFace* f : all_faces) {
        if (f->len < 3) continue;

        // Ray-triangle intersection for each triangle fan of the face
        for (int i = 1; i < f->len - 1; i++) {
            vec3 v0 = f->verts[0]->co;
            vec3 v1 = f->verts[i]->co;
            vec3 v2 = f->verts[i + 1]->co;

            // Moller-Trumbore intersection
            vec3 e1 = v1 - v0;
            vec3 e2 = v2 - v0;
            vec3 h = glm::cross(dir, e2);
            float a = glm::dot(e1, h);
            if (std::abs(a) < 1e-8f) continue;

            float f_inv = 1.0f / a;
            vec3 s = p1 - v0;
            float u = f_inv * glm::dot(s, h);
            if (u < 0.0f || u > 1.0f) continue;

            vec3 q = glm::cross(s, e1);
            float v = f_inv * glm::dot(dir, q);
            if (v < 0.0f || u + v > 1.0f) continue;

            float t = f_inv * glm::dot(e2, q);
            if (t < 0.0f || t > dir_len) continue;

            vec3 hit = p1 + dir * t;

            // Find which edges of the face the hit point is closest to
            // and split those edges
            for (int j = 0; j < f->len; j++) {
                BMEdge* edge = f->edges[j];
                BMVert* ev1 = edge->v1;
                BMVert* ev2 = edge->v2;

                // Project hit point onto edge
                vec3 edge_vec = ev2->co - ev1->co;
                float edge_len2 = glm::dot(edge_vec, edge_vec);
                if (edge_len2 < 1e-8f) continue;

                float t_edge = glm::clamp(glm::dot(hit - ev1->co, edge_vec) / edge_len2, 0.0f, 1.0f);
                vec3 closest = ev1->co + edge_vec * t_edge;
                float dist = glm::length(closest - hit);

                if (dist < 0.01f * dir_len) {
                    // This edge is near the intersection - split it
                    if (t_edge > 0.01f && t_edge < 0.99f) {
                        edge_intersections[edge->index].push_back(nullptr); // will split
                    }
                }
            }
        }
    }

    // Split edges that have intersections
    for (auto& [eidx, pts] : edge_intersections) {
        BMEdge* e = bm.get_edge(eidx);
        if (!e || is_boundary_edge(e)) continue;
        BMVert* nv = bm.edge_split_middle(e);
        if (nv) {
            // Project new vertex onto the cut line
            float t = glm::dot(nv->co - p1, dir);
            vec3 target = p1 + dir * glm::clamp(t, 0.0f, dir_len);
            nv->co = target;
        }
    }

    bm.recalc_all_normals();
}

// ======================== Bridge ========================

void bridge_edge_loops(BMesh& bm, BMEdge* e1, BMEdge* e2) {
    if (!e1 || !e2) return;

    // Collect the two edge loops
    auto collect_loop = [&](BMEdge* start) -> std::vector<BMEdge*> {
        std::vector<BMEdge*> loop;
        std::unordered_set<int> visited;
        BMEdge* cur = start;

        while (cur && !visited.count(cur->index)) {
            visited.insert(cur->index);
            loop.push_back(cur);

            // Find the next edge in the loop at the "end" vertex
            BMVert* end_v = cur->v2;
            BMEdge* next = nullptr;

            std::vector<BMEdge*> ring;
            vert_disk_edges(end_v, ring);
            for (BMEdge* re : ring) {
                if (re == cur || visited.count(re->index)) continue;
                // The next edge in a loop doesn't share a face with cur
                bool shares = false;
                if (cur->f1 && (cur->f1 == re->f1 || cur->f1 == re->f2)) shares = true;
                if (cur->f2 && (cur->f2 == re->f1 || cur->f2 == re->f2)) shares = true;
                if (!shares) {
                    if (next) { next = nullptr; break; }
                    next = re;
                }
            }
            cur = next;
        }
        return loop;
    };

    std::vector<BMEdge*> loop1 = collect_loop(e1);
    std::vector<BMEdge*> loop2 = collect_loop(e2);

    if (loop1.empty() || loop2.empty()) return;

    // Bridge: create faces connecting corresponding edges of the two loops
    int count = std::min((int)loop1.size(), (int)loop2.size());
    for (int i = 0; i < count; i++) {
        BMEdge* a = loop1[i];
        BMEdge* b = loop2[i];

        // Create a quad connecting the two edges
        // Try to match orientation
        std::vector<BMVert*> fv = {a->v1, a->v2, b->v2, b->v1};
        std::set<BMVert*> unique(fv.begin(), fv.end());
        if (unique.size() >= 3) {
            BMFace* nf = bm.face_add(fv);
            if (!nf) {
                // Try reversed winding
                std::vector<BMVert*> fv2 = {a->v2, a->v1, b->v1, b->v2};
                bm.face_add(fv2);
            }
        }
    }

    bm.recalc_all_normals();
}

// ======================== Fill ========================

void fill_hole(BMesh& bm, const std::vector<BMVert*>& boundary) {
    if (boundary.size() < 3) return;

    // Create a face from the boundary vertices
    // Try fan triangulation for robustness
    BMFace* f = bm.face_add(boundary);
    if (f) f->select = true;

    bm.recalc_all_normals();
}

void grid_fill(BMesh& bm, const std::vector<BMVert*>& boundary) {
    int n = (int)boundary.size();
    if (n < 4) {
        fill_hole(bm, boundary);
        return;
    }

    // Simple approach: try to fill with a grid
    // If the boundary has even number of vertices, pair them up

    // For a quad-like boundary, create a grid by connecting opposite sides
    if (n % 2 == 0) {
        int half = n / 2;

        // Connect vertex i with vertex i + half
        for (int i = 0; i < half; i++) {
            int j = (i + 1) % half;
            int ji = (i + half) % n;
            int jji = (j + half) % n;

            std::vector<BMVert*> fv = {boundary[i], boundary[j], boundary[jji], boundary[ji]};
            std::set<BMVert*> unique(fv.begin(), fv.end());
            if (unique.size() >= 3) {
                BMFace* f = bm.face_add(fv);
                if (f) f->select = true;
            }
        }
    } else {
        // Odd number: fan from the first vertex
        for (int i = 1; i < n - 1; i++) {
            std::vector<BMVert*> fv = {boundary[0], boundary[i], boundary[i + 1]};
            BMFace* f = bm.face_add(fv);
            if (f) f->select = true;
        }
    }

    bm.recalc_all_normals();
}

// ======================== Merge ========================

void merge_vertices(BMesh& bm, float threshold) {
    auto verts = collect_sel_verts(bm);
    if (verts.empty()) return;

    // Use spatial hash for performance
    SpatialHash sh(threshold * 2.0f);
    for (BMVert* v : bm.verts) {
        if (v->index >= 0) sh.insert(v);
    }

    std::unordered_set<int> merged;

    for (BMVert* v : verts) {
        if (v->index < 0 || merged.count(v->index)) continue;

        std::vector<BMVert*> nearby;
        sh.find_near(v->co, threshold, nearby);

        for (BMVert* other : nearby) {
            if (other == v || other->index < 0) continue;
            if (merged.count(other->index)) continue;
            if (glm::distance(other->co, v->co) > threshold) continue;

            // Don't merge if it would create non-manifold geometry
            // Check if they share an edge
            BMEdge* shared_e = find_edge(v, other);
            if (shared_e) continue;

            bm.vert_splice(other, v);
            merged.insert(other->index);
        }
    }

    bm.recalc_all_normals();
}

void merge_at_center(BMesh& bm) {
    auto verts = collect_sel_verts(bm);
    if (verts.size() < 2) return;

    // Compute center
    vec3 center(0.0f);
    for (BMVert* v : verts) center += v->co;
    center /= (float)verts.size();

    // Keep the first vertex, merge all others into it
    BMVert* target = verts[0];
    target->co = center;

    for (int i = 1; i < (int)verts.size(); i++) {
        if (verts[i]->index >= 0 && verts[i] != target) {
            bm.vert_splice(verts[i], target);
        }
    }

    bm.recalc_all_normals();
}

// ======================== Split ========================

void split_edge(BMesh& bm, BMEdge* e) {
    if (!e) return;
    bm.edge_split_middle(e);
    bm.recalc_all_normals();
}

// ======================== Weld ========================

void weld_verts(BMesh& bm, BMVert* v1, BMVert* v2) {
    if (!v1 || !v2 || v1 == v2) return;
    if (v1->index < 0 || v2->index < 0) return;
    bm.vert_splice(v1, v2);
    bm.recalc_all_normals();
}

// ======================== Rip ========================

void rip_vertex(BMesh& bm, BMVert* v, const vec3& direction) {
    if (!v || v->index < 0) return;

    // Collect faces and edges around the vertex
    std::vector<BMEdge*> edgs;
    vert_disk_edges(v, edgs);

    // Create a duplicate vertex
    BMVert* dup = bm.vert_add(v->co + direction);
    dup->uv = v->uv;
    dup->color = v->color;
    dup->select = true;

    // Transfer every other edge to the duplicate vertex
    bool toggle = false;
    for (BMEdge* e : edgs) {
        if (e->index < 0) continue; // may have been killed
        if (toggle) {
            // Save face data BEFORE killing anything
            BMFace* f1_save = e->f1;
            BMFace* f2_save = e->f2;
            BMVert* other = edge_other_vert(e, v);

            std::vector<BMVert*> f1_verts, f2_verts;
            bool f1_sel = false, f2_sel = false;
            if (f1_save) {
                f1_verts.assign(f1_save->verts, f1_save->verts + f1_save->len);
                f1_sel = f1_save->select;
            }
            if (f2_save) {
                f2_verts.assign(f2_save->verts, f2_save->verts + f2_save->len);
                f2_sel = f2_save->select;
            }

            // Kill faces then the edge
            if (f1_save) bm.face_kill(f1_save);
            if (f2_save) bm.face_kill(f2_save);
            bm.edge_kill(e);

            // Recreate edge with duplicate
            bm.edge_add(dup, other);

            // Recreate faces, replacing v with dup
            auto rebuild = [&](std::vector<BMVert*>& fv, bool sel) {
                if (fv.size() < 3) return;
                for (BMVert*& fv : fv) {
                    if (fv == v) fv = dup;
                }
                std::set<BMVert*> unique(fv.begin(), fv.end());
                if (unique.size() >= 3) {
                    BMFace* nf = bm.face_add(fv);
                    if (nf) nf->select = sel;
                }
            };
            rebuild(f1_verts, f1_sel);
            rebuild(f2_verts, f2_sel);
        }
        toggle = !toggle;
    }

    bm.recalc_all_normals();
}

// ======================== Spin ========================

void spin(BMesh& bm, int steps, float angle, const vec3& center, const vec3& axis) {
    if (steps < 1) return;

    auto sel_verts = collect_sel_verts(bm);
    auto sel_faces = collect_sel_faces(bm);
    if (sel_verts.empty() && sel_faces.empty()) return;

    vec3 ax = glm::normalize(axis);
    float step_angle = angle / steps;

    // Build index->position map for selected verts
    std::vector<BMVert*> original_verts = sel_verts;
    std::vector<BMFace*> original_faces = sel_faces;

    // Map from original vert pointer to its index in the original_verts array
    std::unordered_map<BMVert*, int> orig_vert_idx;
    for (int i = 0; i < (int)original_verts.size(); i++)
        orig_vert_idx[original_verts[i]] = i;

    // For each step, duplicate and rotate the geometry
    std::vector<std::vector<BMVert*>> all_rings;
    all_rings.push_back(original_verts);

    for (int step = 0; step < steps; step++) {
        float a = step_angle * (step + 1);

        mat4 rot(1.0f);
        rot = glm::translate(rot, center);
        rot = glm::rotate(rot, a, ax);
        rot = glm::translate(rot, -center);

        std::vector<BMVert*> new_ring;

        // Rotate vertices from the PREVIOUS ring (which may be the original or a copy)
        for (BMVert* ov : all_rings[step]) {
            vec4 pos = rot * vec4(ov->co, 1.0f);
            BMVert* nv = bm.vert_add(vec3(pos));
            nv->uv = ov->uv;
            nv->color = ov->color;
            nv->select = true;
            new_ring.push_back(nv);
        }
        all_rings.push_back(new_ring);

        // Create connecting faces between previous ring and this ring
        for (BMFace* f : original_faces) {
            std::vector<BMVert*> prev_verts;
            std::vector<BMVert*> curr_verts;

            for (int i = 0; i < f->len; i++) {
                auto it = orig_vert_idx.find(f->verts[i]);
                if (it == orig_vert_idx.end()) continue;
                int vi = it->second;
                prev_verts.push_back(all_rings[step][vi]);
                curr_verts.push_back(new_ring[vi]);
            }

            if (prev_verts.size() >= 3) {
                // Create quad strip for this face edge pair
                for (int i = 0; i < (int)prev_verts.size(); i++) {
                    int j = (i + 1) % (int)prev_verts.size();
                    std::vector<BMVert*> fv = {
                        prev_verts[i], prev_verts[j],
                        curr_verts[j], curr_verts[i]
                    };
                    std::set<BMVert*> unique(fv.begin(), fv.end());
                    if (unique.size() >= 3) {
                        BMFace* nf = bm.face_add(fv);
                        if (nf) nf->select = true;
                    }
                }
            }
        }
    }

    bm.recalc_all_normals();
}

void screw(BMesh& bm, int steps, float pitch, const vec3& center, const vec3& axis) {
    if (steps < 1) return;

    auto sel_verts = collect_sel_verts(bm);
    if (sel_verts.empty()) return;

    vec3 ax = glm::normalize(axis);
    float step_angle = 2.0f * 3.14159265f / steps;
    float step_pitch = pitch / steps;

    std::vector<BMVert*> prev_ring = sel_verts;

    for (int step = 0; step < steps; step++) {
        float a = step_angle * (step + 1);
        float offset = step_pitch * (step + 1);

        mat4 rot(1.0f);
        rot = glm::translate(rot, center);
        rot = glm::rotate(rot, a, ax);
        rot = glm::translate(rot, -center);
        // Add translation along axis
        rot = glm::translate(rot, ax * offset);

        std::vector<BMVert*> new_ring;
        for (BMVert* ov : prev_ring) {
            vec4 pos = rot * vec4(ov->co, 1.0f);
            BMVert* nv = bm.vert_add(vec3(pos));
            nv->uv = ov->uv;
            nv->color = ov->color;
            nv->select = true;
            new_ring.push_back(nv);
        }

        // Connect this ring to the previous ring with quads
        int count = std::min((int)prev_ring.size(), (int)new_ring.size());
        for (int i = 0; i < count; i++) {
            int j = (i + 1) % count;
            std::vector<BMVert*> fv = {prev_ring[i], prev_ring[j],
                                         new_ring[j], new_ring[i]};
            std::set<BMVert*> unique(fv.begin(), fv.end());
            if (unique.size() >= 3) {
                BMFace* nf = bm.face_add(fv);
                if (nf) nf->select = true;
            }
        }

        prev_ring = new_ring;
    }

    bm.recalc_all_normals();
}

// ======================== Solidify ========================

void solidify(BMesh& bm, float thickness) {
    auto sel_faces = collect_sel_faces(bm);
    if (sel_faces.empty()) {
        // Solidify entire mesh
        for (int i = 0; i < bm.face_count; i++) {
            if (bm.faces[i]->index >= 0)
                sel_faces.push_back(bm.faces[i]);
        }
    }
    if (sel_faces.empty()) return;

    // Build set of selected face indices
    std::unordered_set<int> sel_set;
    for (BMFace* f : sel_faces) sel_set.insert(f->index);

    // For each selected face, duplicate vertices and create offset face
    // Then connect border edges
    std::unordered_map<int, BMVert*> vmap;

    for (BMFace* f : sel_faces) {
        std::vector<BMVert*> new_verts;
        for (int i = 0; i < f->len; i++) {
            BMVert* ov = f->verts[i];
            auto it = vmap.find(ov->index);
            BMVert* nv = nullptr;
            if (it != vmap.end()) {
                nv = it->second;
            } else {
                nv = bm.vert_add(ov->co - f->no * thickness);
                nv->uv = ov->uv;
                nv->color = ov->color;
                vmap[ov->index] = nv;
            }
            new_verts.push_back(nv);
        }

        // Create the offset face (reversed winding for correct normals)
        std::vector<BMVert*> rev(new_verts.rbegin(), new_verts.rend());
        BMFace* nf = bm.face_add(rev);
        if (nf) nf->select = true;
    }

    // Connect border edges
    for (BMFace* f : sel_faces) {
        for (int i = 0; i < f->len; i++) {
            BMEdge* e = f->edges[i];
            BMFace* other = edge_other_face(e, f);
            if (!other || !sel_set.count(other->index)) {
                // This is a border edge - create side face
                BMVert* nv1 = vmap[e->v1->index];
                BMVert* nv2 = vmap[e->v2->index];
                if (nv1 && nv2) {
                    std::vector<BMVert*> sv = {e->v1, e->v2, nv2, nv1};
                    std::set<BMVert*> unique(sv.begin(), sv.end());
                    if (unique.size() >= 3) {
                        BMFace* sf = bm.face_add(sv);
                        if (sf) sf->select = true;
                    }
                }
            }
        }
    }

    bm.recalc_all_normals();
}

// ======================== Symmetrize ========================

void symmetrize(BMesh& bm, int axis, float threshold) {
    // axis: 0=X, 1=Y, 2=Z
    if (axis < 0 || axis > 2) return;

    auto all_verts = collect_sel_verts(bm);
    if (all_verts.empty()) {
        for (int i = 0; i < bm.vert_count; i++)
            if (bm.verts[i]->index >= 0) all_verts.push_back(bm.verts[i]);
    }
    if (all_verts.empty()) return;

    // Determine which side to keep (positive side)
    // Delete vertices on the negative side, mirror the positive side
    std::vector<BMVert*> to_kill;
    std::vector<BMVert*> positive_verts;

    for (BMVert* v : all_verts) {
        float val = (axis == 0) ? v->co.x : (axis == 1) ? v->co.y : v->co.z;
        if (val < -threshold) {
            to_kill.push_back(v);
        } else if (val >= -threshold) {
            positive_verts.push_back(v);
        }
    }

    // Kill negative side faces
    for (int i = 0; i < bm.face_count; i++) {
        BMFace* f = bm.faces[i];
        if (f->index < 0) continue;
        bool has_negative = false;
        for (int j = 0; j < f->len; j++) {
            float val = (axis == 0) ? f->verts[j]->co.x :
                        (axis == 1) ? f->verts[j]->co.y : f->verts[j]->co.z;
            if (val < -threshold) { has_negative = true; break; }
        }
        if (has_negative) bm.face_kill(f);
    }

    // Kill negative side vertices
    for (BMVert* v : to_kill) {
        if (v->index >= 0) bm.vert_kill(v);
    }

    // Mirror the positive side
    // Collect remaining selected faces
    std::vector<BMFace*> remain_faces;
    for (int i = 0; i < bm.face_count; i++) {
        if (bm.faces[i]->index >= 0)
            remain_faces.push_back(bm.faces[i]);
    }

    // Create mirrored copies
    std::unordered_map<int, BMVert*> mirror_map;
    for (BMVert* v : positive_verts) {
        if (v->index < 0) continue;
        vec3 mc = v->co;
        if (axis == 0) mc.x = -mc.x;
        else if (axis == 1) mc.y = -mc.y;
        else mc.z = -mc.z;

        // Check if there's already a vertex at the mirror position
        BMVert* existing = bm.vert_find_nearest(mc, threshold);
        if (existing) {
            mirror_map[v->index] = existing;
        } else {
            BMVert* mv = bm.vert_add(mc);
            mv->uv = v->uv;
            mv->color = v->color;
            mirror_map[v->index] = mv;
        }
    }

    // Create mirrored faces
    for (BMFace* f : remain_faces) {
        std::vector<BMVert*> mfv;
        bool valid = true;
        for (int i = 0; i < f->len; i++) {
            auto it = mirror_map.find(f->verts[i]->index);
            if (it != mirror_map.end()) {
                mfv.push_back(it->second);
            } else {
                valid = false;
                break;
            }
        }
        if (valid && mfv.size() >= 3) {
            // Reverse winding for mirror
            std::reverse(mfv.begin(), mfv.end());
            BMFace* mf = bm.face_add(mfv);
            if (mf) mf->select = true;
        }
    }

    bm.recalc_all_normals();
}

void mirror(BMesh& bm, int axis) {
    if (axis < 0 || axis > 2) return;

    auto sel_verts = collect_sel_verts(bm);
    auto sel_faces = collect_sel_faces(bm);

    if (sel_verts.empty()) {
        for (int i = 0; i < bm.vert_count; i++)
            if (bm.verts[i]->index >= 0) sel_verts.push_back(bm.verts[i]);
    }
    if (sel_faces.empty()) {
        for (int i = 0; i < bm.face_count; i++)
            if (bm.faces[i]->index >= 0) sel_faces.push_back(bm.faces[i]);
    }

    // Create mirrored vertices
    std::unordered_map<int, BMVert*> mmap;
    for (BMVert* v : sel_verts) {
        if (v->index < 0) continue;
        vec3 mc = v->co;
        if (axis == 0) mc.x = -mc.x;
        else if (axis == 1) mc.y = -mc.y;
        else mc.z = -mc.z;

        BMVert* mv = bm.vert_add(mc);
        mv->uv = v->uv;
        mv->color = v->color;
        mv->select = true;
        mmap[v->index] = mv;
    }

    // Create mirrored faces
    for (BMFace* f : sel_faces) {
        std::vector<BMVert*> mfv;
        bool valid = true;
        for (int i = 0; i < f->len; i++) {
            auto it = mmap.find(f->verts[i]->index);
            if (it != mmap.end()) {
                mfv.push_back(it->second);
            } else {
                valid = false;
                break;
            }
        }
        if (valid && mfv.size() >= 3) {
            std::reverse(mfv.begin(), mfv.end());
            BMFace* mf = bm.face_add(mfv);
            if (mf) mf->select = true;
        }
    }

    bm.recalc_all_normals();
}

// ======================== Subdivide ========================

void subdivide(BMesh& bm, int cuts) {
    if (cuts < 1) cuts = 1;

    for (int c = 0; c < cuts; c++) {
        auto faces = collect_sel_faces(bm);
        if (faces.empty()) {
            for (int i = 0; i < bm.face_count; i++)
                if (bm.faces[i]->index >= 0) faces.push_back(bm.faces[i]);
        }
        if (faces.empty()) return;

        for (BMFace* f : faces) {
            if (f->index < 0 || f->len < 3) continue;

            // For each face, split all edges at their midpoints,
            // then create new faces

            // Step 1: Split all edges of the face
            std::vector<BMVert*> mid_verts;
            for (int i = 0; i < f->len; i++) {
                BMVert* mv = bm.edge_split_middle(f->edges[i]);
                mid_verts.push_back(mv);
            }

            // Step 2: Create center vertex
            vec3 center(0.0f);
            for (int i = 0; i < f->len; i++) center += f->verts[i]->co;
            for (int i = 0; i < f->len; i++) center += mid_verts[i]->co;
            center /= (float)(f->len * 2);
            BMVert* cv = bm.vert_add(center);
            cv->select = true;

            // Step 3: Create new faces (fan from center)
            for (int i = 0; i < f->len; i++) {
                int j = (i + 1) % f->len;
                std::vector<BMVert*> fv = {f->verts[i], mid_verts[i], cv, mid_verts[j]};
                std::set<BMVert*> unique(fv.begin(), fv.end());
                if (unique.size() >= 3) {
                    BMFace* nf = bm.face_add(fv);
                    if (nf) nf->select = true;
                }
            }
        }
    }

    bm.recalc_all_normals();
}

// ======================== Decimate ========================

void decimate(BMesh& bm, float ratio) {
    if (ratio >= 1.0f || ratio <= 0.0f) return;

    int target_faces = (int)(bm.face_count * ratio);

    while (bm.face_count > target_faces && bm.face_count > 4) {
        // Find the "best" edge to collapse
        // Score: prefer edges where adjacent faces are nearly coplanar
        BMEdge* best_edge = nullptr;
        float best_score = -1e10f;

        for (int i = 0; i < bm.edge_count; i++) {
            BMEdge* e = bm.edges[i];
            if (e->index < 0) continue;
            if (is_boundary_edge(e)) continue;

            BMFace* f1 = e->f1;
            BMFace* f2 = e->f2;
            if (!f1 || !f2) continue;

            // Score based on face angle (prefer collapsing coplanar faces)
            float dot = glm::dot(f1->no, f2->no);
            // Also prefer short edges
            float edge_len = glm::length(e->v2->co - e->v1->co);
            float score = dot - edge_len * 0.1f;

            if (score > best_score) {
                best_score = score;
                best_edge = e;
            }
        }

        if (!best_edge) break;

        // Collapse the edge: merge v2 into v1
        BMVert* v1 = best_edge->v1;
        BMVert* v2 = best_edge->v2;

        // Move v1 to the midpoint
        v1->co = (v1->co + v2->co) * 0.5f;

        bm.vert_splice(v2, v1);
    }

    bm.recalc_all_normals();
}

// ======================== Triangulate ========================

void triangulate(BMesh& bm) {
    auto faces = collect_sel_faces(bm);
    if (faces.empty()) {
        for (int i = 0; i < bm.face_count; i++)
            if (bm.faces[i]->index >= 0) faces.push_back(bm.faces[i]);
    }

    for (BMFace* f : faces) {
        if (f->index < 0 || f->len < 4) continue;
        BMFace* current = f;
        while (current && current->len > 3) {
            current = bm.face_split_tri(current, 0, 2);
        }
    }

    bm.recalc_all_normals();
}

// ======================== Untriangulate ========================

void untriangulate(BMesh& bm, float angle_threshold_deg) {
    float cos_thresh = std::cos(deg2rad(angle_threshold_deg));

    bool merged = true;
    while (merged) {
        merged = false;
        for (int i = 0; i < bm.edge_count; i++) {
            BMEdge* e = bm.edges[i];
            if (e->index < 0) continue;
            if (is_boundary_edge(e)) continue;

            BMFace* f1 = e->f1;
            BMFace* f2 = e->f2;
            if (!f1 || !f2) continue;
            if (f1->len != 3 || f2->len != 3) continue;

            // Check if the two triangles are coplanar enough to merge
            float dot = glm::dot(f1->no, f2->no);
            if (dot < cos_thresh) continue;

            // Check that the resulting quad would be convex
            // Find the 4 vertices of the potential quad
            BMVert* v1 = e->v1;
            BMVert* v2 = e->v2;
            BMVert* ov1 = nullptr, *ov2 = nullptr;

            for (int j = 0; j < f1->len; j++) {
                if (f1->verts[j] != v1 && f1->verts[j] != v2) {
                    ov1 = f1->verts[j];
                    break;
                }
            }
            for (int j = 0; j < f2->len; j++) {
                if (f2->verts[j] != v1 && f2->verts[j] != v2) {
                    ov2 = f2->verts[j];
                    break;
                }
            }

            if (!ov1 || !ov2) continue;

            // Merge: kill the shared edge and both faces, create a quad
            bm.face_kill(f1);
            bm.face_kill(f2);

            std::vector<BMVert*> qv = {ov1, v1, ov2, v2};
            std::set<BMVert*> unique(qv.begin(), qv.end());
            if (unique.size() >= 4) {
                BMFace* qf = bm.face_add(qv);
                if (qf) {
                    qf->select = true;
                    merged = true;
                    break;
                }
            }
        }
    }

    bm.recalc_all_normals();
}

// ======================== Relax ========================

void relax(BMesh& bm, float factor, int iterations) {
    for (int iter = 0; iter < iterations; iter++) {
        std::unordered_map<int, vec3> displacements;

        for (int i = 0; i < bm.vert_count; i++) {
            BMVert* v = bm.verts[i];
            if (v->index < 0) continue;
            if (is_boundary_vert(v)) continue;

            // Compute average position of connected vertices
            vec3 avg(0.0f);
            int count = 0;
            std::vector<BMEdge*> edgs;
            vert_disk_edges(v, edgs);
            for (BMEdge* e : edgs) {
                BMVert* ov = edge_other_vert(e, v);
                if (ov && ov->index >= 0) {
                    avg += ov->co;
                    count++;
                }
            }

            if (count > 0) {
                avg /= (float)count;
                displacements[v->index] = (avg - v->co) * factor;
            }
        }

        for (auto& [idx, disp] : displacements) {
            BMVert* v = bm.get_vert(idx);
            if (v && v->index >= 0) {
                v->co += disp;
            }
        }
    }

    bm.recalc_all_normals();
}

void smooth(BMesh& bm, float factor, int iterations) {
    // Smooth is similar to relax but also considers Laplacian weighting
    for (int iter = 0; iter < iterations; iter++) {
        std::unordered_map<int, vec3> new_positions;

        for (int i = 0; i < bm.vert_count; i++) {
            BMVert* v = bm.verts[i];
            if (v->index < 0) continue;
            if (is_boundary_vert(v)) {
                new_positions[v->index] = v->co;
                continue;
            }

            vec3 avg(0.0f);
            int count = 0;
            std::vector<BMEdge*> edgs;
            vert_disk_edges(v, edgs);
            for (BMEdge* e : edgs) {
                BMVert* ov = edge_other_vert(e, v);
                if (ov && ov->index >= 0) {
                    avg += ov->co;
                    count++;
                }
            }

            if (count > 0) {
                avg /= (float)count;
                new_positions[v->index] = v->co + (avg - v->co) * factor;
            } else {
                new_positions[v->index] = v->co;
            }
        }

        for (auto& [idx, pos] : new_positions) {
            BMVert* v = bm.get_vert(idx);
            if (v && v->index >= 0) v->co = pos;
        }
    }

    bm.recalc_all_normals();
}

// ======================== Normals ========================

void recalculate_normals(BMesh& bm) {
    bm.recalc_all_normals();
}

void flip_normals(BMesh& bm) {
    for (int i = 0; i < bm.face_count; i++) {
        BMFace* f = bm.faces[i];
        if (f->index < 0) continue;

        // Reverse vertex order
        for (int j = 0; j < f->len / 2; j++) {
            std::swap(f->verts[j], f->verts[f->len - 1 - j]);
        }

        // Reverse edge order
        for (int j = 0; j < f->len / 2; j++) {
            std::swap(f->edges[j], f->edges[f->len - 1 - j]);
        }

        // Flip normal
        f->no = -f->no;
    }

    // Flip vertex normals
    for (int i = 0; i < bm.vert_count; i++) {
        bm.verts[i]->no = -bm.verts[i]->no;
    }
}

} // namespace edit_tools
} // namespace ocp
