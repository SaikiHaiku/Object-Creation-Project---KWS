#include "ai_engine.h"
#include "primitives.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cctype>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ocp {

static std::string to_lower(const std::string& s) {
    std::string r = s; std::transform(r.begin(), r.end(), r.begin(), ::tolower); return r;
}

static bool contains_word(const std::string& text, const std::string& word) {
    std::string lt = to_lower(text);
    std::string lw = to_lower(word);
    return lt.find(lw) != std::string::npos;
}

std::mt19937& ProceduralEngine::rng() { return rng_engine; }

// PromptEngine
ParsedPrompt PromptEngine::parse(const std::string& prompt) {
    ParsedPrompt p; p.original = prompt;
    std::string lp = to_lower(prompt);

    struct { const char* name; const char* keywords; } scenes[] = {
        {"house", "house maison home"}, {"tree", "tree arbre"},
        {"car", "car voiture auto automobile"}, {"robot", "robot"},
        {"castle", "castle chateau"}, {"spaceship", "spaceship vaisseau ship rocket"},
        {"sword", "sword epee blade"}, {"chair", "chair chaise"},
        {"table", "table"}, {"star", "star etoile"},
        {"heart", "heart coeur"}, {"mountain", "mountain montagne"},
        {"snowman", "snowman bonhomme"}, {"diamond", "diamond diamant"},
        {"spiral", "spiral"}, {"dna", "dna adn helix"},
        {"skull", "skull crane tete de mort"},
        {"airplane", "airplane avion plane jet"},
        {"boat", "boat bateau ship voilier"},
        {"door", "door porte"},
        {"window", "window fenetre"},
        {"shelf", "shelf etagere"},
        {"lamp", "lamp lumiere lampe light"},
        {"brain", "brain cerveau"},
        {"flower", "flower fleur rose"},
        {"bone", "bone os"},
        {"key", "key cle"},
        {"helmet", "helmet casque"},
        {"battery", "battery pile batterie"},
        {"ice", "ice cristal glacier glace crystal"},
        {"mushroom", "mushroom champignon"},
        {"pyramid", "pyramide pyramid"},
        {"donut", "donut donut beignet ring"},
        {"satellite", "satellite sputnik"},
        {"jewelry", "jewelry bijou bague ring necklace"},
    };
    for (auto& s : scenes) {
        std::string kw = s.keywords;
        std::istringstream iss(kw);
        std::string w;
        while (iss >> w) { if (contains_word(lp, w)) { p.scene_type = s.name; break; } }
        if (!p.scene_type.empty()) break;
    }

    struct { const char* name; const char* keywords; } shapes[] = {
        {"cube", "cube"}, {"sphere", "sphere boule"},
        {"cylinder", "cylinder cylindre"}, {"cone", "cone"},
        {"torus", "torus tore"}, {"plane", "plane plan"},
        {"torus_knot", "torus_knot noeud"}, {"ico_sphere", "ico_sphere ico"},
        {"arrow", "arrow fleche"}
    };
    for (auto& sh : shapes) {
        std::string kw = sh.keywords;
        std::istringstream iss(kw);
        std::string w;
        while (iss >> w) { if (contains_word(lp, w)) { p.shapes.push_back(sh.name); } }
    }

    struct { const char* keywords; float val; } sizes[] = {
        {"tiny minuscule petit tiny", 0.25f}, {"small petit small", 0.5f},
        {"medium moyen", 1.0f}, {"large grand big gros", 1.5f},
        {"huge enorme", 3.0f}, {"massive massif immense", 5.0f}
    };
    for (auto& s : sizes) {
        std::string kw = s.keywords;
        std::istringstream iss(kw);
        std::string w;
        while (iss >> w) { if (contains_word(lp, w)) { p.size = s.val; break; } }
    }

    struct { const char* keywords; vec4 val; } colors[] = {
        {"rouge red", vec4(0.8f,0.1f,0.1f,1)}, {"vert green", vec4(0.1f,0.7f,0.1f,1)},
        {"bleu blue", vec4(0.1f,0.3f,0.8f,1)}, {"jaune yellow", vec4(0.9f,0.9f,0.1f,1)},
        {"orange orange", vec4(1.0f,0.5f,0.0f,1)}, {"violet purple", vec4(0.6f,0.1f,0.8f,1)},
        {"rose pink", vec4(0.9f,0.4f,0.6f,1)}, {"noir black", vec4(0.1f,0.1f,0.1f,1)},
        {"blanc white", vec4(0.95f,0.95f,0.95f,1)}, {"gris gray grey", vec4(0.5f,0.5f,0.5f,1)},
        {"or gold", vec4(1.0f,0.84f,0.0f,1)}, {"argent silver", vec4(0.8f,0.85f,0.9f,1)},
        {"bronze bronze", vec4(0.8f,0.5f,0.2f,1)},
        {"metal metal metal", vec4(0.7f,0.7f,0.75f,1)},
        {"bois wood", vec4(0.55f,0.35f,0.15f,1)},
        {"pierre stone roche", vec4(0.6f,0.55f,0.5f,1)},
        {"eau water", vec4(0.2f,0.5f,0.9f,0.7f)},
        {"feu fire", vec4(0.9f,0.3f,0.0f,1)},
        {"glace ice", vec4(0.6f,0.85f,1.0f,0.6f)},
        {"neon neon", vec4(0.0f,1.0f,0.5f,1.0f)},
    };
    for (auto& c : colors) {
        std::string kw = c.keywords;
        std::istringstream iss(kw);
        std::string w;
        while (iss >> w) { if (contains_word(lp, w)) { p.color = c.val; break; } }
        if (p.color.x >= 0) break;
    }

    if (contains_word(lp, "metal") && !contains_word(lp, "metal metal")) p.material_type = "metallic";
    else if (contains_word(lp, "glossy") || contains_word(lp, "brillant")) p.material_type = "glossy";
    else if (contains_word(lp, "matte") || contains_word(lp, "mat")) p.material_type = "matte";
    else if (contains_word(lp, "glass") || contains_word(lp, "verre")) p.material_type = "glass";
    else if (contains_word(lp, "emissive") || contains_word(lp, "lumineux") || contains_word(lp, "glow")) p.material_type = "emissive";
    else if (contains_word(lp, "wireframe") || contains_word(lp, "fil")) p.material_type = "wireframe";

    auto count_pos = lp.find("x ");
    if (count_pos == std::string::npos) count_pos = lp.find(" fois");
    if (count_pos == std::string::npos) count_pos = lp.find(" times");
    if (count_pos != std::string::npos) {
        std::string num_str;
        for (int i = (int)count_pos - 1; i >= 0 && std::isdigit((unsigned char)lp[i]); i--) num_str = lp[i] + num_str;
        if (!num_str.empty()) p.count = std::clamp(std::stoi(num_str), 1, 100);
    }

    if (contains_word(lp, "grille") || contains_word(lp, "grid")) p.arrangement = "grid";
    else if (contains_word(lp, "cercle") || contains_word(lp, "circle")) p.arrangement = "circle";
    else if (contains_word(lp, "ligne") || contains_word(lp, "line")) p.arrangement = "line";
    else if (contains_word(lp, "spirale") || contains_word(lp, "spiral arrangement")) p.arrangement = "spiral";

    if (contains_word(lp, "subdivide") || contains_word(lp, "subdiviser")) p.modifiers.push_back("subdivide");
    if (contains_word(lp, "smooth") || contains_word(lp, "lisse")) p.modifiers.push_back("smooth");
    if (contains_word(lp, "mirror") || contains_word(lp, "miroir")) p.modifiers.push_back("mirror");

    if (p.shapes.empty() && p.scene_type.empty()) p.shapes.push_back("cube");
    return p;
}

std::string PromptEngine::get_description(const ParsedPrompt& p) {
    std::string d = "Generated ";
    if (!p.scene_type.empty()) d += "scene: " + p.scene_type;
    else { d += "object(s): "; for (auto& s : p.shapes) d += s + " "; }
    d += " (size: " + std::to_string(p.size) + ")";
    if (p.count > 1) d += " x" + std::to_string(p.count);
    return d;
}

