#include "primitives.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ocp {

Mesh create_cube(float size) {
    Mesh m; m.name = "Cube";
    float s = size * 0.5f;
    vec3 normals[6] = {{0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
    vec3 verts[6][4] = {
        {{-s,-s, s},{ s,-s, s},{ s, s, s},{-s, s, s}},
        {{ s,-s,-s},{-s,-s,-s},{-s, s,-s},{ s, s,-s}},
        {{-s,-s,-s},{-s,-s, s},{-s, s, s},{-s, s,-s}},
        {{ s,-s, s},{ s,-s,-s},{ s, s,-s},{ s, s, s}},
        {{-s, s, s},{ s, s, s},{ s, s,-s},{-s, s,-s}},
        {{-s,-s,-s},{ s,-s,-s},{ s,-s, s},{-s,-s, s}}
    };
    for (int i = 0; i < 6; i++) {
        uint32_t base = (uint32_t)m.vertices.size();
        for (int j = 0; j < 4; j++) {
            m.add_vertex(verts[i][j], normals[i], vec2(j == 0 || j == 3 ? 0 : 1, j < 2 ? 0 : 1));
        }
        m.add_face({base, base+1, base+2, base+3});
    }
    m.compute_normals();
    return m;
}

Mesh create_sphere(float radius, int segments, int rings) {
    Mesh m; m.name = "Sphere";
    for (int r = 0; r <= rings; r++) {
        float phi = (float)M_PI * r / rings;
        for (int s = 0; s <= segments; s++) {
            float theta = 2.0f * (float)M_PI * s / segments;
            vec3 pos(radius * sinf(phi) * cosf(theta), radius * cosf(phi), radius * sinf(phi) * sinf(theta));
            vec3 nrm = glm::normalize(pos);
            vec2 uv((float)s / segments, (float)r / rings);
            m.add_vertex(pos, nrm, uv);
        }
    }
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segments; s++) {
            uint32_t i0 = r * (segments + 1) + s;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + segments + 1;
            uint32_t i3 = i2 + 1;
            m.add_face({i0, i2, i3, i1});
        }
    }
    m.compute_normals();
    return m;
}

Mesh create_cylinder(float radius, float height, int segments) {
    Mesh m; m.name = "Cylinder";
    float hh = height * 0.5f;

    uint32_t top_center = m.add_vertex(vec3(0, hh, 0), vec3(0, 1, 0), vec2(0.5f, 0.5f));
    uint32_t bot_center = m.add_vertex(vec3(0, -hh, 0), vec3(0, -1, 0), vec2(0.5f, 0.5f));

    uint32_t top_ring = (uint32_t)m.vertices.size();
    uint32_t bot_ring = top_ring + (uint32_t)(segments + 1);
    uint32_t side_top = bot_ring + (uint32_t)(segments + 1);
    uint32_t side_bot = side_top + (uint32_t)(segments + 1);

    for (int i = 0; i <= segments; i++) {
        float theta = 2.0f * (float)M_PI * i / segments;
        float ct = cosf(theta), st = sinf(theta);
        float u = (float)i / segments;
        vec3 radial(ct, 0, st);

        m.add_vertex(vec3(ct * radius, hh, st * radius), vec3(0, 1, 0), vec2(u, 1));
        m.add_vertex(vec3(ct * radius, -hh, st * radius), vec3(0, -1, 0), vec2(u, 0));
        m.add_vertex(vec3(ct * radius, hh, st * radius), radial, vec2(u, 1));
        m.add_vertex(vec3(ct * radius, -hh, st * radius), radial, vec2(u, 0));
    }

    for (int i = 0; i < segments; i++) {
        m.add_face({top_center, top_ring + (uint32_t)i, top_ring + (uint32_t)(i + 1)});
        m.add_face({bot_center, bot_ring + (uint32_t)(i + 1), bot_ring + (uint32_t)i});
        uint32_t st_i = side_top + (uint32_t)i;
        uint32_t st_i1 = side_top + (uint32_t)(i + 1);
        uint32_t sb_i = side_bot + (uint32_t)i;
        uint32_t sb_i1 = side_bot + (uint32_t)(i + 1);
        m.add_face({st_i, st_i1, sb_i1, sb_i});
    }

    m.compute_normals();
    return m;
}

