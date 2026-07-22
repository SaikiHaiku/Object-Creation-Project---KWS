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
float ProceduralEngine::randf() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_engine); }

void ProceduralEngine::merge_mesh(Mesh& target, const Mesh& source, const vec3& offset) {
    uint32_t base = (uint32_t)target.vertices.size();
    for (auto& v : source.vertices)
        target.add_vertex(v.position + offset, v.normal, v.uv, v.color);
    for (auto& f : source.faces) {
        std::vector<uint32_t> fi;
        for (auto& vi : f.vertices) fi.push_back(vi + base);
        target.add_face(fi, f.normal);
    }
}

void ProceduralEngine::merge_mesh_transform(Mesh& target, const Mesh& source, const mat4& transform) {
    mat3 nm = mat3_normal_from_mat4(transform);
    uint32_t base = (uint32_t)target.vertices.size();
    for (auto& v : source.vertices) {
        vec3 pos = vec3(transform * vec4(v.position, 1.0f));
        vec3 nrm = glm::normalize(nm * v.normal);
        target.add_vertex(pos, nrm, v.uv, v.color);
    }
    for (auto& f : source.faces) {
        std::vector<uint32_t> fi;
        for (auto& vi : f.vertices) fi.push_back(vi + base);
        target.add_face(fi, f.normal);
    }
}

// ============================================================
// PROMPT ENGINE
// ============================================================

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
        {"door", "door porte"}, {"window", "window fenetre"},
        {"shelf", "shelf etagere"}, {"lamp", "lamp lumiere lampe light"},
        {"brain", "brain cerveau"}, {"flower", "flower fleur rose"},
        {"bone", "bone os"}, {"key", "key cle"},
        {"helmet", "helmet casque"}, {"battery", "battery pile batterie"},
        {"ice", "ice cristal glacier glace crystal"},
        {"mushroom", "mushroom champignon"},
        {"pyramid", "pyramide pyramid"},
        {"donut", "donut donut beignet ring"},
        {"satellite", "satellite sputnik"},
        {"jewelry", "jewelry bijou bague ring necklace"},
        {"throne", "throne trone"}, {"altar", "altar autel"},
        {"campfire", "campfire feu de camp bonfire"},
        {"crate", "crate caisse"}, {"barrel", "barrel tonneau"},
        {"bench", "bench banc"}, {"fountain", "fountain fontaine"},
        {"cart", "cart chariot wagon"}, {"lantern", "lantern lanterne"},
        {"gun", "gun pistolet pistol blaster"}, {"pistol", "pistol gun"},
        {"turret", "turret tourelle"}, {"crystal", "crystal cristal"},
        {"coral", "coral corail"}, {"butterfly", "butterfly papillon"},
        {"cat", "cat chat"}, {"dog", "dog chien"},
        {"bird", "bird oiseau"}, {"stump", "stump souche"},
        {"wall", "wall mur"}, {"grave", "grave tombe tombstone"},
        {"flag", "flag drapeau"}, {"bookshelf", "bookshelf bibliotheque"},
        {"potion", "potion"}, {"cave", "cave grotte"},
        {"tent", "tent tente"},
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

// ============================================================
// DEFORMATION UTILITIES
// ============================================================

void ProceduralEngine::deform_organic(Mesh& m, float amount, float seed) {
    for (auto& v : m.vertices) {
        float n = fbm3d(v.position.x * 3.0f + seed, v.position.y * 3.0f + seed * 0.7f, v.position.z * 3.0f + seed * 1.3f, 4);
        v.position += v.normal * n * amount;
    }
    m.dirty = true;
    m.compute_normals();
}

void ProceduralEngine::bend_mesh(Mesh& m, float amount, vec3 axis) {
    axis = glm::normalize(axis);
    for (auto& v : m.vertices) {
        float t = glm::dot(v.position, axis);
        float angle = t * amount;
        float cs = cosf(angle), sn = sinf(angle);
        vec3 perp = v.position - axis * t;
        v.position = axis * t + perp * cs + glm::cross(axis, perp) * sn;
    }
    m.dirty = true;
    m.compute_normals();
}

void ProceduralEngine::taper_mesh(Mesh& m, float top_scale, float bottom_scale) {
    float min_y = 1e10f, max_y = -1e10f;
    for (auto& v : m.vertices) { min_y = std::min(min_y, v.position.y); max_y = std::max(max_y, v.position.y); }
    float range = max_y - min_y;
    if (range < 1e-6f) return;
    for (auto& v : m.vertices) {
        float t = (v.position.y - min_y) / range;
        float s = lerp(bottom_scale, top_scale, t);
        v.position.x *= s;
        v.position.z *= s;
    }
    m.dirty = true;
    m.compute_normals();
}

void ProceduralEngine::add_random_detail(Mesh& m, float detail_size, int count) {
    if (m.vertices.empty()) return;
    std::uniform_int_distribution<int> dist(0, (int)m.vertices.size() - 1);
    for (int i = 0; i < count; i++) {
        int idx = dist(rng_engine);
        float n = noise2d(idx * 0.17f, i * 0.31f);
        m.vertices[idx].position += m.vertices[idx].normal * detail_size * (0.5f + 0.5f * n);
    }
    m.dirty = true;
    m.compute_normals();
}

// ============================================================
// ORGANIC MESH CREATORS
// ============================================================