// ProceduralEngine helpers
Mesh ProceduralEngine::create_tapered_cylinder(float r_top, float r_bot, float height, int seg) {
    Mesh m; m.name = "TaperedCyl";
    float hh = height * 0.5f;
    for (int i = 0; i <= seg; i++) {
        float theta = 2.0f * (float)M_PI * i / seg;
        float ct = cosf(theta), st = sinf(theta);
        vec3 nrm = glm::normalize(vec3(ct * height, (r_bot - r_top), st * height));
        m.add_vertex(vec3(ct * r_top, hh, st * r_top), vec3(0, 1, 0), vec2((float)i / seg, 1));
        m.add_vertex(vec3(ct * r_bot, -hh, st * r_bot), vec3(0, -1, 0), vec2((float)i / seg, 0));
        m.add_vertex(vec3(ct * r_bot, -hh, st * r_bot), nrm, vec2((float)i / seg, 0));
        m.add_vertex(vec3(ct * r_top, hh, st * r_top), nrm, vec2((float)i / seg, 1));
    }
    for (int i = 0; i < seg; i++) {
        int b = i * 4;
        m.add_face({(uint32_t)b, (uint32_t)(b+4), (uint32_t)(b+5), (uint32_t)(b+1)});
        m.add_face({(uint32_t)(b+2), (uint32_t)(b+3), (uint32_t)(b+7), (uint32_t)(b+6)});
    }
    uint32_t tc = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0, hh, 0), vec3(0, 1, 0));
    uint32_t bc = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0, -hh, 0), vec3(0, -1, 0));
    for (int i = 0; i < seg; i++) {
        m.add_face({tc, (uint32_t)(i*4), (uint32_t)(((i+1)%(seg+1))*4)});
        m.add_face({bc, (uint32_t)(((i+1)%(seg+1))*4+1), (uint32_t)(i*4+1)});
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_pyramid_mesh(float base_size, float height, int sides) {
    Mesh m; m.name = "Pyramid";
    float hs = base_size * 0.5f;
    uint32_t tip = m.add_vertex(vec3(0, height, 0), vec3(0, 1, 0));
    uint32_t center = m.add_vertex(vec3(0, 0, 0), vec3(0, -1, 0));
    for (int i = 0; i <= sides; i++) {
        float a = 2.0f * (float)M_PI * i / sides;
        m.add_vertex(vec3(cosf(a) * hs, 0, sinf(a) * hs), vec3(0, -1, 0));
    }
    for (int i = 0; i < sides; i++) {
        m.add_face({tip, (uint32_t)(i + 2), (uint32_t)(i + 3)});
        m.add_face({center, (uint32_t)(i + 4), (uint32_t)(i + 3)});
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_deformed_sphere(float radius, int seg, int rings, float deform) {
    Mesh m; m.name = "DeformedSphere";
    for (int r = 0; r <= rings; r++) {
        float phi = (float)M_PI * r / rings;
        for (int s = 0; s <= seg; s++) {
            float theta = 2.0f * (float)M_PI * s / seg;
            float r2 = radius * (1.0f + deform * sinf(3 * theta) * sinf(2 * phi));
            vec3 pos(r2 * sinf(phi) * cosf(theta), r2 * cosf(phi), r2 * sinf(phi) * sinf(theta));
            m.add_vertex(pos, glm::normalize(pos), vec2((float)s / seg, (float)r / rings));
        }
    }
    for (int r = 0; r < rings; r++) for (int s = 0; s < seg; s++) {
        uint32_t i0 = r * (seg + 1) + s;
        m.add_face({i0, i0 + (uint32_t)(seg + 1), i0 + (uint32_t)(seg + 2), i0 + 1});
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_star_mesh(float size) {
    Mesh m; m.name = "Star";
    float inner = size * 0.2f, outer = size * 0.5f;
    uint32_t top = m.add_vertex(vec3(0, size * 0.05f, 0), vec3(0, 1, 0));
    uint32_t bot = m.add_vertex(vec3(0, -size * 0.05f, 0), vec3(0, -1, 0));
    for (int i = 0; i < 10; i++) {
        float a = (float)M_PI * 0.5f + i * (float)M_PI / 5.0f;
        float r = (i % 2 == 0) ? outer : inner;
        m.add_vertex(vec3(cosf(a) * r, 0, sinf(a) * r), vec3(0, 1, 0));
    }
    for (int i = 0; i < 10; i++) {
        int next = (i + 1) % 10;
        m.add_face({top, (uint32_t)(i + 2), (uint32_t)(next + 2)});
        m.add_face({bot, (uint32_t)(next + 2), (uint32_t)(i + 2)});
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_heart_mesh(float size) {
    Mesh m; m.name = "Heart";
    int segs = 24, rings = 16;
    for (int r = 0; r <= rings; r++) {
        float phi = (float)M_PI * r / rings;
        for (int s = 0; s <= segs; s++) {
            float theta = 2.0f * (float)M_PI * s / segs;
            float x = 16.0f * powf(sinf(phi), 3.0f) * sinf(theta) / 100.0f;
            float y = (13.0f * cosf(phi) - 5.0f * cosf(2 * phi) - 2.0f * cosf(3 * phi) - cosf(4 * phi)) / 100.0f;
            float z = 16.0f * powf(sinf(phi), 3.0f) * cosf(theta) / 100.0f;
            vec3 pos(x * size, y * size, z * size);
            m.add_vertex(pos, glm::normalize(pos + vec3(0.01f)), vec2((float)s / segs, (float)r / rings));
        }
    }
    for (int r = 0; r < rings; r++) for (int s = 0; s < segs; s++) {
        uint32_t i0 = r * (segs + 1) + s;
        m.add_face({i0, i0 + (uint32_t)(segs + 1), i0 + (uint32_t)(segs + 2), i0 + 1});
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_skull_mesh(float size) {
    Mesh m; m.name = "Skull";
    int segs = 20, rings = 16;
    for (int r = 0; r <= rings; r++) {
        float phi = (float)M_PI * r / rings;
        for (int s = 0; s <= segs; s++) {
            float theta = 2.0f * (float)M_PI * s / segs;
            float ct = cosf(theta), st = sinf(theta);
            float cp = cosf(phi), sp = sinf(phi);
            float cr = 1.0f;
            if (cp < -0.2f) cr = 0.6f + 0.4f * (cp + 0.2f) / 0.8f;
            if (cp > 0.3f) {
                float brow = (cp - 0.3f) / 0.7f;
                float jaw = 1.0f - brow * 0.4f;
                cr = jaw;
            }
            float x = size * 0.5f * sp * ct * cr;
            float y = size * 0.55f * cp + size * 0.1f;
            float z = size * 0.45f * sp * st * cr;
            vec3 pos(x, y, z);
            m.add_vertex(pos, glm::normalize(pos + vec3(0, 0.1f, 0)), vec2((float)s / segs, (float)r / rings));
        }
    }
    for (int r = 0; r < rings; r++) for (int s = 0; s < segs; s++) {
        uint32_t i0 = r * (segs + 1) + s;
        m.add_face({i0, i0 + (uint32_t)(segs + 1), i0 + (uint32_t)(segs + 2), i0 + 1});
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_brain_mesh(float size) {
    Mesh m; m.name = "Brain";
    int segs = 24, rings = 16;
    for (int r = 0; r <= rings; r++) {
        float phi = (float)M_PI * r / rings;
        for (int s = 0; s <= segs; s++) {
            float theta = 2.0f * (float)M_PI * s / segs;
            float ct = cosf(theta), st = sinf(theta);
            float cp = cosf(phi), sp = sinf(phi);
            float wave = 1.0f + 0.15f * sinf(8.0f * theta) * sinf(6.0f * phi);
            float x = size * 0.5f * sp * ct * wave;
            float y = size * 0.4f * cp;
            float z = size * 0.45f * sp * st * wave;
            if (fabsf(ct) < 0.1f && sp > 0.3f) {
                x += (ct > 0 ? 0.02f : -0.02f) * size * sp;
            }
            vec3 pos(x, y, z);
            m.add_vertex(pos, glm::normalize(pos + vec3(0, 0.01f, 0)), vec2((float)s / segs, (float)r / rings));
        }
    }
    for (int r = 0; r < rings; r++) for (int s = 0; s < segs; s++) {
        uint32_t i0 = r * (segs + 1) + s;
        m.add_face({i0, i0 + (uint32_t)(segs + 1), i0 + (uint32_t)(segs + 2), i0 + 1});
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_helmet_mesh(float size) {
    Mesh m; m.name = "Helmet";
    Mesh dome = create_sphere(0.5f * size, 24, 12);
    for (auto& v : dome.vertices) {
        if (v.position.y < -0.1f * size) {
            v.position.y = -0.1f * size;
        }
    }
    for (auto& v : dome.vertices) {
        m.add_vertex(v.position, v.normal, v.uv);
    }
    float rim_y = -0.1f * size;
    float rim_r = 0.48f * size;
    for (int i = 0; i < 24; i++) {
        float a0 = (float)i / 24.0f * 2.0f * (float)M_PI;
        float a1 = (float)(i + 1) / 24.0f * 2.0f * (float)M_PI;
        uint32_t idx = (uint32_t)m.vertices.size();
        m.add_vertex(vec3(cosf(a0) * rim_r, rim_y - 0.05f * size, sinf(a0) * rim_r), vec3(0, -1, 0));
        m.add_vertex(vec3(cosf(a1) * rim_r, rim_y - 0.05f * size, sinf(a1) * rim_r), vec3(0, -1, 0));
        m.add_vertex(vec3(cosf(a0) * (rim_r + 0.03f * size), rim_y, sinf(a0) * (rim_r + 0.03f * size)), vec3(0, 0, -1));
        m.add_vertex(vec3(cosf(a1) * (rim_r + 0.03f * size), rim_y, sinf(a1) * (rim_r + 0.03f * size)), vec3(0, 0, -1));
        m.add_face({idx, idx + 1, idx + 3, idx + 2});
    }
    Mesh visor = create_cube(0.08f * size);
    for (auto& v : visor.vertices) {
        v.position = vec3(v.position.x * 4.0f, v.position.y * 1.5f, v.position.z * 2.0f);
        v.position += vec3(0, 0.05f * size, 0.42f * size);
    }
    uint32_t off = (uint32_t)m.vertices.size();
    for (auto& v : visor.vertices) m.add_vertex(v.position, v.normal, v.uv);
    for (auto& f : visor.faces) {
        std::vector<uint32_t> fi;
        for (auto& vi : f.vertices) fi.push_back(vi + off);
        m.add_face(fi);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_l_shape(float leg1, float leg2, float thickness) {
    Mesh m; m.name = "LShape";
    Mesh a = create_cube(1.0f);
    for (auto& v : a.vertices) {
        v.position = vec3(v.position.x * thickness, v.position.y * leg1 + leg1 * 0.5f - thickness * 0.5f, v.position.z * thickness);
    }
    for (auto& v : a.vertices) m.add_vertex(v.position, v.normal, v.uv);
    for (auto& f : a.faces) {
        std::vector<uint32_t> fi;
        for (auto& vi : f.vertices) fi.push_back(vi);
        m.add_face(fi);
    }
    Mesh b = create_cube(1.0f);
    for (auto& v : b.vertices) {
        v.position = vec3(v.position.x * leg2 + leg2 * 0.5f - thickness * 0.5f, v.position.y * thickness - thickness * 0.5f + leg1 * 0.0f, v.position.z * thickness);
    }
    uint32_t off = (uint32_t)m.vertices.size();
    for (auto& v : b.vertices) m.add_vertex(v.position, v.normal, v.uv);
    for (auto& f : b.faces) {
        std::vector<uint32_t> fi;
        for (auto& vi : f.vertices) fi.push_back(vi + off);
        m.add_face(fi);
    }
    m.weld_vertices(0.001f);
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_ring(float outer_r, float inner_r, int segs) {
    Mesh m; m.name = "Ring";
    for (int i = 0; i <= segs; i++) {
        float a = 2.0f * (float)M_PI * i / segs;
        float ca = cosf(a), sa = sinf(a);
        m.add_vertex(vec3(ca * outer_r, 0, sa * outer_r), vec3(0, 1, 0), vec2((float)i / segs, 1));
        m.add_vertex(vec3(ca * inner_r, 0, sa * inner_r), vec3(0, 1, 0), vec2((float)i / segs, 0));
    }
    for (int i = 0; i < segs; i++) {
        uint32_t a = i * 2, b = i * 2 + 1, c = (i + 1) * 2, d = (i + 1) * 2 + 1;
        m.add_face({a, c, d, b});
    }
    m.compute_normals();
    return m;
}

// Scene generators
std::vector<ProceduralEngine::Part> ProceduralEngine::generate_tree(float height) {
    std::vector<Part> parts;
    Part trunk; trunk.name = "Trunk";
    trunk.mesh = create_tapered_cylinder(0.12f, 0.18f, height * 0.6f);
    trunk.material.diffuse = vec4(0.4f, 0.25f, 0.1f, 1); trunk.material.name = "Bark";
    trunk.position = vec3(0, height * 0.3f, 0);
    parts.push_back(trunk);
    for (int i = 0; i < 5; i++) {
        Part b; b.name = "Branch_" + std::to_string(i);
        float a = (float)M_PI * 2 * i / 5 + 0.3f;
        float h = height * (0.35f + 0.15f * (i % 3));
        b.mesh = create_tapered_cylinder(0.03f, 0.06f, height * 0.25f);
        b.material.diffuse = vec4(0.45f, 0.3f, 0.12f, 1); b.material.name = "Bark";
        b.position = vec3(cosf(a) * 0.2f, h, sinf(a) * 0.2f);
        b.rotation = vec3(0, 0, rad2deg(0.5f * cosf(a)));
        parts.push_back(b);
    }
    for (int i = 0; i < 3; i++) {
        Part l; l.name = "Leaves_" + std::to_string(i);
        l.mesh = create_deformed_sphere(height * (0.25f - i * 0.03f), 20, 12, 0.3f);
        l.material.diffuse = vec4(0.1f + 0.05f * i, 0.55f + 0.1f * i, 0.1f, 1); l.material.name = "Leaves";
        l.position = vec3(0, height * (0.55f + 0.12f * i), 0);
        parts.push_back(l);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_house(float size) {
    std::vector<Part> parts;
    Part body; body.name = "Body";
    body.mesh = create_cube(1.0f);
    body.material.diffuse = vec4(0.85f, 0.82f, 0.75f, 1); body.material.name = "Wall";
    body.scale = vec3(1.2f * size, size, 0.8f * size);
    parts.push_back(body);
    Part roof; roof.name = "Roof";
    roof.mesh = create_pyramid_mesh(1.4f * size, 0.6f * size);
    roof.material.diffuse = vec4(0.6f, 0.15f, 0.1f, 1); roof.material.name = "Roof";
    roof.position = vec3(0, size * 0.8f, 0);
    parts.push_back(roof);
    Part door; door.name = "Door";
    door.mesh = create_cube(0.1f);
    door.material.diffuse = vec4(0.35f, 0.2f, 0.08f, 1); door.material.name = "Wood";
    door.scale = vec3(0.2f * size, 0.35f * size, 0.05f * size);
    door.position = vec3(0, -size * 0.3f, 0.41f * size);
    parts.push_back(door);
    for (int i = -1; i <= 1; i += 2) {
        Part w; w.name = "Window";
        w.mesh = create_cube(0.1f);
        w.material.diffuse = vec4(0.4f, 0.6f, 0.9f, 0.8f); w.material.opacity = 0.8f; w.material.name = "Glass";
        w.scale = vec3(0.15f * size, 0.15f * size, 0.05f * size);
        w.position = vec3(i * 0.35f * size, 0.1f * size, 0.41f * size);
        parts.push_back(w);
    }
    Part chimney; chimney.name = "Chimney";
    chimney.mesh = create_tapered_cylinder(0.06f, 0.08f, 0.35f * size);
    chimney.material.diffuse = vec4(0.5f, 0.12f, 0.08f, 1); chimney.material.name = "Chimney";
    chimney.position = vec3(0.35f * size, 1.05f * size, 0);
    parts.push_back(chimney);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_car(float size) {
    std::vector<Part> parts;
    Part chassis; chassis.name = "Chassis";
    chassis.mesh = create_cube(1.0f);
    chassis.material.diffuse = vec4(0.8f, 0.1f, 0.1f, 1); chassis.material.metallic = 0.7f; chassis.material.name = "Metal";
    chassis.scale = vec3(size, 0.3f * size, 0.5f * size);
    chassis.position = vec3(0, 0.15f * size, 0);
    parts.push_back(chassis);
    Part cabin; cabin.name = "Cabin";
    cabin.mesh = create_cube(1.0f);
    cabin.material.diffuse = vec4(0.7f, 0.08f, 0.08f, 1); cabin.material.metallic = 0.7f; cabin.material.name = "Metal";
    cabin.scale = vec3(0.5f * size, 0.3f * size, 0.45f * size);
    cabin.position = vec3(-0.1f * size, 0.45f * size, 0);
    parts.push_back(cabin);
    for (int i = 0; i < 4; i++) {
        Part wh; wh.name = "Wheel_" + std::to_string(i);
        wh.mesh = create_cylinder(0.1f * size, 0.08f * size, 16);
        wh.material.diffuse = vec4(0.15f, 0.15f, 0.15f, 1); wh.material.name = "Rubber";
        float x = (i < 2 ? -0.3f : 0.3f) * size;
        float z = (i % 2 == 0 ? -0.25f : 0.25f) * size;
        wh.position = vec3(x, 0, z);
        wh.rotation = vec3(90, 0, 0);
        parts.push_back(wh);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_robot(float size) {
    std::vector<Part> parts;
    Part body; body.name = "Body";
    body.mesh = create_cube(0.8f * size);
    body.material.diffuse = vec4(0.6f, 0.65f, 0.7f, 1); body.material.metallic = 0.8f; body.material.name = "Metal";
    parts.push_back(body);
    Part head; head.name = "Head";
    head.mesh = create_cube(0.5f * size);
    head.material.diffuse = vec4(0.65f, 0.68f, 0.72f, 1); head.material.metallic = 0.8f; head.material.name = "Metal";
    head.position = vec3(0, 0.65f * size, 0);
    parts.push_back(head);
    for (int i = -1; i <= 1; i += 2) {
        Part eye; eye.name = "Eye";
        eye.mesh = create_sphere(0.06f * size);
        eye.material.diffuse = vec4(0, 0.8f, 1.0f, 1); eye.material.emission = vec4(0, 0.4f, 0.5f, 1); eye.material.name = "Glow";
        eye.position = vec3(i * 0.12f * size, 0.7f * size, 0.25f * size);
        parts.push_back(eye);
        Part arm; arm.name = "Arm";
        arm.mesh = create_cube(0.15f * size);
        arm.material.diffuse = vec4(0.55f, 0.6f, 0.65f, 1); arm.material.metallic = 0.8f; arm.material.name = "Metal";
        arm.position = vec3(i * 0.55f * size, -0.05f * size, 0);
        parts.push_back(arm);
        Part leg; leg.name = "Leg";
        leg.mesh = create_cube(0.2f * size);
        leg.material.diffuse = vec4(0.5f, 0.55f, 0.6f, 1); leg.material.metallic = 0.8f; leg.material.name = "Metal";
        leg.position = vec3(i * 0.2f * size, -0.6f * size, 0);
        parts.push_back(leg);
    }
    Part antenna; antenna.name = "Antenna";
    antenna.mesh = create_tapered_cylinder(0.015f * size, 0.02f * size, 0.25f * size);
    antenna.material.diffuse = vec4(0.5f, 0.55f, 0.6f, 1); antenna.material.name = "Metal";
    antenna.position = vec3(0, 1.05f * size, 0);
    parts.push_back(antenna);
    Part glow; glow.name = "GlowBall";
    glow.mesh = create_sphere(0.04f * size);
    glow.material.diffuse = vec4(0, 1, 0.5f, 1); glow.material.emission = vec4(0, 0.8f, 0.4f, 1); glow.material.name = "Glow";
    glow.position = vec3(0, 1.2f * size, 0);
    parts.push_back(glow);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_castle(float size) {
    std::vector<Part> parts;
    Part base; base.name = "Base";
    base.mesh = create_cube(1.0f);
    base.material.diffuse = vec4(0.6f, 0.55f, 0.5f, 1); base.material.name = "Stone";
    base.scale = vec3(2.0f * size, 0.8f * size, 1.5f * size);
    parts.push_back(base);
    vec2 tower_pos[] = {{-0.9f, -0.65f}, {0.9f, -0.65f}, {-0.9f, 0.65f}, {0.9f, 0.65f}};
    for (int i = 0; i < 4; i++) {
        Part t; t.name = "Tower_" + std::to_string(i);
        t.mesh = create_tapered_cylinder(0.2f * size, 0.25f * size, 1.2f * size);
        t.material.diffuse = vec4(0.55f, 0.5f, 0.45f, 1); t.material.name = "Stone";
        t.position = vec3(tower_pos[i].x * size, 0.4f * size, tower_pos[i].y * size);
        parts.push_back(t);
        Part cap; cap.name = "TowerCap_" + std::to_string(i);
        cap.mesh = create_cone(0.22f * size, 0.4f * size);
        cap.material.diffuse = vec4(0.35f, 0.12f, 0.1f, 1); cap.material.name = "Roof";
        cap.position = vec3(tower_pos[i].x * size, 1.2f * size, tower_pos[i].y * size);
        parts.push_back(cap);
    }
    Part gate; gate.name = "Gate";
    gate.mesh = create_cube(1.0f);
    gate.material.diffuse = vec4(0.25f, 0.15f, 0.08f, 1); gate.material.name = "Wood";
    gate.scale = vec3(0.3f * size, 0.5f * size, 0.1f * size);
    gate.position = vec3(0, -0.15f * size, 0.76f * size);
    parts.push_back(gate);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_spaceship(float size) {
    std::vector<Part> parts;
    Part body; body.name = "Body";
    body.mesh = create_cylinder(0.3f * size, 1.2f * size, 24);
    body.material.diffuse = vec4(0.7f, 0.72f, 0.75f, 1); body.material.metallic = 0.9f; body.material.name = "Metal";
    body.rotation = vec3(0, 0, 90);
    parts.push_back(body);
    Part nose; nose.name = "Nose";
    nose.mesh = create_cone(0.3f * size, 0.6f * size);
    nose.material.diffuse = vec4(0.65f, 0.67f, 0.7f, 1); nose.material.metallic = 0.9f; nose.material.name = "Metal";
    nose.position = vec3(0.9f * size, 0, 0);
    nose.rotation = vec3(0, 0, 90);
    parts.push_back(nose);
    for (int i = 0; i < 3; i++) {
        Part w; w.name = "Wing_" + std::to_string(i);
        w.mesh = create_cube(0.1f);
        w.material.diffuse = vec4(0.6f, 0.62f, 0.65f, 1); w.material.metallic = 0.8f; w.material.name = "Metal";
        float a = (float)i * 2.0f * (float)M_PI / 3.0f;
        w.scale = vec3(0.5f * size, 0.02f * size, 0.3f * size);
        w.position = vec3(-0.2f * size, cosf(a) * 0.35f * size, sinf(a) * 0.35f * size);
        w.rotation = vec3(rad2deg(a), 0, 0);
        parts.push_back(w);
    }
    Part cockpit; cockpit.name = "Cockpit";
    cockpit.mesh = create_sphere(0.15f * size);
    cockpit.material.diffuse = vec4(0.3f, 0.5f, 0.8f, 0.6f); cockpit.material.opacity = 0.6f; cockpit.material.shininess = 150; cockpit.material.name = "Glass";
    cockpit.position = vec3(0.65f * size, 0.15f * size, 0);
    parts.push_back(cockpit);
    for (int i = 0; i < 3; i++) {
        Part e; e.name = "Engine_" + std::to_string(i);
        e.mesh = create_cylinder(0.08f * size, 0.2f * size, 12);
        e.material.diffuse = vec4(0.2f, 0.5f, 1.0f, 1); e.material.emission = vec4(0.1f, 0.25f, 0.5f, 1); e.material.name = "Glow";
        float a = (float)i * 2.0f * (float)M_PI / 3.0f;
        e.position = vec3(-0.65f * size, cosf(a) * 0.2f * size, sinf(a) * 0.2f * size);
        e.rotation = vec3(0, 0, 90);
        parts.push_back(e);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_sword(float size) {
    std::vector<Part> parts;
    Part blade; blade.name = "Blade";
    blade.mesh = create_cube(1.0f);
    blade.material.diffuse = vec4(0.8f, 0.85f, 0.9f, 1); blade.material.metallic = 0.95f; blade.material.roughness = 0.1f; blade.material.name = "Steel";
    blade.scale = vec3(0.08f * size, 0.8f * size, 0.02f * size);
    blade.position = vec3(0, 0.5f * size, 0);
    parts.push_back(blade);
    Part guard; guard.name = "Guard";
    guard.mesh = create_cube(1.0f);
    guard.material.diffuse = vec4(0.8f, 0.7f, 0.0f, 1); guard.material.metallic = 0.9f; guard.material.name = "Gold";
    guard.scale = vec3(0.3f * size, 0.04f * size, 0.06f * size);
    guard.position = vec3(0, 0.08f * size, 0);
    parts.push_back(guard);
    Part handle; handle.name = "Handle";
    handle.mesh = create_cylinder(0.025f * size, 0.2f * size, 8);
    handle.material.diffuse = vec4(0.35f, 0.2f, 0.1f, 1); handle.material.name = "Wood";
    handle.position = vec3(0, -0.05f * size, 0);
    parts.push_back(handle);
    Part pommel; pommel.name = "Pommel";
    pommel.mesh = create_sphere(0.04f * size);
    pommel.material.diffuse = vec4(0.8f, 0.7f, 0.0f, 1); pommel.material.metallic = 0.9f; pommel.material.name = "Gold";
    pommel.position = vec3(0, -0.18f * size, 0);
    parts.push_back(pommel);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_chair(float size) {
    std::vector<Part> parts;
    Part seat; seat.name = "Seat";
    seat.mesh = create_cube(1.0f);
    seat.material.diffuse = vec4(0.55f, 0.35f, 0.15f, 1); seat.material.name = "Wood";
    seat.scale = vec3(0.45f * size, 0.04f * size, 0.45f * size);
    seat.position = vec3(0, 0.2f * size, 0);
    parts.push_back(seat);
    Part back; back.name = "Back";
    back.mesh = create_cube(1.0f);
    back.material.diffuse = vec4(0.5f, 0.32f, 0.12f, 1); back.material.name = "Wood";
    back.scale = vec3(0.45f * size, 0.4f * size, 0.04f * size);
    back.position = vec3(0, 0.42f * size, -0.2f * size);
    parts.push_back(back);
    vec2 legs[] = {{-0.18f, -0.18f}, {0.18f, -0.18f}, {-0.18f, 0.18f}, {0.18f, 0.18f}};
    for (int i = 0; i < 4; i++) {
        Part l; l.name = "Leg_" + std::to_string(i);
        l.mesh = create_cube(1.0f);
        l.material.diffuse = vec4(0.48f, 0.3f, 0.1f, 1); l.material.name = "Wood";
        l.scale = vec3(0.04f * size, 0.2f * size, 0.04f * size);
        l.position = vec3(legs[i].x * size, 0.1f * size, legs[i].y * size);
        parts.push_back(l);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_table(float size) {
    std::vector<Part> parts;
    Part top; top.name = "TableTop";
    top.mesh = create_cube(1.0f);
    top.material.diffuse = vec4(0.55f, 0.35f, 0.15f, 1); top.material.name = "Wood";
    top.scale = vec3(0.8f * size, 0.05f * size, 0.5f * size);
    top.position = vec3(0, 0.4f * size, 0);
    parts.push_back(top);
    vec2 legs[] = {{-0.35f, -0.2f}, {0.35f, -0.2f}, {-0.35f, 0.2f}, {0.35f, 0.2f}};
    for (int i = 0; i < 4; i++) {
        Part l; l.name = "Leg_" + std::to_string(i);
        l.mesh = create_cube(1.0f);
        l.material.diffuse = vec4(0.48f, 0.3f, 0.1f, 1); l.material.name = "Wood";
        l.scale = vec3(0.05f * size, 0.4f * size, 0.05f * size);
        l.position = vec3(legs[i].x * size, 0.2f * size, legs[i].y * size);
        parts.push_back(l);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_star(float size) {
    Part p; p.name = "Star"; p.mesh = create_star_mesh(size);
    p.material.diffuse = vec4(1, 0.9f, 0, 1); p.material.name = "Gold"; p.material.shininess = 64;
    return {p};
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_heart(float size) {
    Part p; p.name = "Heart"; p.mesh = create_heart_mesh(size);
    p.material.diffuse = vec4(0.9f, 0.1f, 0.2f, 1); p.material.name = "Red";
    return {p};
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_mountain(float size) {
    std::vector<Part> parts;
    Part m; m.name = "Mountain";
    m.mesh = create_cone(1.0f * size, 1.5f * size, 24);
    m.material.diffuse = vec4(0.4f, 0.38f, 0.35f, 1); m.material.name = "Rock";
    parts.push_back(m);
    Part snow; snow.name = "Snow";
    snow.mesh = create_cone(0.3f * size, 0.5f * size, 16);
    snow.material.diffuse = vec4(0.95f, 0.95f, 1.0f, 1); snow.material.name = "Snow";
    snow.position = vec3(0, 0.5f * size, 0);
    parts.push_back(snow);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_snowman(float size) {
    std::vector<Part> parts;
    float radii[] = {0.3f, 0.22f, 0.15f};
    float heights[] = {0.3f, 0.72f, 1.08f};
    for (int i = 0; i < 3; i++) {
        Part b; b.name = "Ball_" + std::to_string(i);
        b.mesh = create_sphere(radii[i] * size);
        b.material.diffuse = vec4(0.95f, 0.95f, 0.95f, 1); b.material.name = "Snow";
        b.position = vec3(0, heights[i] * size, 0);
        parts.push_back(b);
    }
    Part nose; nose.name = "Nose";
    nose.mesh = create_cone(0.03f * size, 0.15f * size, 8);
    nose.material.diffuse = vec4(1.0f, 0.5f, 0.0f, 1); nose.material.name = "Carrot";
    nose.position = vec3(0, 1.1f * size, 0.15f * size);
    nose.rotation = vec3(0, 0, -90);
    parts.push_back(nose);
    for (int i = 0; i < 3; i++) {
        float angle = (float)(i - 1) * 20.0f;
        Part eye; eye.name = "Eye_" + std::to_string(i);
        eye.mesh = create_sphere(0.015f * size);
        eye.material.diffuse = vec4(0.05f, 0.05f, 0.05f, 1); eye.material.name = "Black";
        float rad = deg2rad(angle);
        eye.position = vec3(sin(rad) * 0.1f * size, 1.13f * size, cos(rad) * 0.13f * size);
        parts.push_back(eye);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_diamond(float size) {
    std::vector<Part> parts;
    Part gem; gem.name = "Gem";
    gem.mesh = create_ico_sphere(0.3f * size, 2);
    gem.material.diffuse = vec4(0.2f, 0.6f, 1.0f, 0.9f); gem.material.opacity = 0.9f; gem.material.shininess = 150; gem.material.name = "Diamond";
    gem.scale = vec3(1, 0.7f, 1);
    parts.push_back(gem);
    Part base; base.name = "Base";
    base.mesh = create_ico_sphere(0.2f * size, 1);
    base.material.diffuse = vec4(0.15f, 0.45f, 0.8f, 0.8f); base.material.opacity = 0.8f; base.material.shininess = 150; base.material.name = "Diamond";
    base.position = vec3(0, -0.2f * size, 0);
    parts.push_back(base);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_spiral(float size) {
    std::vector<Part> parts;
    for (int i = 0; i < 60; i++) {
        float t = (float)i / 60.0f;
        float angle = t * 6.0f * (float)M_PI;
        float r = 0.3f * size;
        float s = (0.05f + t * 0.12f) * size;
        Part p; p.name = "Ball_" + std::to_string(i);
        p.mesh = create_sphere(s);
        float cr = fabsf(sinf(t * 2.0f * (float)M_PI));
        float cg = fabsf(sinf(t * 2.0f * (float)M_PI + 2.0f));
        float cb = fabsf(sinf(t * 2.0f * (float)M_PI + 4.0f));
        p.material.diffuse = vec4(cr, cg, cb, 1); p.material.emission = vec4(cr * 0.3f, cg * 0.3f, cb * 0.3f, 1); p.material.name = "Rainbow";
        p.position = vec3(cosf(angle) * r, t * 2.0f * size, sinf(angle) * r);
        parts.push_back(p);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_dna(float size) {
    std::vector<Part> parts;
    for (int i = 0; i < 60; i++) {
        float t = (float)i / 60.0f;
        float angle = t * 6.0f * (float)M_PI;
        float r = 0.3f * size;
        float y = t * 3.0f * size;
        for (int strand = 0; strand < 2; strand++) {
            float a = angle + strand * (float)M_PI;
            Part p; p.name = "DNA_Ball";
            p.mesh = create_sphere(0.04f * size);
            p.material.diffuse = strand == 0 ? vec4(0.2f, 0.4f, 0.9f, 1) : vec4(0.9f, 0.2f, 0.2f, 1);
            p.material.name = strand == 0 ? "Blue" : "Red";
            p.position = vec3(cosf(a) * r, y, sinf(a) * r);
            parts.push_back(p);
        }
        if (i % 5 == 0) {
            Part link; link.name = "DNA_Link";
            link.mesh = create_cylinder(0.01f * size, 0.5f * size, 6);
            link.material.diffuse = vec4(0.7f, 0.7f, 0.7f, 1); link.material.name = "Gray";
            link.position = vec3(cosf(angle) * r * 0.5f, y, sinf(angle) * r * 0.5f);
            link.rotation = vec3(0, rad2deg(angle), 90);
            parts.push_back(link);
        }
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_skull(float size) {
    std::vector<Part> parts;
    Part cranium; cranium.name = "Cranium";
    cranium.mesh = create_skull_mesh(size);
    cranium.material.diffuse = vec4(0.9f, 0.88f, 0.82f, 1); cranium.material.name = "Bone";
    cranium.position = vec3(0, 0.2f * size, 0);
    parts.push_back(cranium);
    for (int i = -1; i <= 1; i += 2) {
        Part eye; eye.name = "EyeSocket";
        eye.mesh = create_sphere(0.06f * size);
        eye.material.diffuse = vec4(0.05f, 0.05f, 0.05f, 1); eye.material.name = "Dark";
        eye.position = vec3(i * 0.1f * size, 0.25f * size, 0.35f * size);
        parts.push_back(eye);
    }
    Part nose; nose.name = "NoseHole";
    nose.mesh = create_cone(0.03f * size, 0.06f * size, 6);
    nose.material.diffuse = vec4(0.05f, 0.05f, 0.05f, 1); nose.material.name = "Dark";
    nose.position = vec3(0, 0.18f * size, 0.38f * size);
    nose.rotation = vec3(0, 0, 180);
    parts.push_back(nose);
    Part jaw; jaw.name = "Jaw";
    jaw.mesh = create_cube(0.35f * size);
    jaw.material.diffuse = vec4(0.85f, 0.83f, 0.77f, 1); jaw.material.name = "Bone";
    jaw.scale = vec3(1.0f, 0.3f, 0.6f);
    jaw.position = vec3(0, -0.02f * size, 0.05f * size);
    parts.push_back(jaw);
    for (int i = 0; i < 6; i++) {
        Part tooth; tooth.name = "Tooth_" + std::to_string(i);
        tooth.mesh = create_cube(0.015f * size);
        tooth.material.diffuse = vec4(0.95f, 0.95f, 0.9f, 1); tooth.material.name = "Teeth";
        float x = ((float)i - 2.5f) * 0.04f * size;
        tooth.position = vec3(x, 0.08f * size, 0.2f * size);
        parts.push_back(tooth);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_airplane(float size) {
    std::vector<Part> parts;
    Part body; body.name = "Fuselage";
    body.mesh = create_cylinder(0.15f * size, 1.5f * size, 16);
    body.material.diffuse = vec4(0.85f, 0.85f, 0.9f, 1); body.material.metallic = 0.6f; body.material.name = "Metal";
    body.rotation = vec3(0, 0, 90);
    parts.push_back(body);
    Part nose; nose.name = "Nose";
    nose.mesh = create_cone(0.15f * size, 0.3f * size, 12);
    nose.material.diffuse = vec4(0.8f, 0.1f, 0.1f, 1); nose.material.name = "Red";
    nose.position = vec3(0.9f * size, 0, 0);
    nose.rotation = vec3(0, 0, 90);
    parts.push_back(nose);
    Part wing; wing.name = "Wings";
    wing.mesh = create_cube(0.1f);
    wing.material.diffuse = vec4(0.8f, 0.8f, 0.85f, 1); wing.material.metallic = 0.5f; wing.material.name = "Metal";
    wing.scale = vec3(0.1f * size, 0.02f * size, 0.8f * size);
    wing.position = vec3(0, 0, 0);
    parts.push_back(wing);
    Part tail_fin; tail_fin.name = "TailFin";
    tail_fin.mesh = create_cube(0.1f);
    tail_fin.material.diffuse = vec4(0.8f, 0.1f, 0.1f, 1); tail_fin.material.name = "Red";
    tail_fin.scale = vec3(0.15f * size, 0.2f * size, 0.02f * size);
    tail_fin.position = vec3(-0.65f * size, 0.1f * size, 0);
    parts.push_back(tail_fin);
    Part prop; prop.name = "Propeller";
    prop.mesh = create_cube(0.1f);
    prop.material.diffuse = vec4(0.3f, 0.2f, 0.1f, 1); prop.material.name = "Wood";
    prop.scale = vec3(0.02f * size, 0.25f * size, 0.03f * size);
    prop.position = vec3(0.78f * size, 0, 0);
    parts.push_back(prop);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_boat(float size) {
    std::vector<Part> parts;
    Part hull; hull.name = "Hull";
    hull.mesh = create_cube(1.0f);
    hull.material.diffuse = vec4(0.55f, 0.35f, 0.15f, 1); hull.material.name = "Wood";
    hull.scale = vec3(1.2f * size, 0.3f * size, 0.4f * size);
    hull.position = vec3(0, -0.1f * size, 0);
    parts.push_back(hull);
    Part deck; deck.name = "Deck";
    deck.mesh = create_cube(1.0f);
    deck.material.diffuse = vec4(0.6f, 0.4f, 0.2f, 1); deck.material.name = "Wood";
    deck.scale = vec3(1.0f * size, 0.04f * size, 0.35f * size);
    deck.position = vec3(0, 0.08f * size, 0);
    parts.push_back(deck);
    Part mast; mast.name = "Mast";
    mast.mesh = create_tapered_cylinder(0.02f * size, 0.03f * size, 0.8f * size);
    mast.material.diffuse = vec4(0.5f, 0.3f, 0.1f, 1); mast.material.name = "Wood";
    mast.position = vec3(0, 0.5f * size, 0);
    parts.push_back(mast);
    Part sail; sail.name = "Sail";
    sail.mesh = create_cube(0.1f);
    sail.material.diffuse = vec4(0.95f, 0.95f, 0.9f, 1); sail.material.name = "Canvas";
    sail.scale = vec3(0.02f * size, 0.5f * size, 0.3f * size);
    sail.position = vec3(0.05f * size, 0.5f * size, 0);
    parts.push_back(sail);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_door(float size) {
    std::vector<Part> parts;
    Part panel; panel.name = "Panel";
    panel.mesh = create_cube(1.0f);
    panel.material.diffuse = vec4(0.45f, 0.25f, 0.1f, 1); panel.material.name = "Wood";
    panel.scale = vec3(0.04f * size, 0.9f * size, 0.45f * size);
    parts.push_back(panel);
    Part knob; knob.name = "Knob";
    knob.mesh = create_sphere(0.03f * size);
    knob.material.diffuse = vec4(0.8f, 0.7f, 0.0f, 1); knob.material.metallic = 0.9f; knob.material.name = "Gold";
    knob.position = vec3(0.03f * size, 0, 0.15f * size);
    parts.push_back(knob);
    Part frame; frame.name = "Frame";
    frame.mesh = create_cube(1.0f);
    frame.material.diffuse = vec4(0.4f, 0.22f, 0.08f, 1); frame.material.name = "Wood";
    frame.scale = vec3(0.06f * size, 0.95f * size, 0.5f * size);
    frame.position = vec3(-0.01f * size, 0, 0);
    parts.push_back(frame);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_window(float size) {
    std::vector<Part> parts;
    Part frame; frame.name = "Frame";
    frame.mesh = create_cube(1.0f);
    frame.material.diffuse = vec4(0.45f, 0.25f, 0.1f, 1); frame.material.name = "Wood";
    frame.scale = vec3(0.04f * size, 0.6f * size, 0.5f * size);
    parts.push_back(frame);
    Part glass; glass.name = "Glass";
    glass.mesh = create_cube(0.08f);
    glass.material.diffuse = vec4(0.5f, 0.7f, 0.9f, 0.4f); glass.material.opacity = 0.4f; glass.material.name = "Glass";
    glass.scale = vec3(0.02f * size, 0.5f * size, 0.4f * size);
    glass.position = vec3(0.01f * size, 0, 0);
    parts.push_back(glass);
    Part cross; cross.name = "CrossBar";
    cross.mesh = create_cube(0.1f);
    cross.material.diffuse = vec4(0.42f, 0.24f, 0.09f, 1); cross.material.name = "Wood";
    cross.scale = vec3(0.05f * size, 0.03f * size, 0.5f * size);
    parts.push_back(cross);
    Part cross2; cross2.name = "CrossBar2";
    cross2.mesh = create_cube(0.1f);
    cross2.material.diffuse = vec4(0.42f, 0.24f, 0.09f, 1); cross2.material.name = "Wood";
    cross2.scale = vec3(0.05f * size, 0.6f * size, 0.03f * size);
    parts.push_back(cross2);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_shelf(float size) {
    std::vector<Part> parts;
    for (int i = 0; i < 3; i++) {
        Part plank; plank.name = "Plank_" + std::to_string(i);
        plank.mesh = create_cube(1.0f);
        plank.material.diffuse = vec4(0.55f, 0.35f, 0.15f, 1); plank.material.name = "Wood";
        plank.scale = vec3(0.6f * size, 0.03f * size, 0.2f * size);
        plank.position = vec3(0, (i - 1) * 0.3f * size, 0);
        parts.push_back(plank);
    }
    Part side_l; side_l.name = "SideL";
    side_l.mesh = create_cube(1.0f);
    side_l.material.diffuse = vec4(0.5f, 0.32f, 0.12f, 1); side_l.material.name = "Wood";
    side_l.scale = vec3(0.03f * size, 0.7f * size, 0.2f * size);
    side_l.position = vec3(-0.3f * size, 0, 0);
    parts.push_back(side_l);
    Part side_r; side_r.name = "SideR";
    side_r.mesh = create_cube(1.0f);
    side_r.material.diffuse = vec4(0.5f, 0.32f, 0.12f, 1); side_r.material.name = "Wood";
    side_r.scale = vec3(0.03f * size, 0.7f * size, 0.2f * size);
    side_r.position = vec3(0.3f * size, 0, 0);
    parts.push_back(side_r);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_lamp(float size) {
    std::vector<Part> parts;
    Part base; base.name = "Base";
    base.mesh = create_cylinder(0.15f * size, 0.03f * size, 16);
    base.material.diffuse = vec4(0.3f, 0.3f, 0.3f, 1); base.material.metallic = 0.8f; base.material.name = "Metal";
    base.position = vec3(0, -0.4f * size, 0);
    parts.push_back(base);
    Part pole; pole.name = "Pole";
    pole.mesh = create_tapered_cylinder(0.02f * size, 0.025f * size, 0.7f * size);
    pole.material.diffuse = vec4(0.35f, 0.35f, 0.38f, 1); pole.material.metallic = 0.7f; pole.material.name = "Metal";
    pole.position = vec3(0, 0, 0);
    parts.push_back(pole);
    Part shade; shade.name = "Shade";
    shade.mesh = create_cone(0.2f * size, 0.2f * size, 16);
    shade.material.diffuse = vec4(0.9f, 0.85f, 0.7f, 0.8f); shade.material.opacity = 0.8f; shade.material.name = "Fabric";
    shade.position = vec3(0, 0.35f * size, 0);
    shade.rotation = vec3(180, 0, 0);
    parts.push_back(shade);
    Part bulb; bulb.name = "Bulb";
    bulb.mesh = create_sphere(0.04f * size);
    bulb.material.diffuse = vec4(1.0f, 0.95f, 0.8f, 1); bulb.material.emission = vec4(0.8f, 0.75f, 0.5f, 1); bulb.material.name = "Light";
    bulb.position = vec3(0, 0.3f * size, 0);
    parts.push_back(bulb);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_brain(float size) {
    std::vector<Part> parts;
    Part brain; brain.name = "Brain";
    brain.mesh = create_brain_mesh(size);
    brain.material.diffuse = vec4(0.85f, 0.55f, 0.55f, 1); brain.material.name = "Brain";
    parts.push_back(brain);
    Part stem; stem.name = "BrainStem";
    stem.mesh = create_tapered_cylinder(0.04f * size, 0.06f * size, 0.2f * size);
    stem.material.diffuse = vec4(0.8f, 0.5f, 0.5f, 1); stem.material.name = "Brain";
    stem.position = vec3(0, -0.3f * size, -0.15f * size);
    stem.rotation = vec3(30, 0, 0);
    parts.push_back(stem);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_flower(float size) {
    std::vector<Part> parts;
    Part stem; stem.name = "Stem";
    stem.mesh = create_tapered_cylinder(0.015f * size, 0.02f * size, 0.6f * size);
    stem.material.diffuse = vec4(0.15f, 0.5f, 0.1f, 1); stem.material.name = "Green";
    stem.position = vec3(0, 0.1f * size, 0);
    parts.push_back(stem);
    for (int i = 0; i < 6; i++) {
        Part petal; petal.name = "Petal_" + std::to_string(i);
        petal.mesh = create_sphere(0.06f * size);
        petal.material.diffuse = vec4(0.9f, 0.2f, 0.4f, 1); petal.material.name = "Pink";
        float a = (float)i * 2.0f * (float)M_PI / 6.0f;
        petal.scale = vec3(1, 0.4f, 0.8f);
        petal.position = vec3(cosf(a) * 0.08f * size, 0.42f * size, sinf(a) * 0.08f * size);
        parts.push_back(petal);
    }
    Part center; center.name = "Center";
    center.mesh = create_sphere(0.04f * size);
    center.material.diffuse = vec4(1.0f, 0.9f, 0.1f, 1); center.material.name = "Yellow";
    center.position = vec3(0, 0.42f * size, 0);
    parts.push_back(center);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_bone(float size) {
    std::vector<Part> parts;
    Part shaft; shaft.name = "Shaft";
    shaft.mesh = create_cylinder(0.03f * size, 0.5f * size, 12);
    shaft.material.diffuse = vec4(0.9f, 0.88f, 0.82f, 1); shaft.material.name = "Bone";
    parts.push_back(shaft);
    for (int i = 0; i < 2; i++) {
        Part end; end.name = "End_" + std::to_string(i);
        end.mesh = create_sphere(0.05f * size);
        end.material.diffuse = vec4(0.9f, 0.88f, 0.82f, 1); end.material.name = "Bone";
        end.position = vec3(0, (i == 0 ? 0.25f : -0.25f) * size, 0);
        parts.push_back(end);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_key(float size) {
    std::vector<Part> parts;
    Part ring; ring.name = "Ring";
    ring.mesh = create_torus(0.08f * size, 0.015f * size, 16, 8);
    ring.material.diffuse = vec4(0.8f, 0.7f, 0.0f, 1); ring.material.metallic = 0.9f; ring.material.name = "Gold";
    ring.position = vec3(0, 0.2f * size, 0);
    parts.push_back(ring);
    Part shaft; shaft.name = "Shaft";
    shaft.mesh = create_cube(0.1f);
    shaft.material.diffuse = vec4(0.8f, 0.7f, 0.0f, 1); shaft.material.metallic = 0.9f; shaft.material.name = "Gold";
    shaft.scale = vec3(0.02f * size, 0.3f * size, 0.01f * size);
    shaft.position = vec3(0, 0, 0);
    parts.push_back(shaft);
    Part tooth; tooth.name = "Tooth";
    tooth.mesh = create_cube(0.1f);
    tooth.material.diffuse = vec4(0.8f, 0.7f, 0.0f, 1); tooth.material.metallic = 0.9f; tooth.material.name = "Gold";
    tooth.scale = vec3(0.04f * size, 0.02f * size, 0.01f * size);
    tooth.position = vec3(0.02f * size, -0.12f * size, 0);
    parts.push_back(tooth);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_helmet(float size) {
    std::vector<Part> parts;
    Part helm; helm.name = "Helmet";
    helm.mesh = create_helmet_mesh(size);
    helm.material.diffuse = vec4(0.4f, 0.45f, 0.5f, 1); helm.material.metallic = 0.85f; helm.material.name = "Metal";
    parts.push_back(helm);
    Part visor; visor.name = "Visor";
    visor.mesh = create_cube(0.08f * size);
    visor.material.diffuse = vec4(0.2f, 0.5f, 0.8f, 0.5f); visor.material.opacity = 0.5f; visor.material.shininess = 150; visor.material.name = "Glass";
    visor.scale = vec3(0.4f * size, 0.15f * size, 0.05f * size);
    visor.position = vec3(0, 0.05f * size, 0.4f * size);
    parts.push_back(visor);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_battery(float size) {
    std::vector<Part> parts;
    Part body; body.name = "Body";
    body.mesh = create_cylinder(0.1f * size, 0.35f * size, 16);
    body.material.diffuse = vec4(0.2f, 0.6f, 0.2f, 1); body.material.name = "Plastic";
    parts.push_back(body);
    Part terminal; terminal.name = "Terminal";
    terminal.mesh = create_cylinder(0.04f * size, 0.03f * size, 8);
    terminal.material.diffuse = vec4(0.7f, 0.7f, 0.7f, 1); terminal.material.metallic = 0.9f; terminal.material.name = "Metal";
    terminal.position = vec3(0, 0.19f * size, 0);
    parts.push_back(terminal);
    Part label; label.name = "Label";
    label.mesh = create_cylinder(0.101f * size, 0.2f * size, 16);
    label.material.diffuse = vec4(0.9f, 0.9f, 0.1f, 1); label.material.name = "Label";
    label.position = vec3(0, -0.02f * size, 0);
    parts.push_back(label);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_ice_crystal(float size) {
    std::vector<Part> parts;
    Part main; main.name = "MainCrystal";
    main.mesh = create_ico_sphere(0.3f * size, 1);
    main.material.diffuse = vec4(0.6f, 0.85f, 1.0f, 0.7f); main.material.opacity = 0.7f; main.material.shininess = 150; main.material.name = "Ice";
    main.scale = vec3(0.6f, 1.5f, 0.6f);
    parts.push_back(main);
    for (int i = 0; i < 4; i++) {
        Part spike; spike.name = "Spike_" + std::to_string(i);
        spike.mesh = create_ico_sphere(0.1f * size, 1);
        spike.material.diffuse = vec4(0.5f, 0.8f, 1.0f, 0.6f); spike.material.opacity = 0.6f; spike.material.name = "Ice";
        spike.scale = vec3(0.4f, 1.2f, 0.4f);
        float a = (float)i * (float)M_PI * 0.5f + 0.3f;
        spike.position = vec3(cosf(a) * 0.15f * size, -0.1f * size, sinf(a) * 0.15f * size);
        spike.rotation = vec3(rad2deg(0.3f * cosf(a)), 0, rad2deg(0.3f * sinf(a)));
        parts.push_back(spike);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_mushroom(float size) {
    std::vector<Part> parts;
    Part stem; stem.name = "Stem";
    stem.mesh = create_tapered_cylinder(0.06f * size, 0.08f * size, 0.3f * size);
    stem.material.diffuse = vec4(0.9f, 0.88f, 0.8f, 1); stem.material.name = "Stem";
    stem.position = vec3(0, -0.05f * size, 0);
    parts.push_back(stem);
    Part cap; cap.name = "Cap";
    cap.mesh = create_sphere(0.2f * size, 16, 8);
    cap.material.diffuse = vec4(0.8f, 0.15f, 0.1f, 1); cap.material.name = "RedCap";
    cap.scale = vec3(1, 0.5f, 1);
    cap.position = vec3(0, 0.12f * size, 0);
    parts.push_back(cap);
    for (int i = 0; i < 5; i++) {
        Part spot; spot.name = "Spot_" + std::to_string(i);
        spot.mesh = create_sphere(0.02f * size);
        spot.material.diffuse = vec4(0.95f, 0.95f, 0.9f, 1); spot.material.name = "White";
        float a = (float)i * 2.0f * (float)M_PI / 5.0f;
        spot.position = vec3(cosf(a) * 0.1f * size, 0.18f * size, sinf(a) * 0.1f * size);
        parts.push_back(spot);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_pyramid(float size) {
    std::vector<Part> parts;
    Part p; p.name = "Pyramid";
    p.mesh = create_pyramid_mesh(1.0f * size, 1.2f * size, 4);
    p.material.diffuse = vec4(0.85f, 0.75f, 0.55f, 1); p.material.name = "Sandstone";
    parts.push_back(p);
    Part door; door.name = "Door";
    door.mesh = create_cube(0.1f);
    door.material.diffuse = vec4(0.2f, 0.15f, 0.08f, 1); door.material.name = "Dark";
    door.scale = vec3(0.1f * size, 0.15f * size, 0.02f * size);
    door.position = vec3(0, -0.5f * size, 0.51f * size);
    parts.push_back(door);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_donut(float size) {
    std::vector<Part> parts;
    Part dough; dough.name = "Dough";
    dough.mesh = create_torus(0.3f * size, 0.12f * size, 24, 12);
    dough.material.diffuse = vec4(0.75f, 0.5f, 0.25f, 1); dough.material.name = "Dough";
    parts.push_back(dough);
    Part glaze; glaze.name = "Glaze";
    glaze.mesh = create_torus(0.3f * size, 0.125f * size, 24, 12);
    glaze.material.diffuse = vec4(0.9f, 0.4f, 0.6f, 1); glaze.material.name = "PinkGlaze";
    glaze.scale = vec3(1.01f, 1.02f, 1.01f);
    glaze.position = vec3(0, 0.01f * size, 0);
    parts.push_back(glaze);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_satellite(float size) {
    std::vector<Part> parts;
    Part body; body.name = "Body";
    body.mesh = create_cube(0.3f * size);
    body.material.diffuse = vec4(0.7f, 0.72f, 0.75f, 1); body.material.metallic = 0.9f; body.material.name = "Metal";
    parts.push_back(body);
    Part dish; dish.name = "Dish";
    dish.mesh = create_sphere(0.15f * size, 12, 8);
    dish.material.diffuse = vec4(0.8f, 0.8f, 0.85f, 1); dish.material.metallic = 0.7f; dish.material.name = "Metal";
    dish.position = vec3(0, 0, 0.2f * size);
    dish.scale = vec3(1, 1, 0.3f);
    parts.push_back(dish);
    for (int i = 0; i < 2; i++) {
        Part panel; panel.name = "SolarPanel_" + std::to_string(i);
        panel.mesh = create_cube(0.1f);
        panel.material.diffuse = vec4(0.1f, 0.15f, 0.4f, 1); panel.material.metallic = 0.5f; panel.material.name = "SolarPanel";
        panel.scale = vec3(0.5f * size, 0.01f * size, 0.3f * size);
        panel.position = vec3((i == 0 ? -0.4f : 0.4f) * size, 0, 0);
        parts.push_back(panel);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_jewelry(float size) {
    std::vector<Part> parts;
    Part ring; ring.name = "Ring";
    ring.mesh = create_torus(0.12f * size, 0.02f * size, 24, 12);
    ring.material.diffuse = vec4(1.0f, 0.84f, 0.0f, 1); ring.material.metallic = 0.95f; ring.material.roughness = 0.05f; ring.material.name = "Gold";
    parts.push_back(ring);
    Part gem; gem.name = "Gem";
    gem.mesh = create_ico_sphere(0.04f * size, 2);
    gem.material.diffuse = vec4(0.2f, 0.5f, 1.0f, 0.9f); gem.material.opacity = 0.9f; gem.material.shininess = 200; gem.material.name = "Sapphire";
    gem.position = vec3(0, 0.12f * size, 0);
    gem.scale = vec3(0.8f, 1.2f, 0.8f);
    parts.push_back(gem);
    Part band; band.name = "Band";
    band.mesh = create_torus(0.12f * size, 0.012f * size, 24, 8);
    band.material.diffuse = vec4(0.85f, 0.85f, 0.88f, 1); band.material.metallic = 0.95f; band.material.name = "Platinum";
    band.position = vec3(0, 0.005f * size, 0);
    parts.push_back(band);
    return parts;
}

// ObjectGenerator
ObjectGenerator::ObjectGenerator() {}

Material ObjectGenerator::create_material(const vec4& color, const std::string& type) {
    Material m;
    m.diffuse = color;
    if (color.a < 1.0f) m.opacity = color.a;
    if (type == "metallic") { m.metallic = 0.9f; m.roughness = 0.15f; m.shininess = 100; }
    else if (type == "glossy") { m.shininess = 120; m.roughness = 0.05f; }
    else if (type == "matte") { m.shininess = 2; m.roughness = 1.0f; }
    else if (type == "glass") { m.opacity = 0.3f; m.shininess = 150; m.diffuse.a = 0.3f; }
    else if (type == "emissive") { m.emission = color * 0.5f; }
    else if (type == "wireframe") { m.wireframe = true; }
    return m;
}

void ObjectGenerator::add_parts_to_scene(Scene* scene, const std::vector<ProceduralEngine::Part>& parts) {
    for (auto& p : parts) {
        auto node = std::make_unique<SceneNode>();
        node->name = p.name;
        node->position = p.position;
        node->rotation = p.rotation;
        node->scale = p.scale;
        node->mesh = std::make_unique<Mesh>(p.mesh.clone());
        node->material = std::make_unique<Material>(p.material);
        scene->add_node(std::move(node));
    }
}

void ObjectGenerator::apply_arrangement(std::vector<SceneNode*>& nodes, const ParsedPrompt& p, Scene* scene) {
    if (nodes.empty() || p.arrangement.empty()) return;
    int count = (int)nodes.size();
    float spacing = 1.5f * p.size;

    for (int i = 0; i < count; i++) {
        SceneNode* node = nodes[i];
        if (p.arrangement == "line") {
            node->position.x = (i - count * 0.5f) * spacing;
        } else if (p.arrangement == "circle") {
            float angle = (float)i / count * 2.0f * (float)M_PI;
            node->position.x = cosf(angle) * spacing * count * 0.3f;
            node->position.z = sinf(angle) * spacing * count * 0.3f;
        } else if (p.arrangement == "grid") {
            int cols = (int)ceilf(sqrtf((float)count));
            int row = i / cols;
            int col = i % cols;
            node->position.x = (col - cols * 0.5f) * spacing;
            node->position.z = (row - (count / cols) * 0.5f) * spacing;
        } else if (p.arrangement == "spiral") {
            float t = (float)i / count;
            float angle = t * 4.0f * (float)M_PI;
            float radius = t * spacing * count * 0.15f;
            node->position.x = cosf(angle) * radius;
            node->position.z = sinf(angle) * radius;
            node->position.y = t * spacing * 2.0f;
        }
    }
}

Scene* ObjectGenerator::generate_from_prompt(const std::string& prompt, Scene* scene) {
    ParsedPrompt parsed = prompt_engine.parse(prompt);
    last_description = prompt_engine.get_description(parsed);

    if (!scene) { scene = new Scene(); }
    scene->clear();

    std::vector<SceneNode*> created_nodes;

    if (!parsed.scene_type.empty()) {
        for (int c = 0; c < parsed.count; c++) {
            std::vector<ProceduralEngine::Part> parts;
            if (parsed.scene_type == "house") parts = proc_engine.generate_house(parsed.size);
            else if (parsed.scene_type == "tree") parts = proc_engine.generate_tree(parsed.size * 3);
            else if (parsed.scene_type == "car") parts = proc_engine.generate_car(parsed.size);
            else if (parsed.scene_type == "robot") parts = proc_engine.generate_robot(parsed.size);
            else if (parsed.scene_type == "castle") parts = proc_engine.generate_castle(parsed.size);
            else if (parsed.scene_type == "spaceship") parts = proc_engine.generate_spaceship(parsed.size);
            else if (parsed.scene_type == "sword") parts = proc_engine.generate_sword(parsed.size);
            else if (parsed.scene_type == "chair") parts = proc_engine.generate_chair(parsed.size);
            else if (parsed.scene_type == "table") parts = proc_engine.generate_table(parsed.size);
            else if (parsed.scene_type == "star") parts = proc_engine.generate_star(parsed.size);
            else if (parsed.scene_type == "heart") parts = proc_engine.generate_heart(parsed.size);
            else if (parsed.scene_type == "mountain") parts = proc_engine.generate_mountain(parsed.size);
            else if (parsed.scene_type == "snowman") parts = proc_engine.generate_snowman(parsed.size);
            else if (parsed.scene_type == "diamond") parts = proc_engine.generate_diamond(parsed.size);
            else if (parsed.scene_type == "spiral") parts = proc_engine.generate_spiral(parsed.size);
            else if (parsed.scene_type == "dna") parts = proc_engine.generate_dna(parsed.size);
            else if (parsed.scene_type == "skull") parts = proc_engine.generate_skull(parsed.size);
            else if (parsed.scene_type == "airplane") parts = proc_engine.generate_airplane(parsed.size);
            else if (parsed.scene_type == "boat") parts = proc_engine.generate_boat(parsed.size);
            else if (parsed.scene_type == "door") parts = proc_engine.generate_door(parsed.size);
            else if (parsed.scene_type == "window") parts = proc_engine.generate_window(parsed.size);
            else if (parsed.scene_type == "shelf") parts = proc_engine.generate_shelf(parsed.size);
            else if (parsed.scene_type == "lamp") parts = proc_engine.generate_lamp(parsed.size);
            else if (parsed.scene_type == "brain") parts = proc_engine.generate_brain(parsed.size);
            else if (parsed.scene_type == "flower") parts = proc_engine.generate_flower(parsed.size);
            else if (parsed.scene_type == "bone") parts = proc_engine.generate_bone(parsed.size);
            else if (parsed.scene_type == "key") parts = proc_engine.generate_key(parsed.size);
            else if (parsed.scene_type == "helmet") parts = proc_engine.generate_helmet(parsed.size);
            else if (parsed.scene_type == "battery") parts = proc_engine.generate_battery(parsed.size);
            else if (parsed.scene_type == "ice") parts = proc_engine.generate_ice_crystal(parsed.size);
            else if (parsed.scene_type == "mushroom") parts = proc_engine.generate_mushroom(parsed.size);
            else if (parsed.scene_type == "pyramid") parts = proc_engine.generate_pyramid(parsed.size);
            else if (parsed.scene_type == "donut") parts = proc_engine.generate_donut(parsed.size);
            else if (parsed.scene_type == "satellite") parts = proc_engine.generate_satellite(parsed.size);
            else if (parsed.scene_type == "jewelry") parts = proc_engine.generate_jewelry(parsed.size);

            size_t before = scene->get_all_nodes().size();
            add_parts_to_scene(scene, parts);

            if (parsed.color.x >= 0 && c == 0) {
                auto all = scene->get_all_nodes();
                for (size_t i = before; i < all.size(); i++) {
                    if (all[i]->material) all[i]->material->diffuse = parsed.color;
                }
            }

            auto all = scene->get_all_nodes();
            for (size_t i = before; i < all.size(); i++) {
                created_nodes.push_back(all[i]);
            }
        }
    } else {
        for (auto& shape : parsed.shapes) {
            auto& creators = get_primitive_creators();
            auto it = creators.find(shape);
            if (it != creators.end()) {
                for (int c = 0; c < parsed.count; c++) {
                    Mesh m = it->second(parsed.size);
                    auto node = std::make_unique<SceneNode>();
                    node->name = shape + (parsed.count > 1 ? "_" + std::to_string(c + 1) : "");
                    node->mesh = std::make_unique<Mesh>(m);
                    vec4 col = parsed.color.x >= 0 ? parsed.color : vec4(0.8f, 0.2f, 0.2f, 1);
                    node->material = std::make_unique<Material>(create_material(col, parsed.material_type));
                    SceneNode* raw = node.get();
                    scene->add_node(std::move(node));
                    created_nodes.push_back(raw);
                }
            }
        }
    }

    apply_arrangement(created_nodes, parsed, scene);

    auto all_nodes = scene->get_all_nodes();
    for (auto& mod : parsed.modifiers) {
        if (mod == "smooth") for (auto* n : all_nodes) if (n->mesh) { n->mesh->subdivide(); n->mesh->subdivide(); }
        else if (mod == "subdivide") for (auto* n : all_nodes) if (n->mesh) n->mesh->subdivide();
        else if (mod == "mirror") {
            std::vector<SceneNode*> to_add;
            for (auto* n : all_nodes) {
                auto clone = n->clone_recursive();
                clone->position.x = -clone->position.x;
                if (clone->mesh) clone->mesh->invert_faces();
                SceneNode* raw = clone.get();
                scene->add_node(std::move(clone));
                to_add.push_back(raw);
            }
            for (auto* c : to_add) created_nodes.push_back(c);
        }
    }

    return scene;
}

std::string ObjectGenerator::get_last_description() const { return last_description; }

} // namespace ocp