Mesh create_cone(float radius, float height, int segments) {
    Mesh m; m.name = "Cone";
    float hh = height * 0.5f;
    uint32_t tip = m.add_vertex(vec3(0, hh, 0), vec3(0, 1, 0));
    uint32_t center = m.add_vertex(vec3(0, -hh, 0), vec3(0, -1, 0));
    for (int i = 0; i <= segments; i++) {
        float theta = 2.0f * (float)M_PI * i / segments;
        float ct = cosf(theta), st = sinf(theta);
        vec3 pos(ct * radius, -hh, st * radius);
        vec3 slope = glm::normalize(vec3(ct * height, radius, st * height));
        m.add_vertex(pos, slope, vec2((float)i / segments, 0));
    }
    for (int i = 0; i < segments; i++) {
        m.add_face({tip, (uint32_t)(i + 2), (uint32_t)(i + 3)});
        m.add_face({center, (uint32_t)(i + 3), (uint32_t)(i + 2)});
    }
    m.compute_normals();
    return m;
}

Mesh create_torus(float major_r, float minor_r, int major_seg, int minor_seg) {
    Mesh m; m.name = "Torus";
    for (int i = 0; i <= major_seg; i++) {
        float theta = 2.0f * (float)M_PI * i / major_seg;
        float ct = cosf(theta), st = sinf(theta);
        for (int j = 0; j <= minor_seg; j++) {
            float phi = 2.0f * (float)M_PI * j / minor_seg;
            float cp = cosf(phi), sp = sinf(phi);
            float x = (major_r + minor_r * cp) * ct;
            float y = minor_r * sp;
            float z = (major_r + minor_r * cp) * st;
            vec3 nrm = glm::normalize(vec3(cp * ct, sp, cp * st));
            vec2 uv((float)i / major_seg, (float)j / minor_seg);
            m.add_vertex(vec3(x, y, z), nrm, uv);
        }
    }
    for (int i = 0; i < major_seg; i++) {
        for (int j = 0; j < minor_seg; j++) {
            uint32_t a = i * (minor_seg + 1) + j;
            uint32_t b = a + minor_seg + 1;
            m.add_face({a, b, b + 1, a + 1});
        }
    }
    m.compute_normals();
    return m;
}

Mesh create_plane(float size, int subdivisions) {
    Mesh m; m.name = "Plane";
    float hs = size * 0.5f;
    for (int z = 0; z <= subdivisions; z++) {
        for (int x = 0; x <= subdivisions; x++) {
            float fx = (float)x / subdivisions, fz = (float)z / subdivisions;
            m.add_vertex(vec3(-hs + fx * size, 0, -hs + fz * size), vec3(0, 1, 0), vec2(fx, fz));
        }
    }
    for (int z = 0; z < subdivisions; z++) {
        for (int x = 0; x < subdivisions; x++) {
            uint32_t i = z * (subdivisions + 1) + x;
            m.add_face({i, i + (uint32_t)(subdivisions + 1), i + (uint32_t)(subdivisions + 2), i + 1});
        }
    }
    m.compute_normals();
    return m;
}

Mesh create_arrow(float length) {
    Mesh m; m.name = "Arrow";
    Mesh shaft = create_cylinder(0.05f, length * 0.7f, 12);
    for (auto& v : shaft.vertices) v.position.y += length * 0.15f;
    Mesh head = create_cone(0.2f, length * 0.3f, 12);
    for (auto& v : head.vertices) v.position.y += length * 0.5f + length * 0.15f;
    uint32_t offset = (uint32_t)m.vertices.size();
    for (auto& v : shaft.vertices) m.add_vertex(v.position, v.normal, v.uv, v.color);
    for (auto& f : shaft.faces) {
        std::vector<uint32_t> fi;
        for (auto& vi : f.vertices) fi.push_back(vi + offset);
        m.add_face(fi);
    }
    offset = (uint32_t)m.vertices.size();
    for (auto& v : head.vertices) m.add_vertex(v.position, v.normal, v.uv, v.color);
    for (auto& f : head.faces) {
        std::vector<uint32_t> fi;
        for (auto& vi : f.vertices) fi.push_back(vi + offset);
        m.add_face(fi);
    }
    m.compute_normals();
    return m;
}

