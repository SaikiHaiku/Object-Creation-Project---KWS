#include "sculpt_tools.h"
#include <cmath>
#include <unordered_map>
#include <algorithm>

namespace ocp {
namespace sculpt {

static std::unordered_map<BMVert*, float> mask_weights;

static float compute_falloff(float dist, float radius, float focal) {
    float t = std::max(0.0f, 1.0f - dist / radius);
    return std::pow(t, 1.0f + focal * 3.0f);
}

static void apply_symmetry(vec3& pos, const BrushSettings& settings) {
    if (settings.use_symmetry_x) pos.x = -pos.x;
    if (settings.use_symmetry_y) pos.y = -pos.y;
    if (settings.use_symmetry_z) pos.z = -pos.z;
}

static void mirror_pos(vec3& pos, const BrushSettings& settings, bool back) {
    vec3 m = pos;
    if (settings.use_symmetry_x) m.x = -m.x;
    if (settings.use_symmetry_y) m.y = -m.y;
    if (settings.use_symmetry_z) m.z = -m.z;
    if (back) pos = m;
}

static float get_mask(BMVert* v) {
    auto it = mask_weights.find(v);
    return (it != mask_weights.end()) ? it->second : 0.0f;
}

static void set_mask(BMVert* v, float w) {
    mask_weights[v] = std::clamp(w, 0.0f, 1.0f);
}

static bool is_visible(BMVert* v) {
    return v && !v->hide;
}

static void brush_draw(BMesh& bm, const vec3& hit_pos, const vec3& hit_normal,
                       const BrushSettings& settings) {
    float radius = settings.radius;
    float strength = settings.strength;
    float focal = settings.focal;

    for (int i = 0; i < bm.vert_count; ++i) {
        BMVert* v = bm.get_vert(i);
        if (!is_visible(v)) continue;
        if (get_mask(v) >= 1.0f) continue;

        float dist = glm::length(v->co - hit_pos);
        if (dist >= radius) continue;

        float falloff = compute_falloff(dist, radius, focal);
        float factor = falloff * strength * (1.0f - get_mask(v));
        v->co += hit_normal * factor;

        // Apply symmetry
        if (settings.use_symmetry_x || settings.use_symmetry_y || settings.use_symmetry_z) {
            vec3 sym = v->co;
            mirror_pos(sym, settings, true);
            // Find or create symmetric vertex
            BMVert* sv = bm.vert_find_nearest(sym, radius * 0.1f);
            if (sv && sv != v && is_visible(sv) && get_mask(sv) < 1.0f) {
                sv->co += hit_normal * factor;
            }
        }
    }
}

static void brush_clay(BMesh& bm, const vec3& hit_pos, const vec3& hit_normal,
                       const BrushSettings& settings) {
    float radius = settings.radius;
    float strength = settings.strength;
    float focal = settings.focal;

    // Clay moves vertices toward a plane at hit_pos offset along normal
    vec3 plane_point = hit_pos + hit_normal * strength * 0.1f;

    for (int i = 0; i < bm.vert_count; ++i) {
        BMVert* v = bm.get_vert(i);
        if (!is_visible(v)) continue;
        if (get_mask(v) >= 1.0f) continue;

        float dist = glm::length(v->co - hit_pos);
        if (dist >= radius) continue;

        float falloff = compute_falloff(dist, radius, focal);
        float factor = falloff * strength * (1.0f - get_mask(v));

        // Project vertex onto plane along normal
        float t = glm::dot(plane_point - v->co, hit_normal);
        vec3 target = v->co + hit_normal * t;
        v->co = glm::mix(v->co, target, factor);

        // Symmetry
        if (settings.use_symmetry_x || settings.use_symmetry_y || settings.use_symmetry_z) {
            vec3 sym = v->co;
            mirror_pos(sym, settings, true);
            BMVert* sv = bm.vert_find_nearest(sym, radius * 0.1f);
            if (sv && sv != v && is_visible(sv) && get_mask(sv) < 1.0f) {
                float st = glm::dot(plane_point - sv->co, hit_normal);
                vec3 starget = sv->co + hit_normal * st;
                sv->co = glm::mix(sv->co, starget, factor);
            }
        }
    }
}

static void brush_inflate(BMesh& bm, const vec3& hit_pos, const vec3& hit_normal,
                          const BrushSettings& settings) {
    float radius = settings.radius;
    float strength = settings.strength;
    float focal = settings.focal;

    for (int i = 0; i < bm.vert_count; ++i) {
        BMVert* v = bm.get_vert(i);
        if (!is_visible(v)) continue;
        if (get_mask(v) >= 1.0f) continue;

        float dist = glm::length(v->co - hit_pos);
        if (dist >= radius) continue;

        float falloff = compute_falloff(dist, radius, focal);
        float factor = falloff * strength * (1.0f - get_mask(v));
        v->co += v->no * factor;

        // Symmetry
        if (settings.use_symmetry_x || settings.use_symmetry_y || settings.use_symmetry_z) {
            vec3 sym = v->co;
            mirror_pos(sym, settings, true);
            BMVert* sv = bm.vert_find_nearest(sym, radius * 0.1f);
            if (sv && sv != v && is_visible(sv) && get_mask(sv) < 1.0f) {
                sv->co += sv->no * factor;
            }
        }
    }
}

static void brush_smooth(BMesh& bm, const vec3& hit_pos, const vec3& hit_normal,
                         const BrushSettings& settings) {
    float radius = settings.radius;
    float strength = settings.strength;
    float focal = settings.focal;

    // Collect affected vertices and their neighbors
    std::vector<BMVert*> affected;
    for (int i = 0; i < bm.vert_count; ++i) {
        BMVert* v = bm.get_vert(i);
        if (!is_visible(v)) continue;
        float dist = glm::length(v->co - hit_pos);
        if (dist < radius) affected.push_back(v);
    }

    // For each affected vertex, compute average of neighbors
    for (BMVert* v : affected) {
        if (get_mask(v) >= 1.0f) continue;

        // Gather edge-connected vertices
        std::vector<BMVert*> neighbors;
        BMEdge* e = v->e;
        if (e) {
            do {
                if (e->v1 == v) {
                    if (is_visible(e->v2)) neighbors.push_back(e->v2);
                } else if (e->v2 == v) {
                    if (is_visible(e->v1)) neighbors.push_back(e->v1);
                }
                e = (e->v1 == v) ? e->v1_disk_next : e->v2_disk_next;
            } while (e && e != v->e);
        }

        if (neighbors.empty()) continue;

        vec3 avg(0.0f);
        for (BMVert* n : neighbors) avg += n->co;
        avg /= static_cast<float>(neighbors.size());

        float dist = glm::length(v->co - hit_pos);
        float falloff = compute_falloff(dist, radius, focal);
        float factor = falloff * strength * (1.0f - get_mask(v));
        v->co = glm::mix(v->co, avg, factor);

        // Symmetry
        if (settings.use_symmetry_x || settings.use_symmetry_y || settings.use_symmetry_z) {
            vec3 sym = v->co;
            mirror_pos(sym, settings, true);
            BMVert* sv = bm.vert_find_nearest(sym, radius * 0.1f);
            if (sv && sv != v && is_visible(sv) && get_mask(sv) < 1.0f) {
                // Also smooth the symmetric vertex
                std::vector<BMVert*> sn;
                BMEdge* se = sv->e;
                if (se) {
                    do {
                        if (se->v1 == sv) {
                            if (is_visible(se->v2)) sn.push_back(se->v2);
                        } else if (se->v2 == sv) {
                            if (is_visible(se->v1)) sn.push_back(se->v1);
                        }
                        se = (se->v1 == sv) ? se->v1_disk_next : se->v2_disk_next;
                    } while (se && se != sv->e);
                }
                if (!sn.empty()) {
                    vec3 savg(0.0f);
                    for (BMVert* n : sn) savg += n->co;
                    savg /= static_cast<float>(sn.size());
                    sv->co = glm::mix(sv->co, savg, factor);
                }
            }
        }
    }
}

static void brush_flatten(BMesh& bm, const vec3& hit_pos, const vec3& hit_normal,
                          const BrushSettings& settings) {
    float radius = settings.radius;
    float strength = settings.strength;
    float focal = settings.focal;

    // Compute average plane from vertices within radius
    vec3 avg_pos(0.0f);
    vec3 avg_normal(0.0f);
    int count = 0;
    for (int i = 0; i < bm.vert_count; ++i) {
        BMVert* v = bm.get_vert(i);
        if (!is_visible(v)) continue;
        float dist = glm::length(v->co - hit_pos);
        if (dist < radius) {
            avg_pos += v->co;
            avg_normal += v->no;
            ++count;
        }
    }
    if (count == 0) return;
    avg_pos /= static_cast<float>(count);
    avg_normal = glm::normalize(avg_normal);

    for (int i = 0; i < bm.vert_count; ++i) {
        BMVert* v = bm.get_vert(i);
        if (!is_visible(v)) continue;
        if (get_mask(v) >= 1.0f) continue;

        float dist = glm::length(v->co - hit_pos);
        if (dist >= radius) continue;

        // Project vertex onto average plane
        float t = glm::dot(avg_pos - v->co, avg_normal);
        vec3 target = v->co + avg_normal * t;

        float falloff = compute_falloff(dist, radius, focal);
        float factor = falloff * strength * (1.0f - get_mask(v));
        v->co = glm::mix(v->co, target, factor);

        // Symmetry
        if (settings.use_symmetry_x || settings.use_symmetry_y || settings.use_symmetry_z) {
            vec3 sym = v->co;
            mirror_pos(sym, settings, true);
            BMVert* sv = bm.vert_find_nearest(sym, radius * 0.1f);
            if (sv && sv != v && is_visible(sv) && get_mask(sv) < 1.0f) {
                float st = glm::dot(avg_pos - sv->co, avg_normal);
                vec3 starget = sv->co + avg_normal * st;
                sv->co = glm::mix(sv->co, starget, factor);
            }
        }
    }
}

static void brush_grab(BMesh& bm, const vec3& hit_pos, const vec3& hit_normal,
                       const BrushSettings& settings) {
    float radius = settings.radius;
    float strength = settings.strength;
    float focal = settings.focal;

    // Grab offset: move all vertices by the same amount (strength along normal)
    vec3 offset = hit_normal * strength;

    for (int i = 0; i < bm.vert_count; ++i) {
        BMVert* v = bm.get_vert(i);
        if (!is_visible(v)) continue;
        if (get_mask(v) >= 1.0f) continue;

        float dist = glm::length(v->co - hit_pos);
        if (dist >= radius) continue;

        float falloff = compute_falloff(dist, radius, focal);
        float factor = falloff * (1.0f - get_mask(v));
        v->co += offset * factor;

        // Symmetry
        if (settings.use_symmetry_x || settings.use_symmetry_y || settings.use_symmetry_z) {
            vec3 sym = v->co;
            mirror_pos(sym, settings, true);
            BMVert* sv = bm.vert_find_nearest(sym, radius * 0.1f);
            if (sv && sv != v && is_visible(sv) && get_mask(sv) < 1.0f) {
                sv->co += offset * factor;
            }
        }
    }
}

static void brush_crease(BMesh& bm, const vec3& hit_pos, const vec3& hit_normal,
                         const BrushSettings& settings) {
    float radius = settings.radius;
    float strength = settings.strength;
    float focal = settings.focal;

    // Find two closest edges to define a center line
    float best1 = radius * 2.0f, best2 = radius * 2.0f;
    BMEdge* e1 = nullptr;
    BMEdge* e2 = nullptr;

    for (int i = 0; i < bm.edge_count; ++i) {
        BMEdge* e = bm.get_edge(i);
        if (!e || e->hide) continue;

        vec3 mid = (e->v1->co + e->v2->co) * 0.5f;
        float d = glm::length(mid - hit_pos);
        if (d < best1) {
            best2 = best1; e2 = e1;
            best1 = d; e1 = e;
        } else if (d < best2) {
            best2 = d; e2 = e;
        }
    }

    if (!e1 || !e2) return;

    vec3 line_mid = ((e1->v1->co + e1->v2->co) + (e2->v1->co + e2->v2->co)) * 0.25f;
    vec3 line_dir = glm::normalize(((e1->v1->co + e1->v2->co) * 0.5f) -
                                   ((e2->v1->co + e2->v2->co) * 0.5f));

    for (int i = 0; i < bm.vert_count; ++i) {
        BMVert* v = bm.get_vert(i);
        if (!is_visible(v)) continue;
        if (get_mask(v) >= 1.0f) continue;

        float dist = glm::length(v->co - hit_pos);
        if (dist >= radius) continue;

        // Project vertex onto line
        vec3 to_v = v->co - line_mid;
        float along = glm::dot(to_v, line_dir);
        vec3 on_line = line_mid + line_dir * along;
        vec3 to_line = on_line - v->co;
        float d_to_line = glm::length(to_line);

        if (d_to_line < 1e-6f) continue;

        float falloff = compute_falloff(dist, radius, focal);
        float factor = falloff * strength * (1.0f - get_mask(v));
        v->co += glm::normalize(to_line) * factor * std::min(d_to_line, radius * 0.5f);

        // Symmetry
        if (settings.use_symmetry_x || settings.use_symmetry_y || settings.use_symmetry_z) {
            vec3 sym = v->co;
            mirror_pos(sym, settings, true);
            BMVert* sv = bm.vert_find_nearest(sym, radius * 0.1f);
            if (sv && sv != v && is_visible(sv) && get_mask(sv) < 1.0f) {
                vec3 sto_v = sv->co - line_mid;
                float salong = glm::dot(sto_v, line_dir);
                vec3 son_line = line_mid + line_dir * salong;
                vec3 sto_line = son_line - sv->co;
                float sd_to_line = glm::length(sto_line);
                if (sd_to_line > 1e-6f) {
                    sv->co += glm::normalize(sto_line) * factor * std::min(sd_to_line, radius * 0.5f);
                }
            }
        }
    }
}

static void brush_pinch(BMesh& bm, const vec3& hit_pos, const vec3& hit_normal,
                        const BrushSettings& settings) {
    float radius = settings.radius;
    float strength = settings.strength;
    float focal = settings.focal;

    for (int i = 0; i < bm.vert_count; ++i) {
        BMVert* v = bm.get_vert(i);
        if (!is_visible(v)) continue;
        if (get_mask(v) >= 1.0f) continue;

        float dist = glm::length(v->co - hit_pos);
        if (dist >= radius) continue;

        vec3 to_center = hit_pos - v->co;
        float falloff = compute_falloff(dist, radius, focal);
        float factor = falloff * strength * (1.0f - get_mask(v));
        v->co += to_center * factor;

        // Symmetry
        if (settings.use_symmetry_x || settings.use_symmetry_y || settings.use_symmetry_z) {
            vec3 sym = v->co;
            mirror_pos(sym, settings, true);
            BMVert* sv = bm.vert_find_nearest(sym, radius * 0.1f);
            if (sv && sv != v && is_visible(sv) && get_mask(sv) < 1.0f) {
                vec3 sto_center = hit_pos - sv->co;
                sv->co += sto_center * factor;
            }
        }
    }
}

static void brush_mask(BMesh& bm, const vec3& hit_pos, const vec3& hit_normal,
                       const BrushSettings& settings) {
    float radius = settings.radius;
    float strength = settings.strength;
    float focal = settings.focal;

    for (int i = 0; i < bm.vert_count; ++i) {
        BMVert* v = bm.get_vert(i);
        if (!is_visible(v)) continue;

        float dist = glm::length(v->co - hit_pos);
        if (dist >= radius) continue;

        float falloff = compute_falloff(dist, radius, focal);
        float delta = falloff * strength;
        float current = get_mask(v);
        set_mask(v, std::min(1.0f, current + delta));

        // Symmetry
        if (settings.use_symmetry_x || settings.use_symmetry_y || settings.use_symmetry_z) {
            vec3 sym = v->co;
            mirror_pos(sym, settings, true);
            BMVert* sv = bm.vert_find_nearest(sym, radius * 0.1f);
            if (sv && sv != v && is_visible(sv)) {
                set_mask(sv, std::min(1.0f, get_mask(sv) + delta));
            }
        }
    }
}

static void brush_smooth_mask(BMesh& bm, const vec3& hit_pos, const vec3& hit_normal,
                              const BrushSettings& settings) {
    float radius = settings.radius;
    float strength = settings.strength;
    float focal = settings.focal;

    // Collect vertices within radius
    std::vector<BMVert*> affected;
    for (int i = 0; i < bm.vert_count; ++i) {
        BMVert* v = bm.get_vert(i);
        if (!is_visible(v)) continue;
        float dist = glm::length(v->co - hit_pos);
        if (dist < radius) affected.push_back(v);
    }

    // Smooth mask weights
    for (BMVert* v : affected) {
        std::vector<BMVert*> neighbors;
        BMEdge* e = v->e;
        if (e) {
            do {
                if (e->v1 == v) {
                    if (is_visible(e->v2)) neighbors.push_back(e->v2);
                } else if (e->v2 == v) {
                    if (is_visible(e->v1)) neighbors.push_back(e->v1);
                }
                e = (e->v1 == v) ? e->v1_disk_next : e->v2_disk_next;
            } while (e && e != v->e);
        }

        if (neighbors.empty()) continue;

        float avg = 0.0f;
        for (BMVert* n : neighbors) avg += get_mask(n);
        avg /= static_cast<float>(neighbors.size());

        float dist = glm::length(v->co - hit_pos);
        float falloff = compute_falloff(dist, radius, focal);
        float factor = falloff * strength;
        float current = get_mask(v);
        set_mask(v, current + (avg - current) * factor);

        // Symmetry
        if (settings.use_symmetry_x || settings.use_symmetry_y || settings.use_symmetry_z) {
            vec3 sym = v->co;
            mirror_pos(sym, settings, true);
            BMVert* sv = bm.vert_find_nearest(sym, radius * 0.1f);
            if (sv && sv != v && is_visible(sv)) {
                float savg = 0.0f;
                std::vector<BMVert*> sn;
                BMEdge* se = sv->e;
                if (se) {
                    do {
                        if (se->v1 == sv) {
                            if (is_visible(se->v2)) sn.push_back(se->v2);
                        } else if (se->v2 == sv) {
                            if (is_visible(se->v1)) sn.push_back(se->v1);
                        }
                        se = (se->v1 == sv) ? se->v1_disk_next : se->v2_disk_next;
                    } while (se && se != sv->e);
                }
                if (!sn.empty()) {
                    for (BMVert* n : sn) savg += get_mask(n);
                    savg /= static_cast<float>(sn.size());
                    float sc = get_mask(sv);
                    set_mask(sv, sc + (savg - sc) * factor);
                }
            }
        }
    }
}

void apply_brush(BMesh& bm, const vec3& hit_pos, const vec3& hit_normal,
                 const BrushSettings& settings) {
    switch (settings.type) {
        case BrushType::Draw:       brush_draw(bm, hit_pos, hit_normal, settings); break;
        case BrushType::Clay:       brush_clay(bm, hit_pos, hit_normal, settings); break;
        case BrushType::Inflate:    brush_inflate(bm, hit_pos, hit_normal, settings); break;
        case BrushType::Smooth:     brush_smooth(bm, hit_pos, hit_normal, settings); break;
        case BrushType::Flatten:    brush_flatten(bm, hit_pos, hit_normal, settings); break;
        case BrushType::Grab:       brush_grab(bm, hit_pos, hit_normal, settings); break;
        case BrushType::Crease:     brush_crease(bm, hit_pos, hit_normal, settings); break;
        case BrushType::Pinch:      brush_pinch(bm, hit_pos, hit_normal, settings); break;
        case BrushType::Mask:       brush_mask(bm, hit_pos, hit_normal, settings); break;
        case BrushType::SmoothMask: brush_smooth_mask(bm, hit_pos, hit_normal, settings); break;
    }

    bm.recalc_all_normals();
}

void smooth_vertices(BMesh& bm, const vec3& center, float radius, float strength) {
    BrushSettings s;
    s.type = BrushType::Smooth;
    s.radius = radius;
    s.strength = strength;
    s.focal = 0.5f;
    apply_brush(bm, center, vec3(0.0f, 1.0f, 0.0f), s);
}

} // namespace sculpt
} // namespace ocp