Mesh ProceduralEngine::create_organic_sphere(float radius, int seg, int rings, float seed, float bumpiness) {
    Mesh m; m.name = "OrganicSphere";
    for (int r = 0; r <= rings; r++) {
        float phi = (float)M_PI * r / rings;
        for (int s = 0; s <= seg; s++) {
            float theta = 2.0f * (float)M_PI * s / seg;
            float n = fbm3d(cosf(theta)*sinf(phi)*2.0f+seed, cosf(phi)*2.0f+seed*0.7f, sinf(theta)*sinf(phi)*2.0f+seed*1.3f, 4);
            float r2 = radius * (1.0f + n * bumpiness);
            vec3 pos(r2*sinf(phi)*cosf(theta), r2*cosf(phi), r2*sinf(phi)*sinf(theta));
            m.add_vertex(pos, glm::normalize(pos), vec2((float)s/seg, (float)r/rings));
        }
    }
    for (int r = 0; r < rings; r++) for (int s = 0; s < seg; s++) {
        uint32_t i0 = r*(seg+1)+s;
        m.add_face({i0, i0+(uint32_t)(seg+1), i0+(uint32_t)(seg+2), i0+1});
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_organic_cylinder(float r, float h, int seg, float seed, float bulge) {
    Mesh m; m.name = "OrganicCyl"; float hh = h*0.5f;
    for (int i = 0; i <= seg; i++) {
        float theta = 2.0f*(float)M_PI*i/seg;
        float ct = cosf(theta), st = sinf(theta);
        float nt = fbm3d(ct*2.0f+seed, hh*2.0f, st*2.0f, 3)*bulge;
        float nb = fbm3d(ct*2.0f+seed+10.0f, -hh*2.0f, st*2.0f, 3)*bulge;
        float rt = r*(1.0f+nt), rb = r*(1.0f+nb);
        vec3 nrm = glm::normalize(vec3(ct*h, (rb-rt), st*h));
        m.add_vertex(vec3(ct*rt, hh, st*rt), vec3(0,1,0), vec2((float)i/seg,1));
        m.add_vertex(vec3(ct*rb, -hh, st*rb), vec3(0,-1,0), vec2((float)i/seg,0));
        m.add_vertex(vec3(ct*rb, -hh, st*rb), nrm, vec2((float)i/seg,0));
        m.add_vertex(vec3(ct*rt, hh, st*rt), nrm, vec2((float)i/seg,1));
    }
    for (int i = 0; i < seg; i++) {
        int b = i*4;
        m.add_face({(uint32_t)b, (uint32_t)(b+4), (uint32_t)(b+5), (uint32_t)(b+1)});
        m.add_face({(uint32_t)(b+2), (uint32_t)(b+3), (uint32_t)(b+7), (uint32_t)(b+6)});
    }
    uint32_t tc = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0,hh,0), vec3(0,1,0));
    uint32_t bc = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0,-hh,0), vec3(0,-1,0));
    for (int i = 0; i < seg; i++) {
        m.add_face({tc, (uint32_t)(i*4), (uint32_t)(((i+1)%(seg+1))*4)});
        m.add_face({bc, (uint32_t)(((i+1)%(seg+1))*4+1), (uint32_t)(i*4+1)});
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_stone(float size, float seed) {
    Mesh m = create_ico_sphere(size*0.5f, 3);
    for (auto& v : m.vertices) {
        float n = fbm3d(v.position.x*4.0f+seed, v.position.y*4.0f, v.position.z*4.0f, 4);
        v.position += v.normal * n * size * 0.15f;
    }
    float sx = 0.8f+0.4f*noise2d(seed,0), sy = 0.7f+0.3f*noise2d(seed,1), sz = 0.8f+0.4f*noise2d(seed,2);
    for (auto& v : m.vertices) { v.position.x *= sx; v.position.y *= sy; v.position.z *= sz; }
    for (auto& v : m.vertices) {
        float cv = 0.55f + 0.1f*fbm3d(v.position.x*6.0f+seed, v.position.y*6.0f, v.position.z*6.0f, 3);
        v.color = vec4(cv, cv*0.95f, cv*0.9f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_log(float radius, float length, float seed) {
    Mesh m = create_tapered_cylinder(radius*0.85f, radius, length, 10);
    for (auto& v : m.vertices) {
        float bark = fbm3d(v.position.x*8.0f+seed, v.position.y*3.0f, v.position.z*8.0f, 4)*radius*0.2f;
        v.position += v.normal * bark;
    }
    for (auto& v : m.vertices) {
        float cv = noise2d(v.position.y*10.0f+seed, v.position.x*10.0f)*0.08f;
        v.color = vec4(0.4f+cv, 0.25f+cv*0.5f, 0.1f+cv*0.3f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_leaf_cluster(float size, float seed) {
    Mesh m; m.name = "Leaves";
    for (int i = 0; i < 5; i++) {
        float s = size*(0.4f+0.3f*noise2d(seed+i, 0.0f));
        Mesh clump = create_organic_sphere(s, 12, 8, seed+i*7.0f, 0.25f);
        merge_mesh(m, clump, vec3(noise2d(seed+i,1)*size*0.3f, noise2d(seed+i,2)*size*0.2f, noise2d(seed+i,3)*size*0.3f));
    }
    for (auto& v : m.vertices) {
        float cv = noise3d(v.position.x*4.0f+seed, v.position.y*4.0f, v.position.z*4.0f)*0.15f;
        v.color = vec4(0.1f+cv*0.3f, 0.5f+cv, 0.1f+cv*0.2f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_rock(float size, float seed) {
    Mesh m = create_stone(size, seed);
    for (auto& v : m.vertices) {
        float crack = fbm3d(v.position.x*12.0f+seed*2.0f, v.position.y*12.0f, v.position.z*12.0f, 3);
        if (crack > 0.3f) v.position -= v.normal * size * 0.03f;
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_cloud(float size, float seed) {
    Mesh m; m.name = "Cloud";
    for (int i = 0; i < 7; i++) {
        float s = size*(0.3f+0.25f*noise2d(seed+i*3.0f, 0.0f));
        Mesh puff = create_organic_sphere(s, 10, 6, seed+i*5.0f, 0.2f);
        merge_mesh(m, puff, vec3(noise2d(seed+i*3.0f,1)*size*0.5f, noise2d(seed+i*3.0f,2)*size*0.15f, noise2d(seed+i*3.0f,3)*size*0.3f));
    }
    for (auto& v : m.vertices) {
        float cv = 0.9f + 0.1f*noise3d(v.position.x*2.0f+seed, v.position.y*2.0f, v.position.z*2.0f);
        v.color = vec4(cv, cv, cv+0.02f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_cave_entrance(float size, float seed) {
    Mesh m; m.name = "Cave";
    Mesh roof = create_stone(size*0.8f, seed);
    for (auto& v : roof.vertices) v.position.y += size*0.4f;
    merge_mesh(m, roof, vec3(0));
    Mesh arch = create_tapered_cylinder(size*0.5f, size*0.55f, size*0.3f, 12);
    taper_mesh(arch, 0.5f, 1.0f);
    for (auto& v : arch.vertices) {
        float n = fbm3d(v.position.x*4.0f+seed, v.position.y*4.0f, v.position.z*4.0f, 3)*size*0.05f;
        v.position += v.normal * n;
    }
    merge_mesh(m, arch, vec3(0, size*0.1f, 0));
    for (int i = 0; i < 3; i++) {
        Mesh stalactite = create_cone(size*0.04f*(1.0f+noise2d(seed+i,4)*0.5f), size*0.15f*(1.0f+noise2d(seed+i,5)*0.5f), 6);
        for (auto& v : stalactite.vertices) v.position.y = -v.position.y;
        merge_mesh(m, stalactite, vec3(noise2d(seed+i,6)*size*0.3f, size*0.5f, noise2d(seed+i,7)*size*0.2f));
    }
    for (auto& v : m.vertices) {
        float cv = 0.4f+0.1f*noise3d(v.position.x*3.0f+seed, v.position.y*3.0f, v.position.z*3.0f);
        v.color = vec4(cv, cv*0.95f, cv*0.9f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_tent_mesh(float size, float seed) {
    Mesh m; m.name = "Tent";
    Mesh body = create_pyramid_mesh(size*0.8f, size*0.7f, 4);
    for (auto& v : body.vertices) {
        float n = fbm3d(v.position.x*5.0f+seed, v.position.y*5.0f, v.position.z*5.0f, 3);
        v.position += v.normal * n * size * 0.02f;
        if (v.position.y > 0.0f) v.position.y *= 0.95f;
    }
    merge_mesh(m, body, vec3(0, size*0.35f, 0));
    for (auto& v : m.vertices) {
        float cv = noise2d(v.position.x*4.0f+seed, v.position.z*4.0f)*0.05f;
        v.color = vec4(0.35f+cv, 0.5f+cv, 0.3f+cv, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_barrel_mesh(float size, float seed) {
    Mesh m; m.name = "Barrel";
    Mesh body = create_organic_cylinder(size*0.3f, size*0.7f, 16, seed, 0.15f);
    taper_mesh(body, 0.9f, 0.92f);
    merge_mesh(m, body, vec3(0));
    for (int i = 0; i < 2; i++) {
        Mesh band = create_torus(size*0.31f, size*0.015f, 20, 6);
        for (auto& v : band.vertices) v.position.y += (i==0 ? 0.2f : -0.2f)*size;
        merge_mesh(m, band, vec3(0));
    }
    for (auto& v : m.vertices) {
        float cv = noise2d(v.position.y*8.0f+seed, v.position.x*8.0f)*0.06f;
        v.color = vec4(0.45f+cv, 0.3f+cv*0.5f, 0.15f+cv*0.3f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_bench_mesh(float size, float seed) {
    Mesh m; m.name = "Bench";
    Mesh seat = create_cube(size*0.8f);
    for (auto& v : seat.vertices) { v.position = vec3(v.position.x, v.position.y*0.05f, v.position.z*0.3f); }
    merge_mesh(m, seat, vec3(0, size*0.3f, 0));
    Mesh back = create_cube(size*0.8f);
    for (auto& v : back.vertices) { v.position = vec3(v.position.x, v.position.y*0.4f, v.position.z*0.03f); }
    merge_mesh(m, back, vec3(0, size*0.52f, -size*0.13f));
    for (int i = 0; i < 4; i++) {
        float lx = (i%2==0 ? -1.0f : 1.0f)*size*0.35f;
        float lz = (i<2 ? -1.0f : 1.0f)*size*0.12f;
        Mesh leg = create_tapered_cylinder(size*0.025f, size*0.03f, size*0.3f, 6);
        merge_mesh(m, leg, vec3(lx, size*0.15f, lz));
    }
    for (auto& v : m.vertices) {
        float cv = noise2d(v.position.x*5.0f+seed, v.position.z*5.0f)*0.05f;
        v.color = vec4(0.5f+cv, 0.33f+cv*0.5f, 0.13f+cv*0.3f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_fountain_mesh(float size, float seed) {
    Mesh m; m.name = "Fountain";
    Mesh basin = create_organic_cylinder(size*0.5f, size*0.15f, 20, seed, 0.05f);
    merge_mesh(m, basin, vec3(0));
    Mesh pillar = create_organic_cylinder(size*0.05f, size*0.4f, 8, seed+5.0f, 0.1f);
    merge_mesh(m, pillar, vec3(0, size*0.2f, 0));
    Mesh top = create_organic_cylinder(size*0.2f, size*0.08f, 16, seed+10.0f, 0.08f);
    merge_mesh(m, top, vec3(0, size*0.42f, 0));
    for (int i = 0; i < 4; i++) {
        float a = (float)i*(float)M_PI*0.5f+0.2f;
        Mesh spout = create_organic_cylinder(size*0.015f, size*0.08f, 6, seed+i*3.0f, 0.1f);
        bend_mesh(spout, 0.5f, vec3(0,0,1));
        merge_mesh(m, spout, vec3(cosf(a)*size*0.15f, size*0.45f, sinf(a)*size*0.15f));
    }
    for (auto& v : m.vertices) {
        float cv = 0.55f+0.08f*noise3d(v.position.x*4.0f+seed, v.position.y*4.0f, v.position.z*4.0f);
        v.color = vec4(cv, cv*0.95f, cv*0.9f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_cart_mesh(float size, float seed) {
    Mesh m; m.name = "Cart";
    Mesh body = create_cube(size*0.6f);
    for (auto& v : body.vertices) { v.position = vec3(v.position.x*1.2f, v.position.y*0.5f, v.position.z*0.8f); }
    merge_mesh(m, body, vec3(0, size*0.25f, 0));
    for (int i = 0; i < 2; i++) {
        Mesh wheel = create_torus(size*0.13f, size*0.03f, 16, 8);
        for (auto& v : wheel.vertices) std::swap(v.position.y, v.position.z);
        merge_mesh(m, wheel, vec3((i==0?-1.0f:1.0f)*size*0.3f, size*0.08f, 0));
    }
    Mesh handle = create_tapered_cylinder(size*0.02f, size*0.025f, size*0.6f, 6);
    bend_mesh(handle, 0.3f, vec3(0,0,1));
    merge_mesh(m, handle, vec3(0, size*0.35f, size*0.45f));
    for (auto& v : m.vertices) {
        float cv = noise2d(v.position.x*6.0f+seed, v.position.z*6.0f)*0.06f;
        v.color = vec4(0.5f+cv, 0.33f+cv*0.5f, 0.15f+cv*0.3f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_crate_mesh(float size, float seed) {
    Mesh m; m.name = "Crate";
    Mesh body = create_cube(size*0.8f);
    for (auto& v : body.vertices) {
        float n = fbm3d(v.position.x*8.0f+seed, v.position.y*8.0f, v.position.z*8.0f, 3)*0.003f;
        v.position += v.normal * n;
    }
    merge_mesh(m, body, vec3(0));
    for (int i = 0; i < 2; i++) {
        Mesh plank = create_cube(1.0f);
        float sz = (i==0) ? size*0.02f : size*0.02f;
        for (auto& v : plank.vertices) v.position = vec3(v.position.x*size*0.82f, v.position.y*sz, v.position.z*size*0.42f);
        merge_mesh(m, plank, vec3(0, (i==0?1.0f:-1.0f)*size*0.41f, 0));
    }
    for (auto& v : m.vertices) {
        float cv = noise2d(v.position.x*10.0f+seed, v.position.y*10.0f)*0.06f;
        v.color = vec4(0.5f+cv, 0.35f+cv*0.5f, 0.15f+cv*0.3f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_lantern_mesh(float size, float seed) {
    Mesh m; m.name = "Lantern";
    Mesh post = create_organic_cylinder(size*0.03f, size*0.8f, 8, seed, 0.05f);
    merge_mesh(m, post, vec3(0, size*0.3f, 0));
    Mesh arm = create_tapered_cylinder(size*0.015f, size*0.02f, size*0.2f, 6);
    bend_mesh(arm, 0.8f, vec3(0,0,1));
    merge_mesh(m, arm, vec3(size*0.1f, size*0.65f, 0));
    Mesh housing = create_organic_cylinder(size*0.08f, size*0.12f, 8, seed+5.0f, 0.08f);
    merge_mesh(m, housing, vec3(size*0.2f, size*0.6f, 0));
    Mesh cap = create_cone(size*0.07f, size*0.06f, 8);
    merge_mesh(m, cap, vec3(size*0.2f, size*0.69f, 0));
    for (auto& v : m.vertices) {
        float cv = noise2d(v.position.x*8.0f+seed, v.position.y*8.0f)*0.05f;
        v.color = vec4(0.25f+cv, 0.25f+cv, 0.28f+cv, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_gun_mesh(float size, float seed) {
    Mesh m; m.name = "Gun";
    Mesh barrel = create_organic_cylinder(size*0.03f, size*0.4f, 8, seed, 0.03f);
    merge_mesh(m, barrel, vec3(0, size*0.08f, 0));
    Mesh receiver = create_cube(size*0.15f);
    for (auto& v : receiver.vertices) v.position = vec3(v.position.x, v.position.y*0.6f, v.position.z*0.8f);
    merge_mesh(m, receiver, vec3(0, size*0.04f, 0));
    Mesh grip = create_cube(size*0.08f);
    for (auto& v : grip.vertices) v.position = vec3(v.position.x*0.6f, v.position.y, v.position.z*0.8f);
    merge_mesh(m, grip, vec3(0, -size*0.06f, -size*0.02f));
    Mesh scope = create_organic_cylinder(size*0.02f, size*0.12f, 6, seed+3.0f, 0.05f);
    merge_mesh(m, scope, vec3(0, size*0.13f, 0));
    Mesh muzzle = create_cone(size*0.035f, size*0.04f, 8);
    for (auto& v : muzzle.vertices) std::swap(v.position.y, v.position.z);
    merge_mesh(m, muzzle, vec3(0, size*0.08f, size*0.22f));
    for (auto& v : m.vertices) {
        float cv = noise3d(v.position.x*10.0f+seed, v.position.y*10.0f, v.position.z*10.0f)*0.04f;
        v.color = vec4(0.3f+cv, 0.32f+cv, 0.35f+cv, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_scifi_turret_mesh(float size, float seed) {
    Mesh m; m.name = "Turret";
    Mesh base = create_organic_cylinder(size*0.25f, size*0.1f, 16, seed, 0.05f);
    merge_mesh(m, base, vec3(0, 0, 0));
    Mesh body = create_organic_cylinder(size*0.15f, size*0.25f, 12, seed+5.0f, 0.08f);
    merge_mesh(m, body, vec3(0, size*0.17f, 0));
    Mesh b1 = create_organic_cylinder(size*0.03f, size*0.35f, 8, seed+10.0f, 0.04f);
    merge_mesh(m, b1, vec3(size*0.08f, size*0.3f, 0));
    Mesh b2 = create_organic_cylinder(size*0.03f, size*0.35f, 8, seed+12.0f, 0.04f);
    merge_mesh(m, b2, vec3(-size*0.08f, size*0.3f, 0));
    Mesh sensor = create_organic_sphere(size*0.05f, 8, 6, seed+15.0f, 0.1f);
    merge_mesh(m, sensor, vec3(0, size*0.33f, size*0.12f));
    for (auto& v : m.vertices) {
        float cv = noise3d(v.position.x*6.0f+seed, v.position.y*6.0f, v.position.z*6.0f)*0.04f;
        v.color = vec4(0.4f+cv, 0.42f+cv, 0.45f+cv, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_crystal_mesh(float size, float seed) {
    Mesh m; m.name = "Crystal";
    int cnt = 4 + (int)(noise2d(seed, 0.0f)*3.0f + 3.0f);
    for (int i = 0; i < cnt; i++) {
        float cs = size*(0.2f+0.3f*noise2d(seed+i, 1.0f));
        float h = cs*(2.0f+1.5f*noise2d(seed+i, 2.0f));
        Mesh shard = create_organic_cylinder(cs*0.15f, h, 6, seed+i*7.0f, 0.05f);
        float a = noise2d(seed+i, 3.0f)*(float)M_PI*2.0f;
        float tilt = noise2d(seed+i, 4.0f)*0.3f;
        bend_mesh(shard, tilt, vec3(1,0,0));
        merge_mesh(m, shard, vec3(cosf(a)*size*0.15f*noise2d(seed+i,5.0f), h*0.5f, sinf(a)*size*0.15f*noise2d(seed+i,6.0f)));
    }
    for (auto& v : m.vertices) {
        float hue = noise3d(v.position.x*3.0f+seed, v.position.y*3.0f, v.position.z*3.0f);
        v.color = vec4(0.4f+hue*0.3f, 0.5f+hue*0.2f, 0.9f+hue*0.1f, 0.85f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_coral_mesh(float size, float seed) {
    Mesh m; m.name = "Coral";
    int branches = 5 + (int)(noise2d(seed, 0.0f)*3.0f + 3.0f);
    for (int i = 0; i < branches; i++) {
        float bh = size*(0.3f+0.4f*noise2d(seed+i, 1.0f));
        Mesh branch = create_organic_cylinder(size*0.04f, bh, 6, seed+i*5.0f, 0.15f);
        float a = (float)i/branches*(float)M_PI*2.0f+noise2d(seed+i,2.0f)*0.5f;
        bend_mesh(branch, 0.2f+0.3f*noise2d(seed+i,3.0f), vec3(sinf(a),0,cosf(a)));
        merge_mesh(m, branch, vec3(cosf(a)*size*0.1f, bh*0.5f, sinf(a)*size*0.1f));
        Mesh tip = create_organic_sphere(size*0.05f, 6, 4, seed+i*9.0f, 0.2f);
        merge_mesh(m, tip, vec3(cosf(a)*size*0.15f, bh, sinf(a)*size*0.15f));
    }
    for (auto& v : m.vertices) {
        float hue = noise3d(v.position.x*4.0f+seed, v.position.y*4.0f, v.position.z*4.0f);
        v.color = vec4(0.9f+hue*0.1f, 0.4f+hue*0.3f, 0.4f+hue*0.2f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_butterfly_mesh(float size, float seed) {
    Mesh m; m.name = "Butterfly";
    Mesh body = create_organic_cylinder(size*0.02f, size*0.15f, 6, seed, 0.05f);
    merge_mesh(m, body, vec3(0));
    Mesh head = create_organic_sphere(size*0.025f, 8, 6, seed+1.0f, 0.1f);
    merge_mesh(m, head, vec3(0, size*0.09f, 0));
    for (int side = -1; side <= 1; side += 2) {
        Mesh wing = create_organic_sphere(size*0.2f, 10, 8, seed+side*3.0f, 0.15f);
        for (auto& v : wing.vertices) v.position = vec3(v.position.x*0.3f, v.position.y*0.1f, v.position.z*1.0f);
        bend_mesh(wing, 0.1f*side, vec3(0,0,1));
        merge_mesh(m, wing, vec3(side*size*0.08f, size*0.05f, 0));
        Mesh lower = create_organic_sphere(size*0.12f, 8, 6, seed+side*5.0f, 0.15f);
        for (auto& v : lower.vertices) v.position = vec3(v.position.x*0.3f, v.position.y*0.1f, v.position.z*0.8f);
        merge_mesh(m, lower, vec3(side*size*0.06f, -size*0.02f, 0));
    }
    for (auto& v : m.vertices) {
        float hue = noise3d(v.position.x*6.0f+seed, v.position.y*6.0f, v.position.z*6.0f);
        v.color = vec4(0.2f+hue*0.4f, 0.3f+hue*0.3f, 0.8f+hue*0.2f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_cat_mesh(float size, float seed) {
    Mesh m; m.name = "Cat";
    Mesh body = create_organic_sphere(size*0.2f, 14, 10, seed, 0.08f);
    for (auto& v : body.vertices) { v.position.x *= 1.4f; v.position.y *= 0.8f; v.position.z *= 0.9f; }
    merge_mesh(m, body, vec3(0, size*0.2f, 0));
    Mesh head = create_organic_sphere(size*0.13f, 12, 8, seed+2.0f, 0.06f);
    merge_mesh(m, head, vec3(size*0.2f, size*0.4f, 0));
    for (int side = -1; side <= 1; side += 2) {
        Mesh ear = create_pyramid_mesh(size*0.04f, size*0.07f, 3);
        merge_mesh(m, ear, vec3(size*0.22f, size*0.52f, side*size*0.06f));
    }
    Mesh tail = create_organic_cylinder(size*0.02f, size*0.25f, 6, seed+4.0f, 0.1f);
    bend_mesh(tail, 1.2f, vec3(0,0,1));
    merge_mesh(m, tail, vec3(-size*0.2f, size*0.25f, 0));
    for (int i = 0; i < 4; i++) {
        float lx = (i%2==0?-1.0f:1.0f)*size*0.08f, lz = (i<2?-1.0f:1.0f)*size*0.07f;
        Mesh paw = create_organic_cylinder(size*0.03f, size*0.12f, 6, seed+i*3.0f, 0.08f);
        merge_mesh(m, paw, vec3(lx, size*0.06f, lz));
    }
    for (auto& v : m.vertices) {
        float cv = noise3d(v.position.x*6.0f+seed, v.position.y*6.0f, v.position.z*6.0f)*0.08f;
        v.color = vec4(0.7f+cv, 0.5f+cv, 0.3f+cv, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_dog_mesh(float size, float seed) {
    Mesh m; m.name = "Dog";
    Mesh body = create_organic_sphere(size*0.22f, 14, 10, seed, 0.07f);
    for (auto& v : body.vertices) { v.position.x *= 1.5f; v.position.y *= 0.75f; }
    merge_mesh(m, body, vec3(0, size*0.2f, 0));
    Mesh head = create_organic_sphere(size*0.14f, 12, 8, seed+2.0f, 0.06f);
    for (auto& v : head.vertices) v.position.z *= 1.2f;
    merge_mesh(m, head, vec3(size*0.25f, size*0.38f, 0));
    Mesh snout = create_organic_cylinder(size*0.04f, size*0.08f, 6, seed+3.0f, 0.08f);
    merge_mesh(m, snout, vec3(size*0.35f, size*0.35f, 0));
    for (int side = -1; side <= 1; side += 2) {
        Mesh ear = create_organic_sphere(size*0.06f, 8, 6, seed+side*3.0f, 0.15f);
        for (auto& v : ear.vertices) v.position.y *= 0.5f;
        merge_mesh(m, ear, vec3(size*0.2f, size*0.48f, side*size*0.1f));
    }
    Mesh tail = create_organic_cylinder(size*0.02f, size*0.15f, 6, seed+5.0f, 0.1f);
    bend_mesh(tail, 0.8f, vec3(1,0,0));
    merge_mesh(m, tail, vec3(-size*0.25f, size*0.3f, 0));
    for (int i = 0; i < 4; i++) {
        float lx = (i%2==0?-1.0f:1.0f)*size*0.1f, lz = (i<2?-1.0f:1.0f)*size*0.08f;
        Mesh leg = create_organic_cylinder(size*0.035f, size*0.15f, 6, seed+i*4.0f, 0.08f);
        merge_mesh(m, leg, vec3(lx, size*0.07f, lz));
    }
    for (auto& v : m.vertices) {
        float cv = noise3d(v.position.x*5.0f+seed, v.position.y*5.0f, v.position.z*5.0f)*0.08f;
        v.color = vec4(0.55f+cv, 0.4f+cv, 0.25f+cv, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_bird_mesh(float size, float seed) {
    Mesh m; m.name = "Bird";
    Mesh body = create_organic_sphere(size*0.12f, 12, 8, seed, 0.08f);
    for (auto& v : body.vertices) { v.position.x *= 1.3f; v.position.y *= 0.85f; }
    merge_mesh(m, body, vec3(0, size*0.1f, 0));
    Mesh head = create_organic_sphere(size*0.07f, 10, 6, seed+2.0f, 0.06f);
    merge_mesh(m, head, vec3(size*0.12f, size*0.2f, 0));
    Mesh beak = create_pyramid_mesh(size*0.025f, size*0.05f, 3);
    for (auto& v : beak.vertices) std::swap(v.position.y, v.position.z);
    merge_mesh(m, beak, vec3(size*0.2f, size*0.2f, 0));
    for (int side = -1; side <= 1; side += 2) {
        Mesh wing = create_organic_sphere(size*0.1f, 8, 6, seed+side*3.0f, 0.12f);
        for (auto& v : wing.vertices) v.position = vec3(v.position.x*0.3f, v.position.y*0.15f, v.position.z*1.2f);
        bend_mesh(wing, 0.15f*side, vec3(0,0,1));
        merge_mesh(m, wing, vec3(-size*0.02f, size*0.12f, side*size*0.08f));
    }
    Mesh tailf = create_organic_sphere(size*0.04f, 6, 4, seed+6.0f, 0.1f);
    for (auto& v : tailf.vertices) { v.position.x *= 0.5f; v.position.z *= 2.0f; }
    merge_mesh(m, tailf, vec3(-size*0.15f, size*0.15f, 0));
    for (auto& v : m.vertices) {
        float cv = noise3d(v.position.x*8.0f+seed, v.position.y*8.0f, v.position.z*8.0f)*0.1f;
        v.color = vec4(0.3f+cv, 0.5f+cv*0.5f, 0.8f+cv*0.3f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_tree_stump_mesh(float size, float seed) {
    Mesh m; m.name = "Stump";
    Mesh trunk = create_tapered_cylinder(size*0.2f, size*0.25f, size*0.3f, 10);
    for (auto& v : trunk.vertices) {
        float bark = fbm3d(v.position.x*8.0f+seed, v.position.y*4.0f, v.position.z*8.0f, 4)*size*0.02f;
        v.position += v.normal * bark;
    }
    merge_mesh(m, trunk, vec3(0, size*0.15f, 0));
    Mesh top = create_organic_cylinder(size*0.2f, size*0.02f, 16, seed+5.0f, 0.03f);
    merge_mesh(m, top, vec3(0, size*0.31f, 0));
    for (int i = 0; i < 3; i++) {
        float a = (float)i*(float)M_PI*2.0f/3.0f;
        Mesh root = create_tapered_cylinder(size*0.03f, size*0.06f, size*0.15f, 6);
        bend_mesh(root, 0.5f, vec3(0,0,1));
        merge_mesh(m, root, vec3(cosf(a)*size*0.22f, size*0.02f, sinf(a)*size*0.22f));
    }
    for (auto& v : m.vertices) {
        float cv = noise2d(v.position.y*10.0f+seed, v.position.x*10.0f)*0.08f;
        v.color = vec4(0.4f+cv, 0.25f+cv*0.5f, 0.1f+cv*0.3f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_rock_wall_mesh(float size, float seed) {
    Mesh m; m.name = "RockWall";
    for (int row = 0; row < 3; row++) {
        int cols = 3 + (row % 2);
        for (int col = 0; col < cols; col++) {
            float w = size*(0.25f+0.05f*noise2d(seed+row*10+col, 0));
            float h = size*(0.12f+0.03f*noise2d(seed+row*10+col, 1));
            Mesh stone = create_stone(w*0.5f, seed+row*10+col);
            for (auto& v : stone.vertices) { v.position.x *= w/size; v.position.y *= h/size; v.position.z *= 0.6f; }
            float xoff = (col - cols*0.5f)*size*0.28f + noise2d(seed+row*10+col, 2)*size*0.03f;
            float yoff = row*size*0.22f + noise2d(seed+row*10+col, 3)*size*0.02f;
            merge_mesh(m, stone, vec3(xoff, yoff, 0));
        }
    }
    for (auto& v : m.vertices) {
        float cv = 0.5f+0.1f*noise3d(v.position.x*4.0f+seed, v.position.y*4.0f, v.position.z*4.0f);
        v.color = vec4(cv, cv*0.95f, cv*0.9f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_grave_mesh(float size, float seed) {
    Mesh m; m.name = "Grave";
    Mesh stone = create_organic_cylinder(size*0.2f, size*0.5f, 8, seed, 0.05f);
    taper_mesh(stone, 0.8f, 1.0f);
    for (auto& v : stone.vertices) {
        if (v.position.y > size*0.15f) v.position.x *= 1.0f - (v.position.y-size*0.15f)/(size*0.1f)*0.3f;
    }
    merge_mesh(m, stone, vec3(0, size*0.25f, 0));
    for (auto& v : m.vertices) {
        float cv = 0.55f+0.08f*noise3d(v.position.x*5.0f+seed, v.position.y*5.0f, v.position.z*5.0f);
        v.color = vec4(cv, cv*0.95f, cv*0.9f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_flag_mesh(float size, float seed) {
    Mesh m; m.name = "Flag";
    Mesh pole = create_organic_cylinder(size*0.02f, size*0.8f, 6, seed, 0.03f);
    merge_mesh(m, pole, vec3(0, size*0.4f, 0));
    Mesh cloth = create_plane(size*0.4f, 8);
    for (auto& v : cloth.vertices) {
        float wave = sinf(v.position.x*6.0f+v.position.y*3.0f)*size*0.03f + noise2d(v.position.x*4.0f+seed, v.position.y*4.0f)*size*0.02f;
        v.position.z += wave;
    }
    merge_mesh(m, cloth, vec3(size*0.2f, size*0.6f, 0));
    for (auto& v : m.vertices) {
        if (v.position.z > size*0.01f) {
            float cv = noise2d(v.position.x*8.0f+seed, v.position.y*8.0f)*0.1f;
            v.color = vec4(0.8f+cv, 0.15f+cv, 0.1f+cv, 1.0f);
        }
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_bookshelf_mesh(float size, float seed) {
    Mesh m; m.name = "Bookshelf";
    Mesh frame = create_cube(size*0.6f);
    for (auto& v : frame.vertices) v.position = vec3(v.position.x, v.position.y*1.2f, v.position.z*0.3f);
    for (auto& v : frame.vertices) {
        float n = fbm3d(v.position.x*6.0f+seed, v.position.y*6.0f, v.position.z*6.0f, 3)*0.003f;
        v.position += v.normal * n;
    }
    merge_mesh(m, frame, vec3(0, size*0.6f, 0));
    for (int row = 0; row < 3; row++) {
        float shelf_y = size*0.25f + row*size*0.35f;
        for (int b = 0; b < 4; b++) {
            float bw = size*(0.04f+0.02f*noise2d(seed+row*10+b, 0));
            float bh = size*(0.15f+0.08f*noise2d(seed+row*10+b, 1));
            Mesh book = create_cube(1.0f);
            for (auto& v : book.vertices) v.position = vec3(v.position.x*bw, v.position.y*bh, v.position.z*size*0.2f);
            float xoff = -size*0.2f + b*size*0.1f + noise2d(seed+row*10+b, 2)*size*0.02f;
            merge_mesh(m, book, vec3(xoff, shelf_y+bh*0.5f+size*0.02f, 0));
        }
    }
    for (auto& v : m.vertices) {
        float cv = noise3d(v.position.x*8.0f+seed, v.position.y*8.0f, v.position.z*8.0f)*0.08f;
        v.color = vec4(0.45f+cv, 0.3f+cv*0.5f, 0.15f+cv*0.3f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_potion_mesh(float size, float seed) {
    Mesh m; m.name = "Potion";
    Mesh bottle = create_organic_cylinder(size*0.08f, size*0.25f, 10, seed, 0.06f);
    taper_mesh(bottle, 0.6f, 1.0f);
    merge_mesh(m, bottle, vec3(0, size*0.15f, 0));
    Mesh neck = create_organic_cylinder(size*0.025f, size*0.08f, 6, seed+5.0f, 0.05f);
    merge_mesh(m, neck, vec3(0, size*0.32f, 0));
    Mesh cork = create_cylinder(size*0.03f, size*0.04f, 6);
    merge_mesh(m, cork, vec3(0, size*0.38f, 0));
    Mesh liquid = create_organic_cylinder(size*0.07f, size*0.12f, 8, seed+10.0f, 0.04f);
    merge_mesh(m, liquid, vec3(0, size*0.12f, 0));
    for (auto& v : m.vertices) {
        float hue = noise3d(v.position.x*5.0f+seed, v.position.y*5.0f, v.position.z*5.0f);
        v.color = vec4(0.3f+hue*0.3f, 0.1f+hue*0.2f, 0.8f+hue*0.2f, 0.8f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_campfire_mesh(float size, float seed) {
    Mesh m; m.name = "Campfire";
    for (int i = 0; i < 8; i++) {
        float a = (float)i*(float)M_PI*2.0f/8.0f + noise2d(seed+i, 0)*0.3f;
        float r = size*0.3f + noise2d(seed+i, 1)*size*0.05f;
        Mesh stone = create_stone(size*0.08f*(1.0f+noise2d(seed+i, 2)*0.3f), seed+i*5.0f);
        merge_mesh(m, stone, vec3(cosf(a)*r, size*0.04f, sinf(a)*r));
    }
    for (int i = 0; i < 3; i++) {
        float a = (float)i*(float)M_PI*2.0f/3.0f + noise2d(seed+i, 3)*0.5f;
        Mesh logpiece = create_log(size*0.04f, size*0.35f, seed+i*7.0f);
        bend_mesh(logpiece, 0.1f, vec3(1,0,0));
        merge_mesh(m, logpiece, vec3(cosf(a)*size*0.08f, size*0.08f, sinf(a)*size*0.08f));
    }
    for (auto& v : m.vertices) {
        float cv = noise3d(v.position.x*6.0f+seed, v.position.y*6.0f, v.position.z*6.0f)*0.08f;
        v.color = vec4(0.45f+cv, 0.3f+cv*0.5f, 0.15f+cv*0.3f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_throne_mesh(float size, float seed) {
    Mesh m; m.name = "Throne";
    Mesh seat = create_cube(size*0.5f);
    for (auto& v : seat.vertices) v.position = vec3(v.position.x, v.position.y*0.08f, v.position.z*0.5f);
    for (auto& v : seat.vertices) {
        float n = fbm3d(v.position.x*6.0f+seed, v.position.y*6.0f, v.position.z*6.0f, 3)*0.003f;
        v.position += v.normal * n;
    }
    merge_mesh(m, seat, vec3(0, size*0.2f, 0));
    Mesh back = create_cube(size*0.5f);
    for (auto& v : back.vertices) v.position = vec3(v.position.x, v.position.y*0.8f, v.position.z*0.06f);
    for (auto& v : back.vertices) {
        if (v.position.y > size*0.2f) v.position.x *= 1.0f - (v.position.y-size*0.2f)/(size*0.3f)*0.15f;
    }
    merge_mesh(m, back, vec3(0, size*0.6f, -size*0.22f));
    for (int side = -1; side <= 1; side += 2) {
        Mesh arm = create_cube(size*0.1f);
        for (auto& v : arm.vertices) v.position = vec3(v.position.x*0.3f, v.position.y, v.position.z*0.4f);
        merge_mesh(m, arm, vec3(side*size*0.28f, size*0.35f, -size*0.05f));
        Mesh finial = create_organic_sphere(size*0.05f, 8, 6, seed+side*3.0f, 0.1f);
        merge_mesh(m, finial, vec3(side*size*0.28f, size*0.45f, size*0.1f));
    }
    for (int i = 0; i < 4; i++) {
        float lx = (i%2==0?-1.0f:1.0f)*size*0.2f, lz = (i<2?-1.0f:1.0f)*size*0.18f;
        Mesh leg = create_tapered_cylinder(size*0.03f, size*0.04f, size*0.2f, 6);
        merge_mesh(m, leg, vec3(lx, size*0.1f, lz));
    }
    Mesh crown = create_pyramid_mesh(size*0.12f, size*0.1f, 5);
    merge_mesh(m, crown, vec3(0, size*1.05f, -size*0.22f));
    for (auto& v : m.vertices) {
        float cv = noise3d(v.position.x*5.0f+seed, v.position.y*5.0f, v.position.z*5.0f)*0.06f;
        v.color = vec4(0.5f+cv, 0.35f+cv*0.5f, 0.15f+cv*0.3f, 1.0f);
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_altar_mesh(float size, float seed) {
    Mesh m; m.name = "Altar";
    Mesh base = create_organic_cylinder(size*0.3f, size*0.1f, 16, seed, 0.04f);
    merge_mesh(m, base, vec3(0, 0.0f, 0));
    Mesh pillar = create_organic_cylinder(size*0.22f, size*0.3f, 12, seed+5.0f, 0.06f);
    merge_mesh(m, pillar, vec3(0, size*0.2f, 0));
    Mesh top = create_organic_cylinder(size*0.35f, size*0.06f, 16, seed+10.0f, 0.03f);
    merge_mesh(m, top, vec3(0, size*0.38f, 0));
    for (auto& v : m.vertices) {
        float cv = 0.55f+0.08f*noise3d(v.position.x*4.0f+seed, v.position.y*4.0f, v.position.z*4.0f);
        v.color = vec4(cv, cv*0.95f, cv*0.9f, 1.0f);
    }
    m.compute_normals();
    return m;
}

// ============================================================
// INTERNAL MESH HELPERS
// ============================================================

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
            float cp = cosf(phi);
            float cr = 1.0f;
            if (cp < -0.2f) cr = 0.6f + 0.4f * (cp + 0.2f) / 0.8f;
            if (cp > 0.3f) cr = 1.0f - (cp - 0.3f) / 0.7f * 0.4f;
            float x = size * 0.5f * sinf(phi) * ct * cr;
            float y = size * 0.55f * cp + size * 0.1f;
            float z = size * 0.45f * sinf(phi) * st * cr;
            m.add_vertex(vec3(x, y, z), glm::normalize(vec3(x, y + 0.1f, z)), vec2((float)s / segs, (float)r / rings));
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
            if (fabsf(ct) < 0.1f && sp > 0.3f) x += (ct > 0 ? 0.02f : -0.02f) * size * sp;
            m.add_vertex(vec3(x, y, z), glm::normalize(vec3(x, y + 0.01f, z)), vec2((float)s / segs, (float)r / rings));
        }
    }
    for (int r = 0; r < rings; r++) for (int s = 0; s < segs; s++) {
        uint32_t i0 = r * (segs + 1) + s;
        m.add_face({i0, i0 + (uint32_t)(segs + 1), i0 + (uint32_t)(segs + 2), i0 + 1});
    }
    m.compute_normals();
    return m;
}

Mesh ProceduralEngine::create_l_shape(float leg1, float leg2, float thickness) {
    Mesh m; m.name = "LShape";
    Mesh a = create_cube(1.0f);
    for (auto& v : a.vertices)
        v.position = vec3(v.position.x * thickness, v.position.y * leg1 + leg1 * 0.5f - thickness * 0.5f, v.position.z * thickness);
    for (auto& v : a.vertices) m.add_vertex(v.position, v.normal, v.uv);
    for (auto& f : a.faces) { std::vector<uint32_t> fi; for (auto& vi : f.vertices) fi.push_back(vi); m.add_face(fi); }
    Mesh b = create_cube(1.0f);
    for (auto& v : b.vertices)
        v.position = vec3(v.position.x * leg2 + leg2 * 0.5f - thickness * 0.5f, v.position.y * thickness - thickness * 0.5f, v.position.z * thickness);
    uint32_t off = (uint32_t)m.vertices.size();
    for (auto& v : b.vertices) m.add_vertex(v.position, v.normal, v.uv);
    for (auto& f : b.faces) { std::vector<uint32_t> fi; for (auto& vi : f.vertices) fi.push_back(vi + off); m.add_face(fi); }
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

// ============================================================
// ORIGINAL SCENE GENERATORS (ORGANIC REWRITE)
// ============================================================

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_tree(float height) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part trunk; trunk.name = "Trunk";
    trunk.mesh = create_organic_cylinder(0.12f, height * 0.6f, 10, sd, 0.12f);
    trunk.material.diffuse = vec4(0.4f, 0.25f, 0.1f, 1); trunk.material.name = "Bark";
    trunk.position = vec3(0, height * 0.3f, 0);
    parts.push_back(trunk);
    for (int i = 0; i < 5; i++) {
        Part b; b.name = "Branch_" + std::to_string(i);
        float a = (float)M_PI * 2 * i / 5 + 0.3f + randf() * 0.2f;
        float h = height * (0.35f + 0.15f * (i % 3));
        b.mesh = create_organic_cylinder(0.03f, height * 0.25f, 6, sd + i * 7.0f, 0.15f);
        b.material.diffuse = vec4(0.45f, 0.3f, 0.12f, 1); b.material.name = "Bark";
        b.position = vec3(cosf(a) * 0.2f, h, sinf(a) * 0.2f);
        b.rotation = vec3(0, 0, rad2deg(0.5f * cosf(a)));
        parts.push_back(b);
    }
    for (int i = 0; i < 3; i++) {
        Part l; l.name = "Leaves_" + std::to_string(i);
        l.mesh = create_leaf_cluster(height * (0.25f - i * 0.03f), sd + i * 13.0f);
        l.material.diffuse = vec4(0.1f + 0.05f * i, 0.55f + 0.1f * i, 0.1f, 1); l.material.name = "Leaves";
        l.position = vec3(randf() * 0.05f, height * (0.55f + 0.12f * i), randf() * 0.05f);
        parts.push_back(l);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_house(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part body; body.name = "Body";
    body.mesh = create_cube(1.0f);
    body.material.diffuse = vec4(0.85f, 0.82f, 0.75f, 1); body.material.name = "Wall";
    body.scale = vec3(1.2f * size, size, 0.8f * size);
    deform_organic(body.mesh, size * 0.02f, sd);
    parts.push_back(body);
    Part roof; roof.name = "Roof";
    roof.mesh = create_pyramid_mesh(1.4f * size, 0.6f * size);
    roof.material.diffuse = vec4(0.6f, 0.15f, 0.1f, 1); roof.material.name = "Roof";
    roof.position = vec3(0, size * 0.8f, 0);
    deform_organic(roof.mesh, size * 0.015f, sd + 1.0f);
    parts.push_back(roof);
    Part door; door.name = "Door";
    door.mesh = create_cube(0.1f);
    door.material.diffuse = vec4(0.35f, 0.2f, 0.08f, 1); door.material.name = "Wood";
    door.scale = vec3(0.2f * size, 0.35f * size, 0.05f * size);
    door.position = vec3(0.02f * size, -size * 0.3f, 0.41f * size);
    parts.push_back(door);
    for (int i = -1; i <= 1; i += 2) {
        Part w; w.name = "Window";
        w.mesh = create_cube(0.1f);
        w.material.diffuse = vec4(0.4f, 0.6f, 0.9f, 0.8f); w.material.opacity = 0.8f; w.material.name = "Glass";
        w.scale = vec3(0.15f * size, 0.15f * size, 0.05f * size);
        w.position = vec3(i * 0.35f * size * (1.0f + randf() * 0.03f), 0.1f * size, 0.41f * size);
        parts.push_back(w);
    }
    Part chimney; chimney.name = "Chimney";
    chimney.mesh = create_organic_cylinder(0.06f, 0.35f * size, 8, sd + 5.0f, 0.1f);
    chimney.material.diffuse = vec4(0.5f, 0.12f, 0.08f, 1); chimney.material.name = "Chimney";
    chimney.position = vec3(0.35f * size, 1.05f * size, 0);
    parts.push_back(chimney);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_car(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part chassis; chassis.name = "Chassis";
    chassis.mesh = create_cube(1.0f);
    chassis.material.diffuse = vec4(0.8f, 0.1f, 0.1f, 1); chassis.material.metallic = 0.7f; chassis.material.name = "Metal";
    chassis.scale = vec3(size, 0.3f * size, 0.5f * size);
    chassis.position = vec3(0, 0.15f * size, 0);
    deform_organic(chassis.mesh, size * 0.01f, sd);
    parts.push_back(chassis);
    Part cabin; cabin.name = "Cabin";
    cabin.mesh = create_cube(1.0f);
    cabin.material.diffuse = vec4(0.7f, 0.08f, 0.08f, 1); cabin.material.metallic = 0.7f; cabin.material.name = "Metal";
    cabin.scale = vec3(0.5f * size, 0.3f * size, 0.45f * size);
    cabin.position = vec3(-0.1f * size, 0.45f * size, 0);
    parts.push_back(cabin);
    for (int i = 0; i < 4; i++) {
        Part wh; wh.name = "Wheel_" + std::to_string(i);
        wh.mesh = create_organic_cylinder(0.1f * size, 0.08f * size, 12, sd + i * 5.0f, 0.05f);
        wh.material.diffuse = vec4(0.15f, 0.15f, 0.15f, 1); wh.material.name = "Rubber";
        float x = (i < 2 ? -0.3f : 0.3f) * size;
        float z = (i % 2 == 0 ? -0.25f : 0.25f) * size;
        wh.position = vec3(x, 0, z);
        wh.rotation = vec3(90, 0, 0);
        parts.push_back(wh);
    }
    Part headlight_l; headlight_l.name = "Headlight_L";
    headlight_l.mesh = create_organic_sphere(0.04f * size, 8, 6, sd + 10.0f, 0.1f);
    headlight_l.material.diffuse = vec4(1.0f, 0.95f, 0.8f, 1); headlight_l.material.emission = vec4(0.5f, 0.48f, 0.4f, 1); headlight_l.material.name = "Light";
    headlight_l.position = vec3(0.5f * size, 0.15f * size, 0.18f * size);
    parts.push_back(headlight_l);
    Part headlight_r = headlight_l; headlight_r.name = "Headlight_R";
    headlight_r.position = vec3(0.5f * size, 0.15f * size, -0.18f * size);
    parts.push_back(headlight_r);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_robot(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part body; body.name = "Body";
    body.mesh = create_cube(0.8f * size);
    body.material.diffuse = vec4(0.6f, 0.65f, 0.7f, 1); body.material.metallic = 0.8f; body.material.name = "Metal";
    deform_organic(body.mesh, size * 0.01f, sd);
    add_random_detail(body.mesh, size * 0.005f, 20);
    parts.push_back(body);
    Part head; head.name = "Head";
    head.mesh = create_organic_sphere(0.25f * size, 16, 10, sd + 1.0f, 0.05f);
    head.material.diffuse = vec4(0.65f, 0.68f, 0.72f, 1); head.material.metallic = 0.8f; head.material.name = "Metal";
    head.position = vec3(0, 0.65f * size, 0);
    parts.push_back(head);
    for (int i = -1; i <= 1; i += 2) {
        Part eye; eye.name = "Eye";
        eye.mesh = create_organic_sphere(0.06f * size, 8, 6, sd + 2.0f, 0.1f);
        eye.material.diffuse = vec4(0, 0.8f, 1.0f, 1); eye.material.emission = vec4(0, 0.4f, 0.5f, 1); eye.material.name = "Glow";
        eye.position = vec3(i * 0.12f * size, 0.7f * size, 0.25f * size);
        parts.push_back(eye);
        Part arm; arm.name = "Arm";
        arm.mesh = create_organic_cylinder(0.075f * size, 0.35f * size, 8, sd + i * 3.0f, 0.08f);
        arm.material.diffuse = vec4(0.55f, 0.6f, 0.65f, 1); arm.material.metallic = 0.8f; arm.material.name = "Metal";
        arm.position = vec3(i * 0.55f * size, -0.05f * size, 0);
        parts.push_back(arm);
        Part leg; leg.name = "Leg";
        leg.mesh = create_organic_cylinder(0.1f * size, 0.4f * size, 8, sd + i * 4.0f, 0.06f);
        leg.material.diffuse = vec4(0.5f, 0.55f, 0.6f, 1); leg.material.metallic = 0.8f; leg.material.name = "Metal";
        leg.position = vec3(i * 0.2f * size, -0.6f * size, 0);
        parts.push_back(leg);
    }
    Part antenna; antenna.name = "Antenna";
    antenna.mesh = create_organic_cylinder(0.015f * size, 0.25f * size, 6, sd + 7.0f, 0.08f);
    antenna.material.diffuse = vec4(0.5f, 0.55f, 0.6f, 1); antenna.material.name = "Metal";
    antenna.position = vec3(0, 1.05f * size, 0);
    parts.push_back(antenna);
    Part glow; glow.name = "GlowBall";
    glow.mesh = create_organic_sphere(0.04f * size, 8, 6, sd + 8.0f, 0.1f);
    glow.material.diffuse = vec4(0, 1, 0.5f, 1); glow.material.emission = vec4(0, 0.8f, 0.4f, 1); glow.material.name = "Glow";
    glow.position = vec3(0, 1.2f * size, 0);
    parts.push_back(glow);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_castle(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part base; base.name = "Base";
    base.mesh = create_cube(1.0f);
    base.material.diffuse = vec4(0.6f, 0.55f, 0.5f, 1); base.material.name = "Stone";
    base.scale = vec3(2.0f * size, 0.8f * size, 1.5f * size);
    deform_organic(base.mesh, size * 0.02f, sd);
    parts.push_back(base);
    vec2 tower_pos[] = {{-0.9f, -0.65f}, {0.9f, -0.65f}, {-0.9f, 0.65f}, {0.9f, 0.65f}};
    for (int i = 0; i < 4; i++) {
        Part t; t.name = "Tower_" + std::to_string(i);
        t.mesh = create_organic_cylinder(0.2f * size, 1.2f * size, 12, sd + i * 5.0f, 0.08f);
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
    float sd = randf() * 100.0f;
    Part body; body.name = "Body";
    body.mesh = create_organic_cylinder(0.3f * size, 1.2f * size, 20, sd, 0.05f);
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
        deform_organic(w.mesh, size * 0.005f, sd + i * 3.0f);
        parts.push_back(w);
    }
    Part cockpit; cockpit.name = "Cockpit";
    cockpit.mesh = create_organic_sphere(0.15f * size, 12, 8, sd + 10.0f, 0.05f);
    cockpit.material.diffuse = vec4(0.3f, 0.5f, 0.8f, 0.6f); cockpit.material.opacity = 0.6f; cockpit.material.shininess = 150; cockpit.material.name = "Glass";
    cockpit.position = vec3(0.65f * size, 0.15f * size, 0);
    parts.push_back(cockpit);
    for (int i = 0; i < 3; i++) {
        Part e; e.name = "Engine_" + std::to_string(i);
        e.mesh = create_organic_cylinder(0.08f * size, 0.2f * size, 10, sd + i * 7.0f, 0.06f);
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
    float sd = randf() * 100.0f;
    Part blade; blade.name = "Blade";
    blade.mesh = create_cube(1.0f);
    blade.material.diffuse = vec4(0.8f, 0.85f, 0.9f, 1); blade.material.metallic = 0.95f; blade.material.roughness = 0.1f; blade.material.name = "Steel";
    blade.scale = vec3(0.08f * size, 0.8f * size, 0.02f * size);
    blade.position = vec3(0, 0.5f * size, 0);
    bend_mesh(blade.mesh, 0.05f, vec3(0, 0, 1));
    parts.push_back(blade);
    Part guard; guard.name = "Guard";
    guard.mesh = create_cube(1.0f);
    guard.material.diffuse = vec4(0.8f, 0.7f, 0.0f, 1); guard.material.metallic = 0.9f; guard.material.name = "Gold";
    guard.scale = vec3(0.3f * size, 0.04f * size, 0.06f * size);
    guard.position = vec3(0, 0.08f * size, 0);
    parts.push_back(guard);
    Part handle; handle.name = "Handle";
    handle.mesh = create_organic_cylinder(0.025f * size, 0.2f * size, 8, sd, 0.08f);
    handle.material.diffuse = vec4(0.35f, 0.2f, 0.1f, 1); handle.material.name = "Wood";
    handle.position = vec3(0, -0.05f * size, 0);
    parts.push_back(handle);
    Part pommel; pommel.name = "Pommel";
    pommel.mesh = create_organic_sphere(0.04f * size, 8, 6, sd + 1.0f, 0.1f);
    pommel.material.diffuse = vec4(0.8f, 0.7f, 0.0f, 1); pommel.material.metallic = 0.9f; pommel.material.name = "Gold";
    pommel.position = vec3(0, -0.18f * size, 0);
    parts.push_back(pommel);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_chair(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part seat; seat.name = "Seat";
    seat.mesh = create_cube(1.0f);
    seat.material.diffuse = vec4(0.55f, 0.35f, 0.15f, 1); seat.material.name = "Wood";
    seat.scale = vec3(0.45f * size, 0.04f * size, 0.45f * size);
    seat.position = vec3(0, 0.2f * size, 0);
    deform_organic(seat.mesh, size * 0.003f, sd);
    parts.push_back(seat);
    Part back; back.name = "Back";
    back.mesh = create_cube(1.0f);
    back.material.diffuse = vec4(0.5f, 0.32f, 0.12f, 1); back.material.name = "Wood";
    back.scale = vec3(0.45f * size, 0.4f * size, 0.04f * size);
    back.position = vec3(0, 0.42f * size, -0.2f * size);
    parts.push_back(back);
    for (int i = 0; i < 4; i++) {
        Part l; l.name = "Leg_" + std::to_string(i);
        l.mesh = create_organic_cylinder(0.02f * size, 0.2f * size, 6, sd + i * 3.0f, 0.06f);
        l.material.diffuse = vec4(0.48f, 0.3f, 0.1f, 1); l.material.name = "Wood";
        float lx = (i % 2 == 0 ? -0.18f : 0.18f) * size;
        float lz = (i < 2 ? -0.18f : 0.18f) * size;
        l.position = vec3(lx, 0.1f * size, lz);
        parts.push_back(l);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_table(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part top; top.name = "TableTop";
    top.mesh = create_cube(1.0f);
    top.material.diffuse = vec4(0.55f, 0.35f, 0.15f, 1); top.material.name = "Wood";
    top.scale = vec3(0.8f * size, 0.05f * size, 0.5f * size);
    top.position = vec3(0, 0.4f * size, 0);
    deform_organic(top.mesh, size * 0.003f, sd);
    parts.push_back(top);
    for (int i = 0; i < 4; i++) {
        Part l; l.name = "Leg_" + std::to_string(i);
        l.mesh = create_organic_cylinder(0.025f * size, 0.4f * size, 6, sd + i * 3.0f, 0.05f);
        l.material.diffuse = vec4(0.48f, 0.3f, 0.1f, 1); l.material.name = "Wood";
        float lx = (i % 2 == 0 ? -0.35f : 0.35f) * size;
        float lz = (i < 2 ? -0.2f : 0.2f) * size;
        l.position = vec3(lx, 0.2f * size, lz);
        parts.push_back(l);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_star(float size) {
    float sd = randf() * 100.0f;
    Part p; p.name = "Star"; p.mesh = create_star_mesh(size);
    deform_organic(p.mesh, size * 0.02f, sd);
    p.material.diffuse = vec4(1, 0.9f, 0, 1); p.material.name = "Gold"; p.material.shininess = 64;
    return {p};
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_heart(float size) {
    float sd = randf() * 100.0f;
    Part p; p.name = "Heart"; p.mesh = create_heart_mesh(size);
    deform_organic(p.mesh, size * 0.01f, sd);
    p.material.diffuse = vec4(0.9f, 0.1f, 0.2f, 1); p.material.name = "Red";
    return {p};
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_mountain(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part m; m.name = "Mountain";
    m.mesh = create_cone(1.0f * size, 1.5f * size, 24);
    deform_organic(m.mesh, size * 0.05f, sd);
    m.material.diffuse = vec4(0.4f, 0.38f, 0.35f, 1); m.material.name = "Rock";
    parts.push_back(m);
    Part snow; snow.name = "Snow";
    snow.mesh = create_cone(0.3f * size, 0.5f * size, 16);
    deform_organic(snow.mesh, size * 0.02f, sd + 2.0f);
    snow.material.diffuse = vec4(0.95f, 0.95f, 1.0f, 1); snow.material.name = "Snow";
    snow.position = vec3(0, 0.5f * size, 0);
    parts.push_back(snow);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_snowman(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    float radii[] = {0.3f, 0.22f, 0.15f};
    float heights[] = {0.3f, 0.72f, 1.08f};
    for (int i = 0; i < 3; i++) {
        Part b; b.name = "Ball_" + std::to_string(i);
        b.mesh = create_organic_sphere(radii[i] * size, 16, 10, sd + i * 5.0f, 0.03f);
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
        Part eye; eye.name = "Eye_" + std::to_string(i);
        eye.mesh = create_organic_sphere(0.015f * size, 6, 4, sd + 10.0f + i, 0.1f);
        eye.material.diffuse = vec4(0.05f, 0.05f, 0.05f, 1); eye.material.name = "Black";
        float angle = (float)(i - 1) * 20.0f;
        float rad = deg2rad(angle);
        eye.position = vec3(sin(rad) * 0.1f * size, 1.13f * size, cos(rad) * 0.13f * size);
        parts.push_back(eye);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_diamond(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part gem; gem.name = "Gem";
    gem.mesh = create_ico_sphere(0.3f * size, 2);
    deform_organic(gem.mesh, size * 0.01f, sd);
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
        p.mesh = create_organic_sphere(s, 8, 6, (float)i * 0.5f, 0.08f);
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
            p.mesh = create_organic_sphere(0.04f * size, 8, 6, (float)i + strand * 30.0f, 0.1f);
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
    float sd = randf() * 100.0f;
    Part cranium; cranium.name = "Cranium";
    cranium.mesh = create_skull_mesh(size);
    cranium.material.diffuse = vec4(0.9f, 0.88f, 0.82f, 1); cranium.material.name = "Bone";
    cranium.position = vec3(0, 0.2f * size, 0);
    deform_organic(cranium.mesh, size * 0.01f, sd);
    parts.push_back(cranium);
    for (int i = -1; i <= 1; i += 2) {
        Part eye; eye.name = "EyeSocket";
        eye.mesh = create_organic_sphere(0.06f * size, 8, 6, sd + 2.0f, 0.1f);
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
    float sd = randf() * 100.0f;
    Part body; body.name = "Fuselage";
    body.mesh = create_organic_cylinder(0.15f * size, 1.5f * size, 16, sd, 0.04f);
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
    float sd = randf() * 100.0f;
    Part hull; hull.name = "Hull";
    hull.mesh = create_cube(1.0f);
    hull.material.diffuse = vec4(0.55f, 0.35f, 0.15f, 1); hull.material.name = "Wood";
    hull.scale = vec3(1.2f * size, 0.3f * size, 0.4f * size);
    hull.position = vec3(0, -0.1f * size, 0);
    deform_organic(hull.mesh, size * 0.01f, sd);
    parts.push_back(hull);
    Part deck; deck.name = "Deck";
    deck.mesh = create_cube(1.0f);
    deck.material.diffuse = vec4(0.6f, 0.4f, 0.2f, 1); deck.material.name = "Wood";
    deck.scale = vec3(1.0f * size, 0.04f * size, 0.35f * size);
    deck.position = vec3(0, 0.08f * size, 0);
    parts.push_back(deck);
    Part mast; mast.name = "Mast";
    mast.mesh = create_organic_cylinder(0.02f * size, 0.8f * size, 8, sd + 3.0f, 0.08f);
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
    float sd = randf() * 100.0f;
    Part panel; panel.name = "Panel";
    panel.mesh = create_cube(1.0f);
    panel.material.diffuse = vec4(0.45f, 0.28f, 0.12f, 1); panel.material.name = "Wood";
    panel.scale = vec3(0.9f * size, 1.8f * size, 0.08f * size);
    panel.position = vec3(0, 0.9f * size, 0);
    deform_organic(panel.mesh, 0.003f * size, sd);
    add_random_detail(panel.mesh, 0.01f * size, 8);
    parts.push_back(panel);
    for (int i = 0; i < 2; i++) {
        Part frame; frame.name = "Frame_" + std::to_string(i);
        frame.mesh = create_cube(1.0f);
        frame.material.diffuse = vec4(0.4f, 0.25f, 0.1f, 1); frame.material.name = "Wood";
        float x = (i == 0 ? -0.48f : 0.48f) * size;
        frame.scale = vec3(0.06f * size, 1.85f * size, 0.1f * size);
        frame.position = vec3(x, 0.9f * size, 0);
        parts.push_back(frame);
    }
    Part top; top.name = "Top_Frame";
    top.mesh = create_cube(1.0f);
    top.material.diffuse = vec4(0.4f, 0.25f, 0.1f, 1); top.material.name = "Wood";
    top.scale = vec3(1.0f * size, 0.06f * size, 0.1f * size);
    top.position = vec3(0, 1.83f * size, 0);
    parts.push_back(top);
    Part handle; handle.name = "Handle";
    handle.mesh = create_organic_cylinder(0.015f * size, 0.12f * size, 6, sd + 5.0f, 0.1f);
    handle.material.diffuse = vec4(0.7f, 0.6f, 0.2f, 1); handle.material.name = "Metal";
    handle.position = vec3(0.35f * size, 0.85f * size, 0.05f * size);
    handle.rotation = vec3(0, 0, 90);
    parts.push_back(handle);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_window(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part frame; frame.name = "Frame";
    frame.mesh = create_cube(1.0f);
    frame.material.diffuse = vec4(0.4f, 0.25f, 0.1f, 1); frame.material.name = "Wood";
    frame.scale = vec3(1.2f * size, 1.0f * size, 0.08f * size);
    frame.position = vec3(0, 0, 0);
    parts.push_back(frame);
    Part glass; glass.name = "Glass";
    glass.mesh = create_cube(1.0f);
    glass.material.diffuse = vec4(0.6f, 0.8f, 1.0f, 0.3f); glass.material.name = "Glass";
    glass.scale = vec3(0.9f * size, 0.7f * size, 0.02f * size);
    glass.position = vec3(0, 0, 0.02f * size);
    parts.push_back(glass);
    for (int i = 0; i < 2; i++) {
        Part bar; bar.name = "Crossbar_" + std::to_string(i);
        bar.mesh = create_cube(1.0f);
        bar.material.diffuse = vec4(0.4f, 0.25f, 0.1f, 1); bar.material.name = "Wood";
        if (i == 0) {
            bar.scale = vec3(0.9f * size, 0.04f * size, 0.1f * size);
        } else {
            bar.scale = vec3(0.04f * size, 0.7f * size, 0.1f * size);
        }
        bar.position = vec3(0, 0, 0.03f * size);
        parts.push_back(bar);
    }
    Part sill; sill.name = "Sill";
    sill.mesh = create_cube(1.0f);
    sill.material.diffuse = vec4(0.4f, 0.3f, 0.15f, 1); sill.material.name = "Wood";
    sill.scale = vec3(1.3f * size, 0.05f * size, 0.15f * size);
    sill.position = vec3(0, -0.52f * size, 0.06f * size);
    deform_organic(sill.mesh, 0.002f * size, sd + 2.0f);
    parts.push_back(sill);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_shelf(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    for (int i = 0; i < 3; i++) {
        Part plank; plank.name = "Plank_" + std::to_string(i);
        plank.mesh = create_cube(1.0f);
        plank.material.diffuse = vec4(0.5f, 0.32f, 0.14f, 1); plank.material.name = "Wood";
        plank.scale = vec3(1.0f * size, 0.04f * size, 0.25f * size);
        plank.position = vec3(0, i * 0.4f * size, 0);
        deform_organic(plank.mesh, 0.002f * size, sd + i * 3.0f);
        parts.push_back(plank);
    }
    for (int i = 0; i < 2; i++) {
        Part side; side.name = "Side_" + std::to_string(i);
        side.mesh = create_cube(1.0f);
        side.material.diffuse = vec4(0.48f, 0.3f, 0.12f, 1); side.material.name = "Wood";
        float x = (i == 0 ? -0.5f : 0.5f) * size;
        side.scale = vec3(0.04f * size, 0.84f * size, 0.25f * size);
        side.position = vec3(x, 0.4f * size, 0);
        parts.push_back(side);
    }
    Part back; back.name = "Back";
    back.mesh = create_cube(1.0f);
    back.material.diffuse = vec4(0.42f, 0.28f, 0.12f, 1); back.material.name = "Wood";
    back.scale = vec3(1.04f * size, 0.84f * size, 0.02f * size);
    back.position = vec3(0, 0.4f * size, -0.12f * size);
    parts.push_back(back);
    for (int i = 0; i < 4; i++) {
        Part book; book.name = "Book_" + std::to_string(i);
        book.mesh = create_cube(1.0f);
        float r = 0.2f + randf() * 0.4f;
        float g = 0.1f + randf() * 0.3f;
        float b = 0.1f + randf() * 0.5f;
        book.material.diffuse = vec4(r, g, b, 1); book.material.name = "Cover";
        book.scale = vec3(0.06f * size, (0.2f + randf() * 0.12f) * size, 0.15f * size);
        int shelf_idx = i / 2;
        book.position = vec3((-0.15f + (i % 2) * 0.3f + randf() * 0.05f) * size, (0.04f + shelf_idx * 0.4f) * size, 0);
        parts.push_back(book);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_lamp(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part base; base.name = "Base";
    base.mesh = create_organic_cylinder(0.15f * size, 0.04f * size, 12, sd, 0.05f);
    base.material.diffuse = vec4(0.3f, 0.3f, 0.3f, 1); base.material.name = "Metal";
    base.position = vec3(0, 0.02f * size, 0);
    parts.push_back(base);
    Part pole; pole.name = "Pole";
    pole.mesh = create_organic_cylinder(0.02f * size, 0.6f * size, 8, sd + 2.0f, 0.08f);
    pole.material.diffuse = vec4(0.35f, 0.3f, 0.25f, 1); pole.material.name = "Metal";
    pole.position = vec3(0, 0.34f * size, 0);
    parts.push_back(pole);
    Part shade; shade.name = "Shade";
    shade.mesh = create_organic_cylinder(0.2f * size, 0.2f * size, 12, sd + 5.0f, 0.1f);
    shade.material.diffuse = vec4(0.9f, 0.85f, 0.7f, 1); shade.material.name = "Fabric";
    shade.scale = vec3(1, 0.6f, 1);
    shade.position = vec3(0, 0.68f * size, 0);
    parts.push_back(shade);
    Part bulb; bulb.name = "Bulb";
    bulb.mesh = create_organic_sphere(0.04f * size, 8, 6, sd + 8.0f, 0.15f);
    bulb.material.diffuse = vec4(1.0f, 0.95f, 0.7f, 1); bulb.material.name = "Light";
    bulb.position = vec3(0, 0.65f * size, 0);
    parts.push_back(bulb);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_brain(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part main; main.name = "Brain";
    main.mesh = create_brain_mesh(size * 0.5f);
    main.material.diffuse = vec4(0.85f, 0.55f, 0.55f, 1); main.material.name = "Flesh";
    main.position = vec3(0, 0, 0);
    deform_organic(main.mesh, 0.02f * size, sd);
    parts.push_back(main);
    Part stem; stem.name = "Stem";
    stem.mesh = create_organic_cylinder(0.08f * size, 0.15f * size, 8, sd + 3.0f, 0.15f);
    stem.material.diffuse = vec4(0.8f, 0.5f, 0.5f, 1); stem.material.name = "Flesh";
    stem.position = vec3(0, -0.2f * size, -0.05f * size);
    stem.rotation = vec3(30, 0, 0);
    parts.push_back(stem);
    for (int i = 0; i < 6; i++) {
        Part fold; fold.name = "Fold_" + std::to_string(i);
        float a = (float)i / 6.0f * (float)M_PI * 2 + randf() * 0.3f;
        float r = 0.15f + randf() * 0.08f;
        fold.mesh = create_organic_cylinder(0.02f * size, r * size, 6, sd + i * 5.0f, 0.2f);
        fold.material.diffuse = vec4(0.9f, 0.6f + randf() * 0.1f, 0.6f, 1); fold.material.name = "Flesh";
        fold.position = vec3(cosf(a) * 0.12f * size, sinf(a) * 0.08f * size, 0.15f * size);
        fold.rotation = vec3(rad2deg(sinf(a) * 0.5f), 0, rad2deg(cosf(a) * 0.5f));
        parts.push_back(fold);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_flower(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part stem; stem.name = "Stem";
    stem.mesh = create_organic_cylinder(0.015f * size, 0.5f * size, 6, sd, 0.1f);
    stem.material.diffuse = vec4(0.15f, 0.5f, 0.1f, 1); stem.material.name = "Plant";
    stem.position = vec3(0, 0.25f * size, 0);
    parts.push_back(stem);
    for (int i = 0; i < 5; i++) {
        Part petal; petal.name = "Petal_" + std::to_string(i);
        float a = (float)i / 5.0f * (float)M_PI * 2 + randf() * 0.2f;
        petal.mesh = create_cube(1.0f);
        petal.material.diffuse = vec4(0.8f + randf() * 0.15f, 0.2f + randf() * 0.3f, 0.3f + randf() * 0.2f, 1);
        petal.material.name = "Petal";
        petal.scale = vec3(0.08f * size, 0.02f * size, 0.12f * size);
        petal.position = vec3(cosf(a) * 0.06f * size, 0.52f * size, sinf(a) * 0.06f * size);
        petal.rotation = vec3(rad2deg(0.5f), rad2deg(a), 0);
        deform_organic(petal.mesh, 0.005f * size, sd + i * 7.0f);
        parts.push_back(petal);
    }
    Part center; center.name = "Center";
    center.mesh = create_organic_sphere(0.03f * size, 8, 6, sd + 10.0f, 0.2f);
    center.material.diffuse = vec4(1.0f, 0.9f, 0.1f, 1); center.material.name = "Pistil";
    center.position = vec3(0, 0.53f * size, 0);
    parts.push_back(center);
    Part leaf1; leaf1.name = "Leaf_1";
    leaf1.mesh = create_leaf_cluster(0.08f * size, sd + 3.0f);
    leaf1.material.diffuse = vec4(0.1f, 0.55f, 0.1f, 1); leaf1.material.name = "Leaf";
    leaf1.position = vec3(0.05f * size, 0.2f * size, 0);
    leaf1.rotation = vec3(0, 0, -30);
    parts.push_back(leaf1);
    Part leaf2; leaf2.name = "Leaf_2";
    leaf2.mesh = create_leaf_cluster(0.07f * size, sd + 6.0f);
    leaf2.material.diffuse = vec4(0.12f, 0.5f, 0.12f, 1); leaf2.material.name = "Leaf";
    leaf2.position = vec3(-0.04f * size, 0.3f * size, 0.03f * size);
    leaf2.rotation = vec3(10, 0, 25);
    parts.push_back(leaf2);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_bone(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part shaft; shaft.name = "Shaft";
    shaft.mesh = create_organic_cylinder(0.03f * size, 0.5f * size, 8, sd, 0.08f);
    shaft.material.diffuse = vec4(0.9f, 0.88f, 0.8f, 1); shaft.material.name = "Bone";
    shaft.position = vec3(0, 0, 0);
    parts.push_back(shaft);
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            Part knob; knob.name = "Knob_" + std::to_string(i * 2 + j);
            knob.mesh = create_organic_sphere(0.05f * size, 8, 6, sd + (i * 2 + j) * 5.0f, 0.2f);
            knob.material.diffuse = vec4(0.92f, 0.9f, 0.82f, 1); knob.material.name = "Bone";
            float x = (i == 0 ? -0.22f : 0.22f) * size;
            float z = (j == 0 ? -0.04f : 0.04f) * size;
            knob.position = vec3(x, 0, z);
            parts.push_back(knob);
        }
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_key(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part shaft; shaft.name = "Shaft";
    shaft.mesh = create_organic_cylinder(0.012f * size, 0.35f * size, 6, sd, 0.06f);
    shaft.material.diffuse = vec4(0.75f, 0.65f, 0.2f, 1); shaft.material.name = "Metal";
    shaft.position = vec3(0, 0, 0);
    parts.push_back(shaft);
    Part bow; bow.name = "Bow";
    bow.mesh = create_ring(0.06f * size, 0.035f * size, 12);
    bow.material.diffuse = vec4(0.75f, 0.65f, 0.2f, 1); bow.material.name = "Metal";
    bow.position = vec3(0, 0.22f * size, 0);
    parts.push_back(bow);
    for (int i = 0; i < 3; i++) {
        Part tooth; tooth.name = "Tooth_" + std::to_string(i);
        tooth.mesh = create_cube(1.0f);
        tooth.material.diffuse = vec4(0.78f, 0.68f, 0.22f, 1); tooth.material.name = "Metal";
        float h = 0.04f + i * 0.06f;
        tooth.scale = vec3(0.03f * size, 0.04f * size, 0.02f * size);
        tooth.position = vec3(0.02f * size, -h * size, 0);
        parts.push_back(tooth);
    }
    deform_organic(parts[0].mesh, 0.001f * size, sd);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_helmet(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part dome; dome.name = "Dome";
    dome.mesh = create_organic_sphere(0.3f * size, 16, 12, sd, 0.08f);
    dome.material.diffuse = vec4(0.4f, 0.42f, 0.45f, 1); dome.material.name = "Metal";
    dome.scale = vec3(1, 0.8f, 1.05f);
    dome.position = vec3(0, 0.1f * size, 0);
    parts.push_back(dome);
    Part visor; visor.name = "Visor";
    visor.mesh = create_cube(1.0f);
    visor.material.diffuse = vec4(0.2f, 0.5f, 0.8f, 0.6f); visor.material.name = "Glass";
    visor.scale = vec3(0.35f * size, 0.12f * size, 0.1f * size);
    visor.position = vec3(0, 0.05f * size, 0.28f * size);
    parts.push_back(visor);
    Part brim; brim.name = "Brim";
    brim.mesh = create_organic_cylinder(0.32f * size, 0.04f * size, 16, sd + 3.0f, 0.06f);
    brim.material.diffuse = vec4(0.38f, 0.4f, 0.42f, 1); brim.material.name = "Metal";
    brim.position = vec3(0, -0.05f * size, 0);
    parts.push_back(brim);
    Part ridge; ridge.name = "Ridge";
    ridge.mesh = create_organic_cylinder(0.02f * size, 0.35f * size, 6, sd + 5.0f, 0.1f);
    ridge.material.diffuse = vec4(0.5f, 0.45f, 0.2f, 1); ridge.material.name = "Metal";
    ridge.position = vec3(0, 0.2f * size, 0);
    ridge.rotation = vec3(90, 0, 0);
    parts.push_back(ridge);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_battery(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part body; body.name = "Body";
    body.mesh = create_organic_cylinder(0.12f * size, 0.4f * size, 12, sd, 0.04f);
    body.material.diffuse = vec4(0.2f, 0.2f, 0.2f, 1); body.material.name = "Metal";
    body.position = vec3(0, 0, 0);
    parts.push_back(body);
    Part terminal; terminal.name = "Terminal";
    terminal.mesh = create_organic_cylinder(0.04f * size, 0.03f * size, 8, sd + 2.0f, 0.1f);
    terminal.material.diffuse = vec4(0.7f, 0.7f, 0.7f, 1); terminal.material.name = "Metal";
    terminal.position = vec3(0, 0.22f * size, 0);
    parts.push_back(terminal);
    Part label; label.name = "Label";
    label.mesh = create_cube(1.0f);
    label.material.diffuse = vec4(0.8f, 0.15f, 0.1f, 1); label.material.name = "Plastic";
    label.scale = vec3(0.2f * size, 0.2f * size, 0.01f * size);
    label.position = vec3(0, 0, 0.12f * size);
    parts.push_back(label);
    Part stripe; stripe.name = "Stripe";
    stripe.mesh = create_cube(1.0f);
    stripe.material.diffuse = vec4(0.1f, 0.6f, 0.1f, 1); stripe.material.name = "Plastic";
    stripe.scale = vec3(0.25f * size, 0.03f * size, 0.01f * size);
    stripe.position = vec3(0, -0.05f * size, 0.12f * size);
    parts.push_back(stripe);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_ice_crystal(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part main; main.name = "Main_Shard";
    main.mesh = create_crystal_mesh(size * 0.4f);
    main.material.diffuse = vec4(0.7f, 0.85f, 1.0f, 0.7f); main.material.name = "Ice";
    main.position = vec3(0, 0.15f * size, 0);
    parts.push_back(main);
    for (int i = 0; i < 4; i++) {
        Part shard; shard.name = "Shard_" + std::to_string(i);
        float a = (float)i / 4.0f * (float)M_PI * 2 + randf() * 0.4f;
        shard.mesh = create_crystal_mesh(size * (0.12f + randf() * 0.1f));
        shard.material.diffuse = vec4(0.65f + randf() * 0.1f, 0.8f + randf() * 0.1f, 1.0f, 0.6f);
        shard.material.name = "Ice";
        shard.position = vec3(cosf(a) * 0.15f * size, 0.05f * size, sinf(a) * 0.15f * size);
        shard.rotation = vec3(randf() * 20 - 10, rad2deg(a), randf() * 20 - 10);
        parts.push_back(shard);
    }
    Part base; base.name = "Base";
    base.mesh = create_stone(0.15f * size, sd + 8.0f);
    base.material.diffuse = vec4(0.6f, 0.6f, 0.65f, 1); base.material.name = "Stone";
    base.position = vec3(0, -0.1f * size, 0);
    parts.push_back(base);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_mushroom(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part stem; stem.name = "Stem";
    stem.mesh = create_organic_cylinder(0.05f * size, 0.2f * size, 8, sd, 0.15f);
    stem.material.diffuse = vec4(0.9f, 0.85f, 0.75f, 1); stem.material.name = "Flesh";
    stem.position = vec3(0, 0.1f * size, 0);
    parts.push_back(stem);
    Part cap; cap.name = "Cap";
    cap.mesh = create_organic_sphere(0.15f * size, 12, 8, sd + 3.0f, 0.12f);
    cap.material.diffuse = vec4(0.8f, 0.15f, 0.1f, 1); cap.material.name = "Skin";
    cap.scale = vec3(1, 0.5f, 1);
    cap.position = vec3(0, 0.22f * size, 0);
    parts.push_back(cap);
    for (int i = 0; i < 5; i++) {
        Part dot; dot.name = "Dot_" + std::to_string(i);
        float a = (float)i / 5.0f * (float)M_PI * 2 + randf() * 0.5f;
        dot.mesh = create_organic_sphere(0.015f * size, 6, 4, sd + i * 5.0f, 0.3f);
        dot.material.diffuse = vec4(0.95f, 0.95f, 0.9f, 1); dot.material.name = "Spot";
        dot.position = vec3(cosf(a) * 0.1f * size, 0.25f * size, sinf(a) * 0.1f * size);
        parts.push_back(dot);
    }
    Part gill; gill.name = "Gills";
    gill.mesh = create_organic_cylinder(0.12f * size, 0.01f * size, 12, sd + 8.0f, 0.08f);
    gill.material.diffuse = vec4(0.85f, 0.8f, 0.7f, 1); gill.material.name = "Flesh";
    gill.position = vec3(0, 0.18f * size, 0);
    parts.push_back(gill);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_pyramid(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part main; main.name = "Pyramid";
    main.mesh = create_pyramid_mesh(0.5f * size, 0.6f * size);
    main.material.diffuse = vec4(0.85f, 0.75f, 0.5f, 1); main.material.name = "Stone";
    main.position = vec3(0, 0.3f * size, 0);
    deform_organic(main.mesh, 0.008f * size, sd);
    add_random_detail(main.mesh, 0.01f * size, 12);
    parts.push_back(main);
    Part cap; cap.name = "Capstone";
    cap.mesh = create_organic_sphere(0.03f * size, 8, 6, sd + 3.0f, 0.2f);
    cap.material.diffuse = vec4(0.9f, 0.85f, 0.2f, 1); cap.material.name = "Gold";
    cap.position = vec3(0, 0.62f * size, 0);
    parts.push_back(cap);
    Part base; base.name = "Base_Ring";
    base.mesh = create_ring(0.38f * size, 0.35f * size, 20);
    base.material.diffuse = vec4(0.75f, 0.65f, 0.4f, 1); base.material.name = "Stone";
    base.position = vec3(0, 0.01f * size, 0);
    parts.push_back(base);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_donut(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part dough; dough.name = "Dough";
    dough.mesh = create_ring(0.2f * size, 0.08f * size, 24);
    dough.material.diffuse = vec4(0.75f, 0.55f, 0.3f, 1); dough.material.name = "Dough";
    dough.scale = vec3(1, 0.5f, 1);
    deform_organic(dough.mesh, 0.006f * size, sd);
    parts.push_back(dough);
    Part glaze; glaze.name = "Glaze";
    glaze.mesh = create_ring(0.21f * size, 0.085f * size, 24);
    glaze.material.diffuse = vec4(0.9f, 0.4f, 0.6f, 1); glaze.material.name = "Glaze";
    glaze.scale = vec3(1, 0.25f, 1);
    glaze.position = vec3(0, 0.02f * size, 0);
    parts.push_back(glaze);
    for (int i = 0; i < 8; i++) {
        Part sprinkle; sprinkle.name = "Sprinkle_" + std::to_string(i);
        float a = (float)i / 8.0f * (float)M_PI * 2 + randf() * 0.3f;
        float r = 0.14f + randf() * 0.03f;
        sprinkle.mesh = create_cube(1.0f);
        sprinkle.material.diffuse = vec4(randf(), randf(), randf(), 1); sprinkle.material.name = "Candy";
        sprinkle.scale = vec3(0.01f * size, 0.005f * size, 0.025f * size);
        sprinkle.position = vec3(cosf(a) * r * size, 0.035f * size, sinf(a) * r * size);
        sprinkle.rotation = vec3(0, rad2deg(a), randf() * 30);
        parts.push_back(sprinkle);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_satellite(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part body; body.name = "Body";
    body.mesh = create_organic_cylinder(0.12f * size, 0.15f * size, 10, sd, 0.05f);
    body.material.diffuse = vec4(0.7f, 0.72f, 0.75f, 1); body.material.name = "Metal";
    body.position = vec3(0, 0, 0);
    parts.push_back(body);
    Part dish; dish.name = "Dish";
    dish.mesh = create_organic_sphere(0.1f * size, 10, 8, sd + 3.0f, 0.1f);
    dish.material.diffuse = vec4(0.85f, 0.87f, 0.9f, 1); dish.material.name = "Metal";
    dish.scale = vec3(1, 0.3f, 1);
    dish.position = vec3(0, 0.12f * size, 0);
    parts.push_back(dish);
    for (int i = 0; i < 2; i++) {
        Part panel; panel.name = "Solar_Panel_" + std::to_string(i);
        panel.mesh = create_cube(1.0f);
        panel.material.diffuse = vec4(0.1f, 0.15f, 0.4f, 1); panel.material.name = "Solar";
        float x = (i == 0 ? -0.3f : 0.3f) * size;
        panel.scale = vec3(0.25f * size, 0.01f * size, 0.15f * size);
        panel.position = vec3(x, 0, 0);
        parts.push_back(panel);
    }
    Part antenna; antenna.name = "Antenna";
    antenna.mesh = create_organic_cylinder(0.005f * size, 0.15f * size, 4, sd + 6.0f, 0.05f);
    antenna.material.diffuse = vec4(0.6f, 0.6f, 0.65f, 1); antenna.material.name = "Metal";
    antenna.position = vec3(0.05f * size, 0.2f * size, 0);
    antenna.rotation = vec3(0, 0, -15);
    parts.push_back(antenna);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_jewelry(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part band; band.name = "Band";
    band.mesh = create_ring(0.15f * size, 0.12f * size, 20);
    band.material.diffuse = vec4(0.85f, 0.75f, 0.2f, 1); band.material.name = "Gold";
    band.scale = vec3(1, 0.3f, 1);
    deform_organic(band.mesh, 0.002f * size, sd);
    parts.push_back(band);
    Part gem; gem.name = "Gem";
    gem.mesh = create_crystal_mesh(0.04f * size);
    gem.material.diffuse = vec4(0.1f, 0.4f, 0.9f, 0.8f); gem.material.name = "Gemstone";
    gem.position = vec3(0, 0.04f * size, 0);
    parts.push_back(gem);
    Part setting; setting.name = "Setting";
    setting.mesh = create_ring(0.035f * size, 0.025f * size, 8);
    setting.material.diffuse = vec4(0.85f, 0.75f, 0.2f, 1); setting.material.name = "Gold";
    setting.position = vec3(0, 0.04f * size, 0);
    parts.push_back(setting);
    for (int i = 0; i < 6; i++) {
        Part accent; accent.name = "Accent_" + std::to_string(i);
        float a = (float)i / 6.0f * (float)M_PI * 2;
        accent.mesh = create_organic_sphere(0.008f * size, 6, 4, sd + i * 4.0f, 0.2f);
        accent.material.diffuse = vec4(0.9f, 0.2f, 0.3f, 1); accent.material.name = "Ruby";
        accent.position = vec3(cosf(a) * 0.13f * size, 0.01f * size, sinf(a) * 0.13f * size);
        parts.push_back(accent);
    }
    return parts;
}

// ============================================================
// NEW SCENE GENERATORS
// ============================================================

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_throne(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part seat; seat.name = "Seat";
    seat.mesh = create_throne_mesh(size * 0.5f);
    seat.material.diffuse = vec4(0.35f, 0.2f, 0.1f, 1); seat.material.name = "Wood";
    seat.position = vec3(0, 0.25f * size, 0);
    parts.push_back(seat);
    Part cushion; cushion.name = "Cushion";
    cushion.mesh = create_organic_sphere(0.18f * size, 10, 8, sd + 2.0f, 0.1f);
    cushion.material.diffuse = vec4(0.7f, 0.1f, 0.1f, 1); cushion.material.name = "Velvet";
    cushion.scale = vec3(1, 0.3f, 1);
    cushion.position = vec3(0, 0.38f * size, 0.02f * size);
    parts.push_back(cushion);
    Part back; back.name = "Backrest";
    back.mesh = create_cube(1.0f);
    back.material.diffuse = vec4(0.38f, 0.22f, 0.12f, 1); back.material.name = "Wood";
    back.scale = vec3(0.45f * size, 0.55f * size, 0.05f * size);
    back.position = vec3(0, 0.55f * size, -0.18f * size);
    deform_organic(back.mesh, 0.005f * size, sd + 3.0f);
    parts.push_back(back);
    for (int i = 0; i < 2; i++) {
        Part arm; arm.name = "Arm_" + std::to_string(i);
        arm.mesh = create_cube(1.0f);
        arm.material.diffuse = vec4(0.36f, 0.21f, 0.11f, 1); arm.material.name = "Wood";
        float x = (i == 0 ? -0.25f : 0.25f) * size;
        arm.scale = vec3(0.06f * size, 0.1f * size, 0.35f * size);
        arm.position = vec3(x, 0.42f * size, -0.02f * size);
        parts.push_back(arm);
    }
    for (int i = 0; i < 4; i++) {
        Part leg; leg.name = "Leg_" + std::to_string(i);
        leg.mesh = create_organic_cylinder(0.03f * size, 0.25f * size, 6, sd + i * 5.0f, 0.12f);
        leg.material.diffuse = vec4(0.34f, 0.2f, 0.1f, 1); leg.material.name = "Wood";
        float x = (i % 2 == 0 ? -0.2f : 0.2f) * size;
        float z = (i < 2 ? -0.15f : 0.15f) * size;
        leg.position = vec3(x, 0.12f * size, z);
        parts.push_back(leg);
    }
    Part crown; crown.name = "Crown";
    crown.mesh = create_star_mesh(0.06f * size);
    crown.material.diffuse = vec4(0.85f, 0.75f, 0.2f, 1); crown.material.name = "Gold";
    crown.position = vec3(0, 0.85f * size, -0.18f * size);
    parts.push_back(crown);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_altar(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part slab; slab.name = "Slab";
    slab.mesh = create_altar_mesh(size * 0.5f);
    slab.material.diffuse = vec4(0.6f, 0.6f, 0.65f, 1); slab.material.name = "Stone";
    slab.position = vec3(0, 0.15f * size, 0);
    parts.push_back(slab);
    Part pillar1; pillar1.name = "Pillar_1";
    pillar1.mesh = create_tapered_cylinder(0.06f * size, 0.08f * size, 0.3f * size, 10);
    pillar1.material.diffuse = vec4(0.55f, 0.55f, 0.6f, 1); pillar1.material.name = "Stone";
    pillar1.position = vec3(-0.18f * size, 0.15f * size, 0);
    parts.push_back(pillar1);
    Part pillar2; pillar2.name = "Pillar_2";
    pillar2.mesh = create_tapered_cylinder(0.06f * size, 0.08f * size, 0.3f * size, 10);
    pillar2.material.diffuse = vec4(0.57f, 0.57f, 0.62f, 1); pillar2.material.name = "Stone";
    pillar2.position = vec3(0.18f * size, 0.15f * size, 0);
    parts.push_back(pillar2);
    Part bowl; bowl.name = "Bowl";
    bowl.mesh = create_organic_cylinder(0.08f * size, 0.04f * size, 10, sd + 3.0f, 0.08f);
    bowl.material.diffuse = vec4(0.7f, 0.6f, 0.2f, 1); bowl.material.name = "Gold";
    bowl.position = vec3(0, 0.33f * size, 0);
    parts.push_back(bowl);
    Part flame; flame.name = "Flame";
    flame.mesh = create_organic_sphere(0.03f * size, 8, 6, sd + 5.0f, 0.3f);
    flame.material.diffuse = vec4(1.0f, 0.6f, 0.1f, 1); flame.material.name = "Fire";
    flame.scale = vec3(0.8f, 1.5f, 0.8f);
    flame.position = vec3(0, 0.37f * size, 0);
    parts.push_back(flame);
    for (int i = 0; i < 3; i++) {
        Part rune; rune.name = "Rune_" + std::to_string(i);
        float a = (float)i / 3.0f * (float)M_PI * 2;
        rune.mesh = create_cube(1.0f);
        rune.material.diffuse = vec4(0.4f, 0.7f, 0.9f, 1); rune.material.name = "Glow";
        rune.scale = vec3(0.02f * size, 0.03f * size, 0.01f * size);
        rune.position = vec3(cosf(a) * 0.19f * size, 0.2f * size, sinf(a) * 0.19f * size);
        parts.push_back(rune);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_campfire(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    for (int i = 0; i < 6; i++) {
        Part stone; stone.name = "Stone_" + std::to_string(i);
        float a = (float)i / 6.0f * (float)M_PI * 2 + randf() * 0.3f;
        stone.mesh = create_stone((0.04f + randf() * 0.03f) * size, sd + i * 5.0f);
        stone.material.diffuse = vec4(0.45f + randf() * 0.1f, 0.43f + randf() * 0.1f, 0.4f, 1);
        stone.material.name = "Stone";
        stone.position = vec3(cosf(a) * 0.15f * size, 0.02f * size, sinf(a) * 0.15f * size);
        stone.rotation = vec3(randf() * 20, randf() * 360, randf() * 15);
        parts.push_back(stone);
    }
    for (int i = 0; i < 4; i++) {
        Part log; log.name = "Log_" + std::to_string(i);
        float a = (float)i / 4.0f * (float)M_PI * 2 + 0.3f;
        log.mesh = create_log(0.02f * size, 0.2f * size, sd + i * 7.0f);
        log.material.diffuse = vec4(0.4f, 0.25f, 0.1f, 1); log.material.name = "Wood";
        log.position = vec3(cosf(a) * 0.05f * size, 0.04f * size, sinf(a) * 0.05f * size);
        log.rotation = vec3(60 + randf() * 15, rad2deg(a), 0);
        parts.push_back(log);
    }
    Part fire; fire.name = "Fire";
    fire.mesh = create_organic_sphere(0.06f * size, 8, 6, sd + 15.0f, 0.4f);
    fire.material.diffuse = vec4(1.0f, 0.5f, 0.05f, 1); fire.material.name = "Fire";
    fire.scale = vec3(0.7f, 1.2f, 0.7f);
    fire.position = vec3(0, 0.1f * size, 0);
    parts.push_back(fire);
    Part fire2; fire2.name = "Fire_Core";
    fire2.mesh = create_organic_sphere(0.03f * size, 6, 4, sd + 18.0f, 0.5f);
    fire2.material.diffuse = vec4(1.0f, 0.9f, 0.3f, 1); fire2.material.name = "Fire";
    fire2.scale = vec3(0.5f, 1.0f, 0.5f);
    fire2.position = vec3(0, 0.12f * size, 0);
    parts.push_back(fire2);
    Part smoke; smoke.name = "Smoke";
    smoke.mesh = create_cloud(0.08f * size, sd + 20.0f);
    smoke.material.diffuse = vec4(0.4f, 0.4f, 0.42f, 0.4f); smoke.material.name = "Smoke";
    smoke.position = vec3(0, 0.25f * size, 0);
    parts.push_back(smoke);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_crate(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part body; body.name = "Body";
    body.mesh = create_crate_mesh(size * 0.4f);
    body.material.diffuse = vec4(0.55f, 0.38f, 0.18f, 1); body.material.name = "Wood";
    body.position = vec3(0, 0.2f * size, 0);
    parts.push_back(body);
    for (int i = 0; i < 2; i++) {
        Part plank; plank.name = "Plank_" + std::to_string(i);
        plank.mesh = create_cube(1.0f);
        plank.material.diffuse = vec4(0.5f, 0.35f, 0.15f, 1); plank.material.name = "Wood";
        plank.scale = vec3(0.42f * size, 0.02f * size, 0.01f * size);
        float y = (i == 0 ? 0.12f : 0.28f) * size;
        plank.position = vec3(0, y, 0.21f * size);
        parts.push_back(plank);
    }
    Part nail1; nail1.name = "Nail_1";
    nail1.mesh = create_organic_sphere(0.005f * size, 4, 3, sd + 1.0f, 0.2f);
    nail1.material.diffuse = vec4(0.5f, 0.5f, 0.5f, 1); nail1.material.name = "Metal";
    nail1.position = vec3(0.18f * size, 0.12f * size, 0.22f * size);
    parts.push_back(nail1);
    Part nail2; nail2.name = "Nail_2";
    nail2.mesh = create_organic_sphere(0.005f * size, 4, 3, sd + 2.0f, 0.2f);
    nail2.material.diffuse = vec4(0.5f, 0.5f, 0.5f, 1); nail2.material.name = "Metal";
    nail2.position = vec3(-0.18f * size, 0.28f * size, 0.22f * size);
    parts.push_back(nail2);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_barrel(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part body; body.name = "Body";
    body.mesh = create_barrel_mesh(size * 0.4f);
    body.material.diffuse = vec4(0.5f, 0.32f, 0.14f, 1); body.material.name = "Wood";
    body.position = vec3(0, 0.22f * size, 0);
    parts.push_back(body);
    for (int i = 0; i < 3; i++) {
        Part band; band.name = "Band_" + std::to_string(i);
        float y = (0.08f + i * 0.14f) * size;
        band.mesh = create_ring(0.16f * size, 0.15f * size, 16);
        band.material.diffuse = vec4(0.35f, 0.35f, 0.38f, 1); band.material.name = "Metal";
        band.position = vec3(0, y, 0);
        parts.push_back(band);
    }
    Part lid; lid.name = "Lid";
    lid.mesh = create_organic_cylinder(0.15f * size, 0.02f * size, 12, sd + 5.0f, 0.05f);
    lid.material.diffuse = vec4(0.48f, 0.3f, 0.13f, 1); lid.material.name = "Wood";
    lid.position = vec3(0, 0.44f * size, 0);
    parts.push_back(lid);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_bench(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part seat; seat.name = "Seat";
    seat.mesh = create_bench_mesh(size * 0.5f);
    seat.material.diffuse = vec4(0.5f, 0.33f, 0.15f, 1); seat.material.name = "Wood";
    seat.position = vec3(0, 0.22f * size, 0);
    parts.push_back(seat);
    for (int i = 0; i < 2; i++) {
        Part leg; leg.name = "Leg_" + std::to_string(i);
        leg.mesh = create_cube(1.0f);
        leg.material.diffuse = vec4(0.45f, 0.3f, 0.13f, 1); leg.material.name = "Wood";
        float x = (i == 0 ? -0.22f : 0.22f) * size;
        leg.scale = vec3(0.05f * size, 0.22f * size, 0.3f * size);
        leg.position = vec3(x, 0.11f * size, 0);
        parts.push_back(leg);
    }
    Part back; back.name = "Backrest";
    back.mesh = create_cube(1.0f);
    back.material.diffuse = vec4(0.52f, 0.35f, 0.16f, 1); back.material.name = "Wood";
    back.scale = vec3(0.5f * size, 0.18f * size, 0.03f * size);
    back.position = vec3(0, 0.38f * size, -0.15f * size);
    deform_organic(back.mesh, 0.003f * size, sd + 2.0f);
    parts.push_back(back);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_fountain(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part basin; basin.name = "Basin";
    basin.mesh = create_fountain_mesh(size * 0.5f);
    basin.material.diffuse = vec4(0.6f, 0.6f, 0.62f, 1); basin.material.name = "Stone";
    basin.position = vec3(0, 0.08f * size, 0);
    parts.push_back(basin);
    Part pillar; pillar.name = "Pillar";
    pillar.mesh = create_tapered_cylinder(0.04f * size, 0.06f * size, 0.35f * size, 10);
    pillar.material.diffuse = vec4(0.58f, 0.58f, 0.6f, 1); pillar.material.name = "Stone";
    pillar.position = vec3(0, 0.25f * size, 0);
    parts.push_back(pillar);
    Part top_bowl; top_bowl.name = "Top_Bowl";
    top_bowl.mesh = create_organic_cylinder(0.08f * size, 0.03f * size, 10, sd + 3.0f, 0.06f);
    top_bowl.material.diffuse = vec4(0.62f, 0.62f, 0.64f, 1); top_bowl.material.name = "Stone";
    top_bowl.position = vec3(0, 0.44f * size, 0);
    parts.push_back(top_bowl);
    for (int i = 0; i < 4; i++) {
        Part water; water.name = "Water_" + std::to_string(i);
        float a = (float)i / 4.0f * (float)M_PI * 2;
        water.mesh = create_organic_sphere(0.02f * size, 6, 4, sd + i * 4.0f, 0.3f);
        water.material.diffuse = vec4(0.3f, 0.5f, 0.9f, 0.5f); water.material.name = "Water";
        water.scale = vec3(0.5f, 1.5f, 0.5f);
        water.position = vec3(cosf(a) * 0.04f * size, 0.48f * size, sinf(a) * 0.04f * size);
        parts.push_back(water);
    }
    Part splash; splash.name = "Splash";
    splash.mesh = create_cloud(0.06f * size, sd + 10.0f);
    splash.material.diffuse = vec4(0.5f, 0.7f, 1.0f, 0.4f); splash.material.name = "Water";
    splash.position = vec3(0, 0.5f * size, 0);
    parts.push_back(splash);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_cart(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part bed; bed.name = "Bed";
    bed.mesh = create_cart_mesh(size * 0.5f);
    bed.material.diffuse = vec4(0.5f, 0.33f, 0.15f, 1); bed.material.name = "Wood";
    bed.position = vec3(0, 0.2f * size, 0);
    parts.push_back(bed);
    for (int i = 0; i < 2; i++) {
        Part wheel; wheel.name = "Wheel_" + std::to_string(i);
        wheel.mesh = create_organic_cylinder(0.08f * size, 0.03f * size, 12, sd + i * 6.0f, 0.08f);
        wheel.material.diffuse = vec4(0.4f, 0.25f, 0.1f, 1); wheel.material.name = "Wood";
        float x = (i == 0 ? -0.22f : 0.22f) * size;
        wheel.position = vec3(x, 0.08f * size, 0);
        wheel.rotation = vec3(90, 0, 0);
        parts.push_back(wheel);
    }
    Part handle; handle.name = "Handle";
    handle.mesh = create_organic_cylinder(0.015f * size, 0.35f * size, 6, sd + 10.0f, 0.1f);
    handle.material.diffuse = vec4(0.48f, 0.3f, 0.14f, 1); handle.material.name = "Wood";
    handle.position = vec3(0, 0.3f * size, 0.25f * size);
    handle.rotation = vec3(45, 0, 0);
    parts.push_back(handle);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_lantern(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part frame; frame.name = "Frame";
    frame.mesh = create_lantern_mesh(size * 0.4f);
    frame.material.diffuse = vec4(0.35f, 0.35f, 0.38f, 1); frame.material.name = "Metal";
    frame.position = vec3(0, 0.15f * size, 0);
    parts.push_back(frame);
    Part glass; glass.name = "Glass";
    glass.mesh = create_organic_cylinder(0.06f * size, 0.12f * size, 10, sd + 2.0f, 0.05f);
    glass.material.diffuse = vec4(0.9f, 0.85f, 0.5f, 0.4f); glass.material.name = "Glass";
    glass.position = vec3(0, 0.15f * size, 0);
    parts.push_back(glass);
    Part flame; flame.name = "Flame";
    flame.mesh = create_organic_sphere(0.02f * size, 6, 4, sd + 5.0f, 0.4f);
    flame.material.diffuse = vec4(1.0f, 0.7f, 0.2f, 1); flame.material.name = "Fire";
    flame.scale = vec3(0.7f, 1.3f, 0.7f);
    flame.position = vec3(0, 0.15f * size, 0);
    parts.push_back(flame);
    Part hook; hook.name = "Hook";
    hook.mesh = create_ring(0.03f * size, 0.02f * size, 8);
    hook.material.diffuse = vec4(0.3f, 0.3f, 0.33f, 1); hook.material.name = "Metal";
    hook.position = vec3(0, 0.28f * size, 0);
    parts.push_back(hook);
    Part base; base.name = "Base_Plate";
    base.mesh = create_organic_cylinder(0.07f * size, 0.01f * size, 8, sd + 7.0f, 0.05f);
    base.material.diffuse = vec4(0.38f, 0.38f, 0.4f, 1); base.material.name = "Metal";
    base.position = vec3(0, 0.08f * size, 0);
    parts.push_back(base);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_gun(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part body; body.name = "Body";
    body.mesh = create_gun_mesh(size * 0.5f);
    body.material.diffuse = vec4(0.25f, 0.25f, 0.28f, 1); body.material.name = "Metal";
    body.position = vec3(0, 0, 0);
    parts.push_back(body);
    Part grip; grip.name = "Grip";
    grip.mesh = create_cube(1.0f);
    grip.material.diffuse = vec4(0.35f, 0.25f, 0.15f, 1); grip.material.name = "Wood";
    grip.scale = vec3(0.04f * size, 0.1f * size, 0.03f * size);
    grip.position = vec3(0, -0.06f * size, -0.02f * size);
    grip.rotation = vec3(15, 0, 0);
    parts.push_back(grip);
    Part barrel; barrel.name = "Barrel";
    barrel.mesh = create_organic_cylinder(0.01f * size, 0.18f * size, 6, sd + 3.0f, 0.05f);
    barrel.material.diffuse = vec4(0.22f, 0.22f, 0.25f, 1); barrel.material.name = "Metal";
    barrel.position = vec3(0, 0.02f * size, 0.12f * size);
    barrel.rotation = vec3(90, 0, 0);
    parts.push_back(barrel);
    Part trigger; trigger.name = "Trigger";
    trigger.mesh = create_cube(1.0f);
    trigger.material.diffuse = vec4(0.3f, 0.3f, 0.33f, 1); trigger.material.name = "Metal";
    trigger.scale = vec3(0.008f * size, 0.025f * size, 0.015f * size);
    trigger.position = vec3(0, -0.03f * size, 0.01f * size);
    parts.push_back(trigger);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_scifi_turret(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part base; base.name = "Base";
    base.mesh = create_scifi_turret_mesh(size * 0.5f);
    base.material.diffuse = vec4(0.5f, 0.52f, 0.55f, 1); base.material.name = "Metal";
    base.position = vec3(0, 0.1f * size, 0);
    parts.push_back(base);
    Part core; core.name = "Core";
    core.mesh = create_organic_sphere(0.08f * size, 12, 10, sd + 3.0f, 0.1f);
    core.material.diffuse = vec4(0.2f, 0.6f, 0.9f, 1); core.material.name = "Energy";
    core.position = vec3(0, 0.2f * size, 0);
    parts.push_back(core);
    for (int i = 0; i < 2; i++) {
        Part gun; gun.name = "Gun_" + std::to_string(i);
        float x = (i == 0 ? -0.1f : 0.1f) * size;
        gun.mesh = create_organic_cylinder(0.015f * size, 0.2f * size, 8, sd + i * 5.0f, 0.06f);
        gun.material.diffuse = vec4(0.45f, 0.47f, 0.5f, 1); gun.material.name = "Metal";
        gun.position = vec3(x, 0.22f * size, 0.08f * size);
        gun.rotation = vec3(90, 0, 0);
        parts.push_back(gun);
    }
    for (int i = 0; i < 4; i++) {
        Part light; light.name = "Light_" + std::to_string(i);
        float a = (float)i / 4.0f * (float)M_PI * 2;
        light.mesh = create_organic_sphere(0.015f * size, 6, 4, sd + i * 3.0f, 0.2f);
        light.material.diffuse = vec4(0.1f, 0.9f, 0.3f, 1); light.material.name = "Light";
        light.position = vec3(cosf(a) * 0.12f * size, 0.15f * size, sinf(a) * 0.12f * size);
        parts.push_back(light);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_crystal(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part main; main.name = "Main_Crystal";
    main.mesh = create_crystal_mesh(size * 0.4f);
    main.material.diffuse = vec4(0.6f, 0.2f, 0.8f, 0.7f); main.material.name = "Crystal";
    main.position = vec3(0, 0.15f * size, 0);
    parts.push_back(main);
    for (int i = 0; i < 5; i++) {
        Part shard; shard.name = "Shard_" + std::to_string(i);
        float a = (float)i / 5.0f * (float)M_PI * 2 + randf() * 0.4f;
        float h = 0.08f + randf() * 0.1f;
        shard.mesh = create_crystal_mesh(size * (0.08f + randf() * 0.08f));
        float r = 0.3f + randf() * 0.5f;
        float g = 0.1f + randf() * 0.3f;
        float b = 0.5f + randf() * 0.4f;
        shard.material.diffuse = vec4(r, g, b, 0.65f); shard.material.name = "Crystal";
        shard.position = vec3(cosf(a) * 0.12f * size, h * size * 0.5f, sinf(a) * 0.12f * size);
        shard.rotation = vec3(randf() * 30 - 15, rad2deg(a), randf() * 20 - 10);
        parts.push_back(shard);
    }
    Part base; base.name = "Base_Rock";
    base.mesh = create_rock(0.15f * size, sd + 10.0f);
    base.material.diffuse = vec4(0.4f, 0.38f, 0.42f, 1); base.material.name = "Stone";
    base.position = vec3(0, -0.05f * size, 0);
    parts.push_back(base);
    Part glow; glow.name = "Glow";
    glow.mesh = create_organic_sphere(0.2f * size, 10, 8, sd + 12.0f, 0.15f);
    glow.material.diffuse = vec4(0.5f, 0.2f, 0.8f, 0.15f); glow.material.name = "Glow";
    glow.position = vec3(0, 0.1f * size, 0);
    parts.push_back(glow);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_coral(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part main; main.name = "Main_Coral";
    main.mesh = create_coral_mesh(size * 0.4f);
    main.material.diffuse = vec4(0.9f, 0.4f, 0.5f, 1); main.material.name = "Coral";
    main.position = vec3(0, 0.1f * size, 0);
    parts.push_back(main);
    for (int i = 0; i < 3; i++) {
        Part branch; branch.name = "Branch_" + std::to_string(i);
        float a = (float)i / 3.0f * (float)M_PI * 2 + randf() * 0.5f;
        branch.mesh = create_organic_cylinder(0.02f * size, (0.1f + randf() * 0.08f) * size, 6, sd + i * 5.0f, 0.2f);
        float r = 0.8f + randf() * 0.15f;
        float g = 0.3f + randf() * 0.3f;
        branch.material.diffuse = vec4(r, g, 0.5f, 1); branch.material.name = "Coral";
        branch.position = vec3(cosf(a) * 0.08f * size, 0.15f * size, sinf(a) * 0.08f * size);
        branch.rotation = vec3(randf() * 30 - 15, 0, randf() * 30 - 15);
        parts.push_back(branch);
    }
    Part base; base.name = "Base";
    base.mesh = create_stone(0.1f * size, sd + 8.0f);
    base.material.diffuse = vec4(0.5f, 0.5f, 0.55f, 1); base.material.name = "Stone";
    base.position = vec3(0, -0.02f * size, 0);
    parts.push_back(base);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_butterfly(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part body; body.name = "Body";
    body.mesh = create_butterfly_mesh(size * 0.4f);
    body.material.diffuse = vec4(0.3f, 0.25f, 0.2f, 1); body.material.name = "Chitin";
    body.position = vec3(0, 0.1f * size, 0);
    parts.push_back(body);
    for (int i = 0; i < 2; i++) {
        Part wing; wing.name = "Wing_" + std::to_string(i);
        wing.mesh = create_cube(1.0f);
        float x = (i == 0 ? -0.12f : 0.12f) * size;
        wing.material.diffuse = vec4(0.2f + randf() * 0.3f, 0.4f + randf() * 0.3f, 0.8f + randf() * 0.2f, 0.7f);
        wing.material.name = "Wing";
        wing.scale = vec3(0.1f * size, 0.005f * size, 0.15f * size);
        wing.position = vec3(x, 0.12f * size, 0);
        wing.rotation = vec3(10, 0, i == 0 ? 15 : -15);
        deform_organic(wing.mesh, 0.003f * size, sd + i * 5.0f);
        parts.push_back(wing);
    }
    for (int i = 0; i < 2; i++) {
        Part antenna; antenna.name = "Antenna_" + std::to_string(i);
        float x = (i == 0 ? -0.02f : 0.02f) * size;
        antenna.mesh = create_organic_cylinder(0.002f * size, 0.08f * size, 4, sd + i * 3.0f, 0.1f);
        antenna.material.diffuse = vec4(0.3f, 0.25f, 0.2f, 1); antenna.material.name = "Chitin";
        antenna.position = vec3(x, 0.18f * size, 0.03f * size);
        antenna.rotation = vec3(-30, 0, i == 0 ? -15 : 15);
        parts.push_back(antenna);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_cat(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part body; body.name = "Body";
    body.mesh = create_cat_mesh(size * 0.5f);
    body.material.diffuse = vec4(0.7f, 0.55f, 0.3f, 1); body.material.name = "Fur";
    body.position = vec3(0, 0.15f * size, 0);
    parts.push_back(body);
    Part head; head.name = "Head";
    head.mesh = create_organic_sphere(0.1f * size, 12, 10, sd + 2.0f, 0.1f);
    head.material.diffuse = vec4(0.72f, 0.57f, 0.32f, 1); head.material.name = "Fur";
    head.position = vec3(0, 0.28f * size, 0.12f * size);
    parts.push_back(head);
    for (int i = 0; i < 2; i++) {
        Part ear; ear.name = "Ear_" + std::to_string(i);
        float x = (i == 0 ? -0.05f : 0.05f) * size;
        ear.mesh = create_pyramid_mesh(0.03f * size, 0.05f * size, 3);
        ear.material.diffuse = vec4(0.68f, 0.53f, 0.28f, 1); ear.material.name = "Fur";
        ear.position = vec3(x, 0.36f * size, 0.11f * size);
        ear.rotation = vec3(i == 0 ? 10 : -10, 0, i == 0 ? -15 : 15);
        parts.push_back(ear);
    }
    for (int i = 0; i < 4; i++) {
        Part paw; paw.name = "Paw_" + std::to_string(i);
        paw.mesh = create_organic_sphere(0.025f * size, 6, 4, sd + i * 4.0f, 0.15f);
        paw.material.diffuse = vec4(0.7f, 0.55f, 0.3f, 1); paw.material.name = "Fur";
        float x = (i % 2 == 0 ? -0.06f : 0.06f) * size;
        float z = (i < 2 ? 0.08f : -0.08f) * size;
        paw.position = vec3(x, 0.03f * size, z);
        parts.push_back(paw);
    }
    Part tail; tail.name = "Tail";
    tail.mesh = create_organic_cylinder(0.015f * size, 0.18f * size, 6, sd + 10.0f, 0.12f);
    tail.material.diffuse = vec4(0.7f, 0.55f, 0.3f, 1); tail.material.name = "Fur";
    tail.position = vec3(0, 0.18f * size, -0.14f * size);
    tail.rotation = vec3(-45, 0, 10);
    parts.push_back(tail);
    Part eye1; eye1.name = "Eye_1";
    eye1.mesh = create_organic_sphere(0.012f * size, 6, 4, sd + 12.0f, 0.1f);
    eye1.material.diffuse = vec4(0.2f, 0.8f, 0.3f, 1); eye1.material.name = "Eye";
    eye1.position = vec3(-0.03f * size, 0.3f * size, 0.2f * size);
    parts.push_back(eye1);
    Part eye2; eye2.name = "Eye_2";
    eye2.mesh = create_organic_sphere(0.012f * size, 6, 4, sd + 13.0f, 0.1f);
    eye2.material.diffuse = vec4(0.2f, 0.8f, 0.3f, 1); eye2.material.name = "Eye";
    eye2.position = vec3(0.03f * size, 0.3f * size, 0.2f * size);
    parts.push_back(eye2);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_dog(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part body; body.name = "Body";
    body.mesh = create_dog_mesh(size * 0.5f);
    body.material.diffuse = vec4(0.55f, 0.4f, 0.25f, 1); body.material.name = "Fur";
    body.position = vec3(0, 0.18f * size, 0);
    parts.push_back(body);
    Part head; head.name = "Head";
    head.mesh = create_organic_sphere(0.1f * size, 12, 10, sd + 2.0f, 0.12f);
    head.material.diffuse = vec4(0.57f, 0.42f, 0.27f, 1); head.material.name = "Fur";
    head.position = vec3(0, 0.3f * size, 0.15f * size);
    parts.push_back(head);
    Part snout; snout.name = "Snout";
    snout.mesh = create_organic_cylinder(0.03f * size, 0.06f * size, 6, sd + 4.0f, 0.1f);
    snout.material.diffuse = vec4(0.5f, 0.38f, 0.22f, 1); snout.material.name = "Fur";
    snout.position = vec3(0, 0.28f * size, 0.22f * size);
    snout.rotation = vec3(90, 0, 0);
    parts.push_back(snout);
    for (int i = 0; i < 2; i++) {
        Part ear; ear.name = "Ear_" + std::to_string(i);
        float x = (i == 0 ? -0.07f : 0.07f) * size;
        ear.mesh = create_organic_sphere(0.035f * size, 6, 4, sd + i * 5.0f, 0.15f);
        ear.material.diffuse = vec4(0.45f, 0.33f, 0.2f, 1); ear.material.name = "Fur";
        ear.scale = vec3(0.6f, 1.0f, 0.4f);
        ear.position = vec3(x, 0.32f * size, 0.1f * size);
        parts.push_back(ear);
    }
    for (int i = 0; i < 4; i++) {
        Part paw; paw.name = "Paw_" + std::to_string(i);
        paw.mesh = create_organic_sphere(0.03f * size, 6, 4, sd + i * 3.0f, 0.12f);
        paw.material.diffuse = vec4(0.55f, 0.4f, 0.25f, 1); paw.material.name = "Fur";
        float x = (i % 2 == 0 ? -0.07f : 0.07f) * size;
        float z = (i < 2 ? 0.1f : -0.1f) * size;
        paw.position = vec3(x, 0.03f * size, z);
        parts.push_back(paw);
    }
    Part tail; tail.name = "Tail";
    tail.mesh = create_organic_cylinder(0.015f * size, 0.15f * size, 6, sd + 10.0f, 0.1f);
    tail.material.diffuse = vec4(0.55f, 0.4f, 0.25f, 1); tail.material.name = "Fur";
    tail.position = vec3(0, 0.25f * size, -0.15f * size);
    tail.rotation = vec3(-60, 0, 20);
    parts.push_back(tail);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_bird(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part body; body.name = "Body";
    body.mesh = create_bird_mesh(size * 0.4f);
    body.material.diffuse = vec4(0.2f, 0.5f, 0.8f, 1); body.material.name = "Feathers";
    body.position = vec3(0, 0.12f * size, 0);
    parts.push_back(body);
    Part head; head.name = "Head";
    head.mesh = create_organic_sphere(0.05f * size, 8, 6, sd + 2.0f, 0.1f);
    head.material.diffuse = vec4(0.15f, 0.45f, 0.75f, 1); head.material.name = "Feathers";
    head.position = vec3(0, 0.2f * size, 0.08f * size);
    parts.push_back(head);
    Part beak; beak.name = "Beak";
    beak.mesh = create_pyramid_mesh(0.015f * size, 0.04f * size, 3);
    beak.material.diffuse = vec4(0.9f, 0.6f, 0.1f, 1); beak.material.name = "Keratin";
    beak.position = vec3(0, 0.19f * size, 0.13f * size);
    beak.rotation = vec3(90, 0, 0);
    parts.push_back(beak);
    for (int i = 0; i < 2; i++) {
        Part wing; wing.name = "Wing_" + std::to_string(i);
        float x = (i == 0 ? -0.1f : 0.1f) * size;
        wing.mesh = create_cube(1.0f);
        wing.material.diffuse = vec4(0.18f, 0.48f, 0.78f, 1); wing.material.name = "Feathers";
        wing.scale = vec3(0.08f * size, 0.01f * size, 0.12f * size);
        wing.position = vec3(x, 0.13f * size, -0.02f * size);
        wing.rotation = vec3(0, 0, i == 0 ? 20 : -20);
        deform_organic(wing.mesh, 0.002f * size, sd + i * 5.0f);
        parts.push_back(wing);
    }
    Part eye; eye.name = "Eye";
    eye.mesh = create_organic_sphere(0.008f * size, 4, 3, sd + 6.0f, 0.1f);
    eye.material.diffuse = vec4(0.1f, 0.1f, 0.1f, 1); eye.material.name = "Eye";
    eye.position = vec3(0.03f * size, 0.21f * size, 0.12f * size);
    parts.push_back(eye);
    for (int i = 0; i < 2; i++) {
        Part leg; leg.name = "Leg_" + std::to_string(i);
        float x = (i == 0 ? -0.03f : 0.03f) * size;
        leg.mesh = create_organic_cylinder(0.005f * size, 0.08f * size, 4, sd + i * 3.0f, 0.08f);
        leg.material.diffuse = vec4(0.8f, 0.5f, 0.1f, 1); leg.material.name = "Keratin";
        leg.position = vec3(x, 0.04f * size, 0);
        parts.push_back(leg);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_tree_stump(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part main; main.name = "Stump";
    main.mesh = create_tree_stump_mesh(size * 0.4f);
    main.material.diffuse = vec4(0.4f, 0.28f, 0.12f, 1); main.material.name = "Bark";
    main.position = vec3(0, 0.1f * size, 0);
    parts.push_back(main);
    Part top; top.name = "Rings";
    top.mesh = create_organic_cylinder(0.12f * size, 0.01f * size, 12, sd + 3.0f, 0.04f);
    top.material.diffuse = vec4(0.55f, 0.4f, 0.22f, 1); top.material.name = "Wood";
    top.position = vec3(0, 0.21f * size, 0);
    parts.push_back(top);
    for (int i = 0; i < 3; i++) {
        Part moss; moss.name = "Moss_" + std::to_string(i);
        float a = (float)i / 3.0f * (float)M_PI * 2 + randf() * 0.5f;
        moss.mesh = create_leaf_cluster(0.03f * size, sd + i * 5.0f);
        moss.material.diffuse = vec4(0.15f, 0.45f, 0.1f, 1); moss.material.name = "Moss";
        moss.position = vec3(cosf(a) * 0.11f * size, 0.08f * size + randf() * 0.08f * size, sinf(a) * 0.11f * size);
        parts.push_back(moss);
    }
    Part root; root.name = "Root";
    root.mesh = create_log(0.02f * size, 0.12f * size, sd + 8.0f);
    root.material.diffuse = vec4(0.38f, 0.26f, 0.1f, 1); root.material.name = "Root";
    root.position = vec3(0.1f * size, 0.02f * size, 0.05f * size);
    root.rotation = vec3(10, 30, -15);
    parts.push_back(root);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_rock_wall(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part main; main.name = "Wall";
    main.mesh = create_rock_wall_mesh(size * 0.5f);
    main.material.diffuse = vec4(0.5f, 0.48f, 0.52f, 1); main.material.name = "Stone";
    main.position = vec3(0, 0.3f * size, 0);
    parts.push_back(main);
    for (int i = 0; i < 8; i++) {
        Part rock; rock.name = "Rock_" + std::to_string(i);
        rock.mesh = create_stone((0.04f + randf() * 0.04f) * size, sd + i * 7.0f);
        rock.material.diffuse = vec4(0.45f + randf() * 0.1f, 0.43f + randf() * 0.1f, 0.48f + randf() * 0.05f, 1);
        rock.material.name = "Stone";
        float x = (randf() - 0.5f) * 0.6f * size;
        float y = randf() * 0.5f * size;
        float z = (randf() - 0.5f) * 0.1f * size;
        rock.position = vec3(x, y, z);
        rock.rotation = vec3(randf() * 30, randf() * 360, randf() * 20);
        parts.push_back(rock);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_grave(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part stone; stone.name = "Headstone";
    stone.mesh = create_grave_mesh(size * 0.4f);
    stone.material.diffuse = vec4(0.55f, 0.55f, 0.58f, 1); stone.material.name = "Stone";
    stone.position = vec3(0, 0.2f * size, 0);
    parts.push_back(stone);
    Part mound; mound.name = "Mound";
    mound.mesh = create_organic_sphere(0.15f * size, 10, 6, sd + 3.0f, 0.2f);
    mound.material.diffuse = vec4(0.35f, 0.3f, 0.2f, 1); mound.material.name = "Dirt";
    mound.scale = vec3(1.2f, 0.4f, 0.8f);
    mound.position = vec3(0, 0.02f * size, 0.2f * size);
    parts.push_back(mound);
    Part cross; cross.name = "Cross";
    cross.mesh = create_cube(1.0f);
    cross.material.diffuse = vec4(0.5f, 0.5f, 0.53f, 1); cross.material.name = "Stone";
    cross.scale = vec3(0.02f * size, 0.08f * size, 0.02f * size);
    cross.position = vec3(0, 0.42f * size, 0);
    parts.push_back(cross);
    Part crossbar; crossbar.name = "Crossbar";
    crossbar.mesh = create_cube(1.0f);
    crossbar.material.diffuse = vec4(0.5f, 0.5f, 0.53f, 1); crossbar.material.name = "Stone";
    crossbar.scale = vec3(0.06f * size, 0.02f * size, 0.02f * size);
    crossbar.position = vec3(0, 0.44f * size, 0);
    parts.push_back(crossbar);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_flag(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part pole; pole.name = "Pole";
    pole.mesh = create_organic_cylinder(0.015f * size, 0.6f * size, 8, sd, 0.06f);
    pole.material.diffuse = vec4(0.5f, 0.35f, 0.15f, 1); pole.material.name = "Wood";
    pole.position = vec3(0, 0.3f * size, 0);
    parts.push_back(pole);
    Part cloth; cloth.name = "Flag_Cloth";
    cloth.mesh = create_flag_mesh(size * 0.4f);
    cloth.material.diffuse = vec4(0.8f, 0.15f, 0.1f, 1); cloth.material.name = "Fabric";
    cloth.position = vec3(0.12f * size, 0.48f * size, 0);
    parts.push_back(cloth);
    Part finial; finial.name = "Finial";
    finial.mesh = create_organic_sphere(0.02f * size, 6, 4, sd + 3.0f, 0.15f);
    finial.material.diffuse = vec4(0.8f, 0.7f, 0.2f, 1); finial.material.name = "Gold";
    finial.position = vec3(0, 0.62f * size, 0);
    parts.push_back(finial);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_bookshelf(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part frame; frame.name = "Frame";
    frame.mesh = create_bookshelf_mesh(size * 0.5f);
    frame.material.diffuse = vec4(0.48f, 0.3f, 0.14f, 1); frame.material.name = "Wood";
    frame.position = vec3(0, 0.4f * size, 0);
    parts.push_back(frame);
    for (int i = 0; i < 12; i++) {
        Part book; book.name = "Book_" + std::to_string(i);
        book.mesh = create_cube(1.0f);
        float r = 0.15f + randf() * 0.6f;
        float g = 0.1f + randf() * 0.4f;
        float b = 0.1f + randf() * 0.6f;
        book.material.diffuse = vec4(r, g, b, 1); book.material.name = "Cover";
        int shelf = i / 4;
        int pos = i % 4;
        book.scale = vec3((0.03f + randf() * 0.02f) * size, (0.14f + randf() * 0.06f) * size, 0.1f * size);
        book.position = vec3((-0.12f + pos * 0.08f + randf() * 0.02f) * size, (0.04f + shelf * 0.35f) * size, 0.02f * size);
        book.rotation = vec3(0, 0, (randf() - 0.5f) * 5);
        parts.push_back(book);
    }
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_potion(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part flask; flask.name = "Flask";
    flask.mesh = create_potion_mesh(size * 0.4f);
    flask.material.diffuse = vec4(0.7f, 0.85f, 0.9f, 0.5f); flask.material.name = "Glass";
    flask.position = vec3(0, 0.12f * size, 0);
    parts.push_back(flask);
    Part liquid; liquid.name = "Liquid";
    liquid.mesh = create_organic_cylinder(0.06f * size, 0.08f * size, 10, sd + 2.0f, 0.08f);
    liquid.material.diffuse = vec4(0.2f, 0.8f, 0.4f, 0.7f); liquid.material.name = "Potion";
    liquid.position = vec3(0, 0.08f * size, 0);
    parts.push_back(liquid);
    Part cork; cork.name = "Cork";
    cork.mesh = create_organic_cylinder(0.025f * size, 0.03f * size, 6, sd + 4.0f, 0.1f);
    cork.material.diffuse = vec4(0.6f, 0.45f, 0.2f, 1); cork.material.name = "Cork";
    cork.position = vec3(0, 0.19f * size, 0);
    parts.push_back(cork);
    Part bubble; bubble.name = "Bubble";
    bubble.mesh = create_organic_sphere(0.008f * size, 4, 3, sd + 6.0f, 0.2f);
    bubble.material.diffuse = vec4(0.3f, 0.9f, 0.5f, 0.4f); bubble.material.name = "Bubble";
    bubble.position = vec3(0.02f * size, 0.1f * size, 0.01f * size);
    parts.push_back(bubble);
    Part glow; glow.name = "Glow";
    glow.mesh = create_organic_sphere(0.1f * size, 8, 6, sd + 8.0f, 0.15f);
    glow.material.diffuse = vec4(0.2f, 0.8f, 0.4f, 0.1f); glow.material.name = "Glow";
    glow.position = vec3(0, 0.08f * size, 0);
    parts.push_back(glow);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_cave(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part entrance; entrance.name = "Entrance";
    entrance.mesh = create_cave_entrance(size * 0.5f);
    entrance.material.diffuse = vec4(0.35f, 0.33f, 0.38f, 1); entrance.material.name = "Stone";
    entrance.position = vec3(0, 0.2f * size, 0);
    parts.push_back(entrance);
    for (int i = 0; i < 5; i++) {
        Part stalactite; stalactite.name = "Stalactite_" + std::to_string(i);
        float x = (randf() - 0.5f) * 0.4f * size;
        float z = (randf() - 0.5f) * 0.2f * size;
        stalactite.mesh = create_tapered_cylinder(0.005f * size, 0.02f * size, (0.05f + randf() * 0.1f) * size, 6);
        stalactite.material.diffuse = vec4(0.4f + randf() * 0.1f, 0.38f + randf() * 0.1f, 0.42f, 1);
        stalactite.material.name = "Stone";
        stalactite.position = vec3(x, 0.38f * size, z);
        stalactite.rotation = vec3(180, 0, randf() * 20 - 10);
        parts.push_back(stalactite);
    }
    for (int i = 0; i < 4; i++) {
        Part rock; rock.name = "Rock_" + std::to_string(i);
        rock.mesh = create_stone((0.05f + randf() * 0.05f) * size, sd + i * 7.0f);
        rock.material.diffuse = vec4(0.38f + randf() * 0.08f, 0.36f + randf() * 0.08f, 0.4f, 1);
        rock.material.name = "Stone";
        rock.position = vec3((randf() - 0.5f) * 0.3f * size, 0.03f * size, (randf() - 0.5f) * 0.15f * size);
        rock.rotation = vec3(randf() * 20, randf() * 360, randf() * 15);
        parts.push_back(rock);
    }
    Part crystal; crystal.name = "Cave_Crystal";
    crystal.mesh = create_crystal_mesh(0.06f * size);
    crystal.material.diffuse = vec4(0.4f, 0.6f, 0.9f, 0.6f); crystal.material.name = "Crystal";
    crystal.position = vec3(0.12f * size, 0.1f * size, 0.05f * size);
    crystal.rotation = vec3(0, 0, -20);
    parts.push_back(crystal);
    return parts;
}

std::vector<ProceduralEngine::Part> ProceduralEngine::generate_tent(float size) {
    std::vector<Part> parts;
    float sd = randf() * 100.0f;
    Part body; body.name = "Tent";
    body.mesh = create_tent_mesh(size * 0.5f);
    body.material.diffuse = vec4(0.6f, 0.45f, 0.2f, 1); body.material.name = "Canvas";
    body.position = vec3(0, 0.18f * size, 0);
    parts.push_back(body);
    Part pole; pole.name = "Pole";
    pole.mesh = create_organic_cylinder(0.015f * size, 0.4f * size, 6, sd + 2.0f, 0.06f);
    pole.material.diffuse = vec4(0.5f, 0.35f, 0.15f, 1); pole.material.name = "Wood";
    pole.position = vec3(0, 0.2f * size, 0);
    parts.push_back(pole);
    Part floor; floor.name = "Floor";
    floor.mesh = create_organic_cylinder(0.2f * size, 0.01f * size, 10, sd + 4.0f, 0.05f);
    floor.material.diffuse = vec4(0.45f, 0.4f, 0.35f, 1); floor.material.name = "Fabric";
    floor.position = vec3(0, 0.01f * size, 0);
    parts.push_back(floor);
    Part rope1; rope1.name = "Rope_1";
    rope1.mesh = create_organic_cylinder(0.005f * size, 0.3f * size, 4, sd + 6.0f, 0.05f);
    rope1.material.diffuse = vec4(0.6f, 0.5f, 0.3f, 1); rope1.material.name = "Rope";
    rope1.position = vec3(0.2f * size, 0.1f * size, 0.15f * size);
    rope1.rotation = vec3(30, 0, -20);
    parts.push_back(rope1);
    return parts;
}

// ============================================================
// OBJECTGENERATOR CLASS
// ============================================================

ObjectGenerator::ObjectGenerator() {}

std::string ObjectGenerator::get_last_description() const {
    return last_description;
}

Material ObjectGenerator::create_material(const vec4& color, const std::string& type) {
    Material m;
    m.diffuse = color;
    m.specular = vec4(0.3f, 0.3f, 0.3f, 1);
    m.shininess = 32.0f;
    m.name = type;
    if (type == "Metal") { m.specular = vec4(0.6f, 0.6f, 0.6f, 1); m.shininess = 64.0f; }
    else if (type == "Glass") { m.specular = vec4(0.8f, 0.8f, 0.8f, 1); m.shininess = 128.0f; }
    else if (type == "Wood") { m.specular = vec4(0.1f, 0.1f, 0.1f, 1); m.shininess = 16.0f; }
    else if (type == "Stone") { m.specular = vec4(0.15f, 0.15f, 0.15f, 1); m.shininess = 20.0f; }
    else if (type == "Fabric") { m.specular = vec4(0.05f, 0.05f, 0.05f, 1); m.shininess = 8.0f; }
    else if (type == "Flesh") { m.specular = vec4(0.2f, 0.2f, 0.2f, 1); m.shininess = 24.0f; }
    else if (type == "Gold") { m.specular = vec4(0.9f, 0.8f, 0.3f, 1); m.shininess = 96.0f; }
    else if (type == "Fire") { m.specular = vec4(1.0f, 0.8f, 0.2f, 1); m.shininess = 1.0f; }
    else if (type == "Water") { m.specular = vec4(0.5f, 0.5f, 0.5f, 1); m.shininess = 80.0f; }
    else if (type == "Energy") { m.specular = vec4(0.8f, 0.8f, 1.0f, 1); m.shininess = 200.0f; }
    return m;
}

void ObjectGenerator::add_parts_to_scene(Scene* scene, const std::vector<ProceduralEngine::Part>& parts) {
    for (const auto& p : parts) {
        auto node = std::make_unique<SceneNode>();
        node->name = p.name;
        node->mesh = std::make_unique<Mesh>(p.mesh);
        node->material = std::make_unique<Material>(p.material);
        node->position = p.position;
        node->rotation = p.rotation;
        node->scale = p.scale;
        scene->root.add_child(std::move(node));
    }
}

void ObjectGenerator::apply_arrangement(std::vector<SceneNode*>& nodes, const ParsedPrompt& p, Scene* scene) {
    if (p.arrangement == "circle" && nodes.size() > 1) {
        float radius = 1.5f * p.size;
        for (size_t i = 0; i < nodes.size(); i++) {
            float a = (float)i / (float)nodes.size() * (float)M_PI * 2;
            vec3 pos = nodes[i]->position;
            pos.x += cosf(a) * radius;
            pos.z += sinf(a) * radius;
            nodes[i]->position = pos;
        }
    } else if (p.arrangement == "grid" && nodes.size() > 1) {
        int cols = (int)ceilf(sqrtf((float)nodes.size()));
        float spacing = 1.2f * p.size;
        for (size_t i = 0; i < nodes.size(); i++) {
            int row = (int)i / cols;
            int col = (int)i % cols;
            vec3 pos = nodes[i]->position;
            pos.x += (col - cols / 2) * spacing;
            pos.z += (row - (int)nodes.size() / cols / 2) * spacing;
            nodes[i]->position = pos;
        }
    } else if (p.arrangement == "line" && nodes.size() > 1) {
        float spacing = 1.5f * p.size;
        for (size_t i = 0; i < nodes.size(); i++) {
            vec3 pos = nodes[i]->position;
            pos.x += ((float)i - (float)nodes.size() / 2.0f) * spacing;
            nodes[i]->position = pos;
        }
    }
}

Scene* ObjectGenerator::generate_from_prompt(const std::string& prompt, Scene* scene) {
    if (!scene) scene = new Scene();
    ParsedPrompt pp = prompt_engine.parse(prompt);
    last_description = prompt_engine.get_description(pp);

    proc_engine.rng() = std::mt19937(pp.seed);

    std::vector<ProceduralEngine::Part> all_parts;

    if (pp.scene_type == "tree") all_parts = proc_engine.generate_tree(pp.size * 3.0f);
    else if (pp.scene_type == "house") all_parts = proc_engine.generate_house(pp.size);
    else if (pp.scene_type == "car") all_parts = proc_engine.generate_car(pp.size);
    else if (pp.scene_type == "robot") all_parts = proc_engine.generate_robot(pp.size);
    else if (pp.scene_type == "castle") all_parts = proc_engine.generate_castle(pp.size);
    else if (pp.scene_type == "spaceship") all_parts = proc_engine.generate_spaceship(pp.size);
    else if (pp.scene_type == "sword") all_parts = proc_engine.generate_sword(pp.size);
    else if (pp.scene_type == "chair") all_parts = proc_engine.generate_chair(pp.size);
    else if (pp.scene_type == "table") all_parts = proc_engine.generate_table(pp.size);
    else if (pp.scene_type == "star") all_parts = proc_engine.generate_star(pp.size);
    else if (pp.scene_type == "heart") all_parts = proc_engine.generate_heart(pp.size);
    else if (pp.scene_type == "mountain") all_parts = proc_engine.generate_mountain(pp.size * 2.0f);
    else if (pp.scene_type == "snowman") all_parts = proc_engine.generate_snowman(pp.size);
    else if (pp.scene_type == "diamond") all_parts = proc_engine.generate_diamond(pp.size);
    else if (pp.scene_type == "spiral") all_parts = proc_engine.generate_spiral(pp.size);
    else if (pp.scene_type == "dna") all_parts = proc_engine.generate_dna(pp.size);
    else if (pp.scene_type == "skull") all_parts = proc_engine.generate_skull(pp.size);
    else if (pp.scene_type == "airplane") all_parts = proc_engine.generate_airplane(pp.size);
    else if (pp.scene_type == "boat") all_parts = proc_engine.generate_boat(pp.size);
    else if (pp.scene_type == "door") all_parts = proc_engine.generate_door(pp.size);
    else if (pp.scene_type == "window") all_parts = proc_engine.generate_window(pp.size);
    else if (pp.scene_type == "shelf") all_parts = proc_engine.generate_shelf(pp.size);
    else if (pp.scene_type == "lamp") all_parts = proc_engine.generate_lamp(pp.size);
    else if (pp.scene_type == "brain") all_parts = proc_engine.generate_brain(pp.size);
    else if (pp.scene_type == "flower") all_parts = proc_engine.generate_flower(pp.size);
    else if (pp.scene_type == "bone") all_parts = proc_engine.generate_bone(pp.size);
    else if (pp.scene_type == "key") all_parts = proc_engine.generate_key(pp.size);
    else if (pp.scene_type == "helmet") all_parts = proc_engine.generate_helmet(pp.size);
    else if (pp.scene_type == "battery") all_parts = proc_engine.generate_battery(pp.size);
    else if (pp.scene_type == "ice_crystal") all_parts = proc_engine.generate_ice_crystal(pp.size);
    else if (pp.scene_type == "mushroom") all_parts = proc_engine.generate_mushroom(pp.size);
    else if (pp.scene_type == "pyramid") all_parts = proc_engine.generate_pyramid(pp.size);
    else if (pp.scene_type == "donut") all_parts = proc_engine.generate_donut(pp.size);
    else if (pp.scene_type == "satellite") all_parts = proc_engine.generate_satellite(pp.size);
    else if (pp.scene_type == "jewelry") all_parts = proc_engine.generate_jewelry(pp.size);
    else if (pp.scene_type == "throne") all_parts = proc_engine.generate_throne(pp.size);
    else if (pp.scene_type == "altar") all_parts = proc_engine.generate_altar(pp.size);
    else if (pp.scene_type == "campfire") all_parts = proc_engine.generate_campfire(pp.size);
    else if (pp.scene_type == "crate") all_parts = proc_engine.generate_crate(pp.size);
    else if (pp.scene_type == "barrel") all_parts = proc_engine.generate_barrel(pp.size);
    else if (pp.scene_type == "bench") all_parts = proc_engine.generate_bench(pp.size);
    else if (pp.scene_type == "fountain") all_parts = proc_engine.generate_fountain(pp.size);
    else if (pp.scene_type == "cart") all_parts = proc_engine.generate_cart(pp.size);
    else if (pp.scene_type == "lantern") all_parts = proc_engine.generate_lantern(pp.size);
    else if (pp.scene_type == "gun") all_parts = proc_engine.generate_gun(pp.size);
    else if (pp.scene_type == "pistol") all_parts = proc_engine.generate_gun(pp.size);
    else if (pp.scene_type == "turret") all_parts = proc_engine.generate_scifi_turret(pp.size);
    else if (pp.scene_type == "crystal") all_parts = proc_engine.generate_crystal(pp.size);
    else if (pp.scene_type == "coral") all_parts = proc_engine.generate_coral(pp.size);
    else if (pp.scene_type == "butterfly") all_parts = proc_engine.generate_butterfly(pp.size);
    else if (pp.scene_type == "cat") all_parts = proc_engine.generate_cat(pp.size);
    else if (pp.scene_type == "dog") all_parts = proc_engine.generate_dog(pp.size);
    else if (pp.scene_type == "bird") all_parts = proc_engine.generate_bird(pp.size);
    else if (pp.scene_type == "stump") all_parts = proc_engine.generate_tree_stump(pp.size);
    else if (pp.scene_type == "wall") all_parts = proc_engine.generate_rock_wall(pp.size);
    else if (pp.scene_type == "grave") all_parts = proc_engine.generate_grave(pp.size);
    else if (pp.scene_type == "flag") all_parts = proc_engine.generate_flag(pp.size);
    else if (pp.scene_type == "bookshelf") all_parts = proc_engine.generate_bookshelf(pp.size);
    else if (pp.scene_type == "potion") all_parts = proc_engine.generate_potion(pp.size);
    else if (pp.scene_type == "cave") all_parts = proc_engine.generate_cave(pp.size);
    else if (pp.scene_type == "tent") all_parts = proc_engine.generate_tent(pp.size);
    else {
        all_parts = proc_engine.generate_tree(pp.size * 3.0f);
    }

    add_parts_to_scene(scene, all_parts);
    return scene;
}

} // namespace ocp