Mesh create_torus_knot(int p, int q, float major_r, float tube_r) {
    Mesh m; m.name = "TorusKnot";
    int segments = 128, tube_seg = 16;
    for (int i = 0; i <= segments; i++) {
        float t = 2.0f * (float)M_PI * i / segments;
        float r = major_r * (2.0f + cosf((float)q * t)) / 3.0f;
        vec3 center(r * cosf((float)p * t), r * sinf((float)p * t), major_r * sinf((float)q * t) / 3.0f);

        float t2 = t + 0.01f;
        float r2 = major_r * (2.0f + cosf((float)q * t2)) / 3.0f;
        vec3 next(r2 * cosf((float)p * t2), r2 * sinf((float)p * t2), major_r * sinf((float)q * t2) / 3.0f);
        vec3 tangent = glm::normalize(next - center);

        vec3 up = vec3(0, 1, 0);
        if (fabsf(glm::dot(tangent, up)) > 0.99f) up = vec3(1, 0, 0);
        vec3 side = glm::normalize(glm::cross(tangent, up));
        vec3 binormal = glm::cross(tangent, side);

        for (int j = 0; j <= tube_seg; j++) {
            float phi = 2.0f * (float)M_PI * j / tube_seg;
            vec3 pos = center + tube_r * (cosf(phi) * side + sinf(phi) * binormal);
            vec3 nrm = glm::normalize(pos - center);
            vec2 uv((float)i / segments, (float)j / tube_seg);
            m.add_vertex(pos, nrm, uv);
        }
    }
    for (int i = 0; i < segments; i++) {
        for (int j = 0; j < tube_seg; j++) {
            uint32_t a = i * (tube_seg + 1) + j;
            uint32_t b = (i + 1) * (tube_seg + 1) + j;
            m.add_face({a, b, b + 1, a + 1});
        }
    }
    m.compute_normals();
    return m;
}

Mesh create_ico_sphere(float radius, int subdivisions) {
    Mesh m; m.name = "IcoSphere";
    float t = (1.0f + sqrtf(5.0f)) / 2.0f;
    float len = sqrtf(1 + t * t);
    vec3 ico_verts[12] = {
        glm::normalize(vec3(-1, t, 0)) * radius, glm::normalize(vec3(1, t, 0)) * radius,
        glm::normalize(vec3(-1, -t, 0)) * radius, glm::normalize(vec3(1, -t, 0)) * radius,
        glm::normalize(vec3(0, -1, t)) * radius, glm::normalize(vec3(0, 1, t)) * radius,
        glm::normalize(vec3(0, -1, -t)) * radius, glm::normalize(vec3(0, 1, -t)) * radius,
        glm::normalize(vec3(t, 0, -1)) * radius, glm::normalize(vec3(t, 0, 1)) * radius,
        glm::normalize(vec3(-t, 0, -1)) * radius, glm::normalize(vec3(-t, 0, 1)) * radius
    };
    for (auto& v : ico_verts) m.add_vertex(v, glm::normalize(v));

    uint32_t faces[20][3] = {
        {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
        {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
        {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
        {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}
    };
    for (auto& f : faces) m.add_face({f[0], f[1], f[2]});

    for (int s = 0; s < subdivisions; s++) m.subdivide();
    for (auto& v : m.vertices) {
        v.normal = glm::normalize(v.position);
        v.position = v.normal * radius;
    }
    m.dirty = true;
    m.compute_normals();
    return m;
}

const std::map<std::string, PrimitiveCreator>& get_primitive_creators() {
    static std::map<std::string, PrimitiveCreator> creators = {
        {"cube", [](float s){ return create_cube(s); }},
        {"sphere", [](float s){ return create_sphere(s * 0.5f, 64, 32); }},
        {"cylinder", [](float s){ return create_cylinder(s * 0.5f, s, 48); }},
        {"cone", [](float s){ return create_cone(s * 0.5f, s, 48); }},
        {"torus", [](float s){ return create_torus(s * 0.4f, s * 0.15f, 48, 24); }},
        {"plane", [](float s){ return create_plane(s * 2.0f); }},
        {"torus_knot", [](float s){ return create_torus_knot(2, 3, s * 0.5f, s * 0.08f); }},
        {"ico_sphere", [](float s){ return create_ico_sphere(s * 0.5f); }},
        {"arrow", [](float s){ return create_arrow(s); }}
    };
    return creators;
}

} // namespace ocp
