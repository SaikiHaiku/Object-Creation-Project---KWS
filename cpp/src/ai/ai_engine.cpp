#include "ai_engine.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cctype>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ocp {

// ============================================================
// UTILITIES
// ============================================================

static std::string to_lower(const std::string& s) {
    std::string r = s; std::transform(r.begin(), r.end(), r.begin(), ::tolower); return r;
}

static bool contains_word(const std::string& text, const std::string& word) {
    std::string lt = to_lower(text);
    std::string lw = to_lower(word);
    size_t pos = 0;
    while ((pos = lt.find(lw, pos)) != std::string::npos) {
        bool left_ok = (pos == 0) || !std::isalnum((unsigned char)lt[pos - 1]);
        bool right_ok = (pos + lw.size() >= lt.size()) || !std::isalnum((unsigned char)lt[pos + lw.size()]);
        if (left_ok && right_ok) return true;
        pos += lw.size();
    }
    return false;
}

// ============================================================
// PROMPT ENGINE
// ============================================================

ParsedPrompt PromptEngine::parse(const std::string& prompt) {
    ParsedPrompt p; p.original = prompt;
    { std::random_device rd; p.seed = rd(); }
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
    else if (contains_word(lp, "metal metallique metallic")) p.material_type = "metallic";
    else if (contains_word(lp, "glossy") || contains_word(lp, "brillant") || contains_word(lp, "shiny") || contains_word(lp, "brillance")) p.material_type = "glossy";
    else if (contains_word(lp, "matte") || contains_word(lp, "mat") || contains_word(lp, "plat") || contains_word(lp, "diffus")) p.material_type = "matte";
    else if (contains_word(lp, "glass") || contains_word(lp, "verre")) p.material_type = "glass";
    else if (contains_word(lp, "emissive") || contains_word(lp, "lumineux") || contains_word(lp, "glow") || contains_word(lp, "lumiere")) p.material_type = "emissive";
    else if (contains_word(lp, "wireframe") || contains_word(lp, "fil")) p.material_type = "wireframe";
    else if (contains_word(lp, "bois") || contains_word(lp, "wood") || contains_word(lp, "en bois")) p.material_type = "wood";
    else if (contains_word(lp, "pierre") || contains_word(lp, "stone") || contains_word(lp, "roche")) p.material_type = "stone";
    else if (contains_word(lp, "plastique") || contains_word(lp, "plastic")) p.material_type = "plastic";
    else if (contains_word(lp, "tissu") || contains_word(lp, "fabric") || contains_word(lp, "toile")) p.material_type = "fabric";

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

    if (contains_word(lp, "fantaisie") || contains_word(lp, "fantasy") || contains_word(lp, "magique")) p.style = "fantasy";
    else if (contains_word(lp, "sombre") || contains_word(lp, "dark")) p.style = "dark";
    else if (contains_word(lp, "lumineux") || contains_word(lp, "bright")) p.style = "bright";
    else if (contains_word(lp, "realiste") || contains_word(lp, "realistic")) p.style = "realistic";
    else if (contains_word(lp, "cartoon") || contains_word(lp, "cartoonesque")) p.style = "cartoon";
    else if (contains_word(lp, "futuriste") || contains_word(lp, "futuristic")) p.style = "futuristic";
    else if (contains_word(lp, "ancien") || contains_word(lp, "ancient") || contains_word(lp, "vieux")) p.style = "ancient";
    else if (contains_word(lp, "beau") || contains_word(lp, "beautiful") || contains_word(lp, "joli") || contains_word(lp, "pretty")) p.style = "beautiful";

    if (p.shapes.empty() && p.scene_type.empty()) p.shapes.push_back("cube");
    return p;
}

std::string PromptEngine::get_description(const ParsedPrompt& p) {
    std::string d;
    if (!p.scene_type.empty()) {
        d = "Procedurally modeled a detailed " + p.scene_type + " from parametric volumes with organic noise deformation";
    } else {
        d = "Created object(s): ";
        for (auto& s : p.shapes) d += s + " ";
    }
    d += " (size: " + std::to_string(p.size) + ")";
    if (p.count > 1) d += " x" + std::to_string(p.count);
    if (!p.material_type.empty()) d += " [" + p.material_type + "]";
    return d;
}

// ============================================================
// PARAMETRIC BUILDER — builds every volume vertex-by-vertex
// ============================================================

ParametricBuilder::ParametricBuilder() {}

float ParametricBuilder::vol_noise(float x, float y, float z, float seed) {
    return fbm3d(x + seed, y + seed * 0.7f, z + seed * 1.3f, 5);
}

// ---- SPHERE ----
Mesh ParametricBuilder::build_sphere(const VolumeDescription& vol) {
    Mesh m; m.name = vol.name.empty() ? "ParamSphere" : vol.name;
    int seg = vol.segments;
    int ring = vol.rings;
    float seed = vol.seed_offset;
    float na = vol.noise_amount;
    float ns = vol.noise_scale;
    float rad = vol.params.sphere.radius;

    for (int r = 0; r <= ring; r++) {
        float phi = (float)M_PI * r / ring;
        for (int s = 0; s <= seg; s++) {
            float theta = 2.0f * (float)M_PI * s / seg;
            float nx = cosf(theta) * sinf(phi);
            float ny = cosf(phi);
            float nz = sinf(theta) * sinf(phi);
            float n = vol_noise(nx * ns, ny * ns, nz * ns, seed);
            float ridge = 1.0f - std::abs(vol_noise(nx * ns * 1.5f + 50.0f, ny * ns * 1.5f, nz * ns * 1.5f, seed + 10.0f));
            ridge *= ridge * 0.3f;
            float r2 = rad * (1.0f + (n + ridge) * na);
            vec3 pos(r2 * nx, r2 * ny, r2 * nz);
            vec2 uv((float)s / seg, (float)r / ring);
            m.add_vertex(pos, glm::normalize(pos), uv);
        }
    }
    for (int r = 0; r < ring; r++) {
        for (int s = 0; s < seg; s++) {
            uint32_t i0 = r * (seg + 1) + s;
            m.add_face({i0, i0 + (uint32_t)(seg + 1), i0 + (uint32_t)(seg + 2), i0 + 1});
        }
    }
    m.compute_normals();
    return m;
}

// ---- BOX ----
Mesh ParametricBuilder::build_box(const VolumeDescription& vol) {
    Mesh m; m.name = vol.name.empty() ? "ParamBox" : vol.name;
    float seed = vol.seed_offset;
    float na = vol.noise_amount;
    float ns = vol.noise_scale;
    vec3 s = vol.scale;

    struct FaceDef { vec3 normal; vec3 up; vec3 right; };
    FaceDef faces[6] = {
        { vec3(0,0,1),  vec3(0,1,0),  vec3(1,0,0) },   // front
        { vec3(0,0,-1), vec3(0,1,0),  vec3(-1,0,0) },  // back
        { vec3(1,0,0),  vec3(0,1,0),  vec3(0,0,-1) },  // right
        { vec3(-1,0,0), vec3(0,1,0),  vec3(0,0,1) },   // left
        { vec3(0,1,0),  vec3(0,0,-1), vec3(1,0,0) },   // top
        { vec3(0,-1,0), vec3(0,0,1),  vec3(1,0,0) },   // bottom
    };

    float hx = s.x * 0.5f, hy = s.y * 0.5f, hz = s.z * 0.5f;
    float half[3] = {hx, hy, hz};

    for (int f = 0; f < 6; f++) {
        int axis = f / 2;
        float h = half[axis];
        uint32_t base = (uint32_t)m.vertices.size();
        for (int vy = 0; vy <= 1; vy++) {
            for (int vx = 0; vx <= 1; vx++) {
                float u = (float)vx * 2.0f - 1.0f;
                float v_ = (float)vy * 2.0f - 1.0f;
                vec3 local(0);
                if (axis == 0) { local = vec3(h * (f % 2 == 0 ? 1.0f : -1.0f), v_ * half[1], u * half[2]); }
                else if (axis == 1) { local = vec3(u * half[0], h * (f % 2 == 0 ? 1.0f : -1.0f), v_ * half[2]); }
                else { local = vec3(u * half[0], v_ * half[1], h * (f % 2 == 0 ? 1.0f : -1.0f)); }

                float n = vol_noise(local.x * ns, local.y * ns, local.z * ns, seed + f * 7.0f);
                local += faces[f].normal * n * na;
                vec2 uv((float)vx, (float)vy);
                m.add_vertex(local, faces[f].normal, uv);
            }
        }
        m.add_face({base, base + 1, base + 3, base + 2});
    }
    m.compute_normals();
    return m;
}

// ---- CYLINDER ----
Mesh ParametricBuilder::build_cylinder(const VolumeDescription& vol) {
    Mesh m; m.name = vol.name.empty() ? "ParamCyl" : vol.name;
    int seg = vol.segments;
    float seed = vol.seed_offset;
    float na = vol.noise_amount;
    float ns = vol.noise_scale;
    float rad = vol.params.cylinder.radius;
    float h = vol.params.cylinder.height;
    float hh = h * 0.5f;

    // Side vertices: top ring + bottom ring
    for (int i = 0; i <= seg; i++) {
        float theta = 2.0f * (float)M_PI * i / seg;
        float ct = cosf(theta), st = sinf(theta);
        float n_top = vol_noise(ct * ns, hh * ns, st * ns, seed);
        float n_bot = vol_noise(ct * ns, -hh * ns, st * ns, seed + 5.0f);
        float rt = rad * (1.0f + n_top * na);
        float rb = rad * (1.0f + n_bot * na);
        vec3 nrm_side = glm::normalize(vec3(ct * h, (rb - rt), st * h));
        m.add_vertex(vec3(ct * rt, hh, st * rt), vec3(0, 1, 0), vec2((float)i / seg, 1.0f));
        m.add_vertex(vec3(ct * rb, -hh, st * rb), vec3(0, -1, 0), vec2((float)i / seg, 0.0f));
        m.add_vertex(vec3(ct * rb, -hh, st * rb), nrm_side, vec2((float)i / seg, 0.0f));
        m.add_vertex(vec3(ct * rt, hh, st * rt), nrm_side, vec2((float)i / seg, 1.0f));
    }
    for (int i = 0; i < seg; i++) {
        int b = i * 4;
        m.add_face({(uint32_t)b, (uint32_t)(b + 4), (uint32_t)(b + 5), (uint32_t)(b + 1)});
        m.add_face({(uint32_t)(b + 2), (uint32_t)(b + 3), (uint32_t)(b + 7), (uint32_t)(b + 6)});
    }

    // Top cap
    uint32_t tc = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0, hh, 0), vec3(0, 1, 0));
    for (int i = 0; i < seg; i++) {
        m.add_face({tc, (uint32_t)(i * 4), (uint32_t)(((i + 1) % (seg + 1)) * 4)});
    }

    // Bottom cap
    uint32_t bc = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0, -hh, 0), vec3(0, -1, 0));
    for (int i = 0; i < seg; i++) {
        m.add_face({bc, (uint32_t)(((i + 1) % (seg + 1)) * 4 + 1), (uint32_t)(i * 4 + 1)});
    }

    m.compute_normals();
    return m;
}

// ---- CONE ----
Mesh ParametricBuilder::build_cone(const VolumeDescription& vol) {
    Mesh m; m.name = vol.name.empty() ? "ParamCone" : vol.name;
    int seg = vol.segments;
    float seed = vol.seed_offset;
    float na = vol.noise_amount;
    float ns = vol.noise_scale;
    float rad = vol.params.cone.radius;
    float h = vol.params.cone.height;
    float hh = h * 0.5f;

    // Tip vertex
    uint32_t tip = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0, hh, 0), vec3(0, 1, 0));

    // Side rings (3 rings for detail)
    for (int ring = 0; ring <= 2; ring++) {
        float t = (float)ring / 2.0f;
        float r = rad * (1.0f - t);
        float y = hh - t * h;
        for (int i = 0; i <= seg; i++) {
            float theta = 2.0f * (float)M_PI * i / seg;
            float ct = cosf(theta), st = sinf(theta);
            float n = vol_noise(ct * ns, y * ns, st * ns, seed + ring * 3.0f);
            float rr = r * (1.0f + n * na);
            vec3 pos(ct * rr, y, st * rr);
            vec3 nrm = glm::normalize(vec3(ct * h / rad, 1.0f, st * h / rad));
            m.add_vertex(pos, nrm, vec2((float)i / seg, 1.0f - t));
        }
    }

    // Side faces
    for (int ring = 0; ring < 2; ring++) {
        for (int i = 0; i < seg; i++) {
            uint32_t row1 = 1 + ring * (seg + 1) + i;
            uint32_t row2 = 1 + (ring + 1) * (seg + 1) + i;
            m.add_face({row1, row2, row2 + 1, row1 + 1});
        }
    }
    // Top cap faces (tip to first ring)
    for (int i = 0; i < seg; i++) {
        m.add_face({tip, (uint32_t)(1 + i), (uint32_t)(1 + i + 1)});
    }

    // Bottom cap
    uint32_t bc = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0, -hh, 0), vec3(0, -1, 0));
    uint32_t bottom_ring = 1 + 2 * (seg + 1);
    for (int i = 0; i < seg; i++) {
        m.add_face({bc, (uint32_t)(bottom_ring + i + 1), (uint32_t)(bottom_ring + i)});
    }

    m.compute_normals();
    return m;
}

// ---- TORUS ----
Mesh ParametricBuilder::build_torus(const VolumeDescription& vol) {
    Mesh m; m.name = vol.name.empty() ? "ParamTorus" : vol.name;
    int seg = vol.segments;
    int minor_seg = vol.rings;
    float seed = vol.seed_offset;
    float na = vol.noise_amount;
    float ns = vol.noise_scale;
    float R = vol.params.torus.major_r;
    float r = vol.params.torus.minor_r;

    for (int i = 0; i <= seg; i++) {
        float theta = 2.0f * (float)M_PI * i / seg;
        float ct = cosf(theta), st = sinf(theta);
        for (int j = 0; j <= minor_seg; j++) {
            float phi = 2.0f * (float)M_PI * j / minor_seg;
            float cp = cosf(phi), sp = sinf(phi);
            float nx = (R + r * cp) * ct;
            float ny = r * sp;
            float nz = (R + r * cp) * st;
            float n = vol_noise(nx * ns, ny * ns, nz * ns, seed);
            vec3 pos(nx + n * na * ct, ny + n * na, nz + n * na * st);
            vec3 nrm = glm::normalize(vec3(cp * ct, sp, cp * st));
            vec2 uv((float)i / seg, (float)j / minor_seg);
            m.add_vertex(pos, nrm, uv);
        }
    }
    for (int i = 0; i < seg; i++) {
        for (int j = 0; j < minor_seg; j++) {
            uint32_t a = i * (minor_seg + 1) + j;
            uint32_t b = a + (minor_seg + 1);
            m.add_face({a, b, b + 1, a + 1});
        }
    }
    m.compute_normals();
    return m;
}

// ---- CAPSULE ----
Mesh ParametricBuilder::build_capsule(const VolumeDescription& vol) {
    Mesh m; m.name = vol.name.empty() ? "ParamCapsule" : vol.name;
    int seg = vol.segments;
    float seed = vol.seed_offset;
    float na = vol.noise_amount;
    float ns = vol.noise_scale;
    float rad = vol.params.capsule.radius;
    float cyl_h = vol.params.capsule.height;
    float half_cyl = cyl_h * 0.5f;

    // Top hemisphere
    int h_rings = seg / 3;
    for (int r = 0; r <= h_rings; r++) {
        float phi = (float)M_PI * 0.5f * r / h_rings;
        for (int s = 0; s <= seg; s++) {
            float theta = 2.0f * (float)M_PI * s / seg;
            float nx = cosf(theta) * sinf(phi);
            float ny = cosf(phi);
            float nz = sinf(theta) * sinf(phi);
            float n = vol_noise(nx * ns, (ny + 1.0f) * ns, nz * ns, seed);
            float rr = rad * (1.0f + n * na);
            vec3 pos(rr * nx, half_cyl + rr * ny, rr * nz);
            m.add_vertex(pos, glm::normalize(vec3(nx, ny, nz)), vec2((float)s / seg, 1.0f - (float)r / h_rings));
        }
    }

    // Bottom hemisphere
    for (int r = 0; r <= h_rings; r++) {
        float phi = (float)M_PI * 0.5f + (float)M_PI * 0.5f * r / h_rings;
        for (int s = 0; s <= seg; s++) {
            float theta = 2.0f * (float)M_PI * s / seg;
            float nx = cosf(theta) * sinf(phi);
            float ny = cosf(phi);
            float nz = sinf(theta) * sinf(phi);
            float n = vol_noise(nx * ns, (ny - 1.0f) * ns, nz * ns, seed + 10.0f);
            float rr = rad * (1.0f + n * na);
            vec3 pos(rr * nx, -half_cyl + rr * ny, rr * nz);
            m.add_vertex(pos, glm::normalize(vec3(nx, ny, nz)), vec2((float)s / seg, (float)r / h_rings));
        }
    }

    // Top cap vertex
    m.add_vertex(vec3(0, half_cyl + rad, 0), vec3(0, 1, 0));

    // Bottom cap vertex
    m.add_vertex(vec3(0, -half_cyl - rad, 0), vec3(0, -1, 0));

    // Connect top hemisphere faces
    for (int r = 0; r < h_rings; r++) {
        for (int s = 0; s < seg; s++) {
            uint32_t a = r * (seg + 1) + s;
            uint32_t b = (r + 1) * (seg + 1) + s;
            m.add_face({a, b, b + 1, a + 1});
        }
    }
    // Connect bottom hemisphere faces
    int bot_start = h_rings * (seg + 1);
    for (int r = 0; r < h_rings; r++) {
        for (int s = 0; s < seg; s++) {
            uint32_t a = bot_start + r * (seg + 1) + s;
            uint32_t b = bot_start + (r + 1) * (seg + 1) + s;
            m.add_face({a, b, b + 1, a + 1});
        }
    }

    m.compute_normals();
    return m;
}

// ---- HEMISPHERE ----
Mesh ParametricBuilder::build_hemisphere(const VolumeDescription& vol) {
    Mesh m; m.name = vol.name.empty() ? "ParamHemi" : vol.name;
    int seg = vol.segments;
    float seed = vol.seed_offset;
    float na = vol.noise_amount;
    float ns = vol.noise_scale;
    float rad = vol.params.hemisphere.radius;

    for (int r = 0; r <= seg / 2; r++) {
        float phi = (float)M_PI * 0.5f * r / (seg / 2);
        for (int s = 0; s <= seg; s++) {
            float theta = 2.0f * (float)M_PI * s / seg;
            float nx = cosf(theta) * sinf(phi);
            float ny = cosf(phi);
            float nz = sinf(theta) * sinf(phi);
            float n = vol_noise(nx * ns, ny * ns, nz * ns, seed);
            float rr = rad * (1.0f + n * na);
            m.add_vertex(vec3(rr * nx, rr * ny, rr * nz), glm::normalize(vec3(nx, ny, nz)),
                         vec2((float)s / seg, (float)r / (seg / 2)));
        }
    }
    for (int r = 0; r < seg / 2; r++) {
        for (int s = 0; s < seg; s++) {
            uint32_t a = r * (seg + 1) + s;
            m.add_face({a, a + (uint32_t)(seg + 1), a + (uint32_t)(seg + 2), a + 1});
        }
    }
    m.compute_normals();
    return m;
}

// ---- RING ----
Mesh ParametricBuilder::build_ring(const VolumeDescription& vol) {
    Mesh m; m.name = vol.name.empty() ? "ParamRing" : vol.name;
    int seg = vol.segments;
    float seed = vol.seed_offset;
    float na = vol.noise_amount;
    float ns = vol.noise_scale;
    float inner = vol.params.ring.inner_r;
    float outer = vol.params.ring.outer_r;

    for (int s = 0; s <= seg; s++) {
        float theta = 2.0f * (float)M_PI * s / seg;
        float ct = cosf(theta), st = sinf(theta);
        float n_in = vol_noise(ct * ns, 0, st * ns, seed);
        float n_out = vol_noise(ct * ns, 0, st * ns, seed + 5.0f);
        float ri = inner * (1.0f + n_in * na);
        float ro = outer * (1.0f + n_out * na);
        m.add_vertex(vec3(ct * ri, 0, st * ri), vec3(0, 1, 0), vec2((float)s / seg, 0));
        m.add_vertex(vec3(ct * ro, 0, st * ro), vec3(0, 1, 0), vec2((float)s / seg, 1));
    }
    for (int s = 0; s < seg; s++) {
        uint32_t a = s * 2;
        m.add_face({a, a + 2, a + 3, a + 1});
    }
    m.compute_normals();
    return m;
}

// ---- WEDGE ----
Mesh ParametricBuilder::build_wedge(const VolumeDescription& vol) {
    Mesh m; m.name = vol.name.empty() ? "ParamWedge" : vol.name;
    int seg = vol.segments;
    float seed = vol.seed_offset;
    float na = vol.noise_amount;
    float ns = vol.noise_scale;
    float rad = vol.params.wedge.radius;
    float h = vol.params.wedge.height;
    float angle = vol.params.wedge.angle;
    float hh = h * 0.5f;

    // Bottom disc
    uint32_t bot_center = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0, -hh, 0), vec3(0, -1, 0));
    int wedges = std::max(3, (int)(angle / (2.0f * (float)M_PI) * seg));
    for (int i = 0; i <= wedges; i++) {
        float theta = -angle * 0.5f + angle * i / wedges;
        float ct = cosf(theta), st = sinf(theta);
        float n = vol_noise(ct * ns, -hh * ns, st * ns, seed);
        float rr = rad * (1.0f + n * na);
        m.add_vertex(vec3(ct * rr, -hh, st * rr), vec3(0, -1, 0), vec2((float)i / wedges, 0));
    }
    for (int i = 0; i < wedges; i++) {
        m.add_face({bot_center, (uint32_t)(1 + i + 1), (uint32_t)(1 + i)});
    }

    // Top disc
    uint32_t top_center = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0, hh, 0), vec3(0, 1, 0));
    for (int i = 0; i <= wedges; i++) {
        float theta = -angle * 0.5f + angle * i / wedges;
        float ct = cosf(theta), st = sinf(theta);
        float n = vol_noise(ct * ns, hh * ns, st * ns, seed + 10.0f);
        float rr = rad * (1.0f + n * na);
        m.add_vertex(vec3(ct * rr, hh, st * rr), vec3(0, 1, 0), vec2((float)i / wedges, 1));
    }
    uint32_t top_base = 1 + wedges + 1;
    for (int i = 0; i < wedges; i++) {
        m.add_face({top_center, (uint32_t)(top_base + i), (uint32_t)(top_base + i + 1)});
    }

    // Side faces
    for (int i = 0; i < wedges; i++) {
        uint32_t bl = 1 + i;
        uint32_t br = 1 + i + 1;
        uint32_t tl = top_base + i;
        uint32_t tr = top_base + i + 1;
        m.add_face({bl, br, tr, tl});
    }

    m.compute_normals();
    return m;
}

// ---- DISC ----
Mesh ParametricBuilder::build_disc(const VolumeDescription& vol) {
    Mesh m; m.name = vol.name.empty() ? "ParamDisc" : vol.name;
    int seg = vol.segments;
    float seed = vol.seed_offset;
    float na = vol.noise_amount;
    float ns = vol.noise_scale;
    float rad = vol.params.disc.radius;
    float thick = vol.params.disc.thickness;
    float hh = thick * 0.5f;

    // Top face
    uint32_t tc = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0, hh, 0), vec3(0, 1, 0));
    for (int i = 0; i <= seg; i++) {
        float theta = 2.0f * (float)M_PI * i / seg;
        float ct = cosf(theta), st = sinf(theta);
        float n = vol_noise(ct * ns, hh * ns, st * ns, seed);
        float rr = rad * (1.0f + n * na);
        m.add_vertex(vec3(ct * rr, hh, st * rr), vec3(0, 1, 0), vec2(0.5f + ct * 0.5f, 0.5f + st * 0.5f));
    }
    for (int i = 0; i < seg; i++) {
        m.add_face({tc, (uint32_t)(tc + 1 + i), (uint32_t)(tc + 2 + i)});
    }

    // Bottom face
    uint32_t bc = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0, -hh, 0), vec3(0, -1, 0));
    for (int i = 0; i <= seg; i++) {
        float theta = 2.0f * (float)M_PI * i / seg;
        float ct = cosf(theta), st = sinf(theta);
        float n = vol_noise(ct * ns, -hh * ns, st * ns, seed + 5.0f);
        float rr = rad * (1.0f + n * na);
        m.add_vertex(vec3(ct * rr, -hh, st * rr), vec3(0, -1, 0), vec2(0.5f + ct * 0.5f, 0.5f + st * 0.5f));
    }
    uint32_t bot_ring = bc + 1;
    for (int i = 0; i < seg; i++) {
        m.add_face({bc, (uint32_t)(bot_ring + i + 1), (uint32_t)(bot_ring + i)});
    }

    // Side faces connecting top and bottom rings
    uint32_t top_ring = tc + 1;
    for (int i = 0; i < seg; i++) {
        m.add_face({top_ring + i, top_ring + i + 1, bot_ring + i + 1, bot_ring + i});
    }

    m.compute_normals();
    return m;
}

// ---- ARROW (shaft cylinder + head cone) ----
Mesh ParametricBuilder::build_arrow(const VolumeDescription& vol) {
    Mesh m; m.name = vol.name.empty() ? "ParamArrow" : vol.name;
    float seed = vol.seed_offset;
    float na = vol.noise_amount;
    float ns = vol.noise_scale;
    float shaft_r = vol.params.cylinder.radius;
    float shaft_h = vol.params.cylinder.height;
    float head_r = vol.params.cone.radius;
    float head_h = vol.params.cone.height;
    int seg = vol.segments;

    // Shaft (cylinder)
    float hh = shaft_h * 0.5f;
    for (int i = 0; i <= seg; i++) {
        float theta = 2.0f * (float)M_PI * i / seg;
        float ct = cosf(theta), st = sinf(theta);
        float n = vol_noise(ct * ns, 0, st * ns, seed);
        float rr = shaft_r * (1.0f + n * na);
        m.add_vertex(vec3(ct * rr, -hh, st * rr), vec3(0, -1, 0), vec2((float)i / seg, 0));
        m.add_vertex(vec3(ct * rr, hh, st * rr), vec3(0, 1, 0), vec2((float)i / seg, 1));
    }
    for (int i = 0; i < seg; i++) {
        uint32_t b = i * 2;
        m.add_face({(uint32_t)b, (uint32_t)(b + 1), (uint32_t)(b + 3), (uint32_t)(b + 2)});
    }
    // Shaft caps
    uint32_t bot_c = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0, -hh, 0), vec3(0, -1, 0));
    for (int i = 0; i < seg; i++) m.add_face({bot_c, (uint32_t)(i * 2 + 2), (uint32_t)(i * 2)});
    uint32_t top_c = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0, hh, 0), vec3(0, 1, 0));
    for (int i = 0; i < seg; i++) m.add_face({top_c, (uint32_t)(i * 2 + 1), (uint32_t)(i * 2 + 3)});

    // Head (cone, sits on top of shaft)
    float head_base = hh;
    uint32_t tip = (uint32_t)m.vertices.size();
    m.add_vertex(vec3(0, head_base + head_h, 0), vec3(0, 1, 0));
    for (int i = 0; i <= seg; i++) {
        float theta = 2.0f * (float)M_PI * i / seg;
        float ct = cosf(theta), st = sinf(theta);
        float n = vol_noise(ct * ns, head_base * ns, st * ns, seed + 20);
        float rr = head_r * (1.0f + n * na);
        vec3 nrm = glm::normalize(vec3(ct * head_h / head_r, 1.0f, st * head_h / head_r));
        m.add_vertex(vec3(ct * rr, head_base, st * rr), nrm, vec2((float)i / seg, 0));
    }
    for (int i = 0; i < seg; i++) {
        uint32_t ring_start = tip + 1;
        m.add_face({tip, (uint32_t)(ring_start + i + 1), (uint32_t)(ring_start + i)});
    }

    m.compute_normals();
    return m;
}

// ---- PLANE (flat quad) ----
Mesh ParametricBuilder::build_plane(const VolumeDescription& vol) {
    Mesh m; m.name = vol.name.empty() ? "ParamPlane" : vol.name;
    float seed = vol.seed_offset;
    float na = vol.noise_amount;
    float ns = vol.noise_scale;
    vec3 s = vol.scale;
    float hx = s.x * 0.5f;
    float hz = s.z * 0.5f;

    // 4 corner vertices
    float n0 = vol_noise(-hx * ns, 0, -hz * ns, seed);
    float n1 = vol_noise(hx * ns, 0, -hz * ns, seed);
    float n2 = vol_noise(hx * ns, 0, hz * ns, seed);
    float n3 = vol_noise(-hx * ns, 0, hz * ns, seed);

    m.add_vertex(vec3(-hx, n0 * na * s.y, -hz), vec3(0, 1, 0), vec2(0, 0));
    m.add_vertex(vec3(hx, n1 * na * s.y, -hz), vec3(0, 1, 0), vec2(1, 0));
    m.add_vertex(vec3(hx, n2 * na * s.y, hz), vec3(0, 1, 0), vec2(1, 1));
    m.add_vertex(vec3(-hx, n3 * na * s.y, hz), vec3(0, 1, 0), vec2(0, 1));

    m.add_face({0, 1, 2, 3});

    // Bottom face
    m.add_vertex(vec3(-hx, -n0 * na * s.y * 0.01f, -hz), vec3(0, -1, 0), vec2(0, 0));
    m.add_vertex(vec3(hx, -n1 * na * s.y * 0.01f, -hz), vec3(0, -1, 0), vec2(1, 0));
    m.add_vertex(vec3(hx, -n2 * na * s.y * 0.01f, hz), vec3(0, -1, 0), vec2(1, 1));
    m.add_vertex(vec3(-hx, -n3 * na * s.y * 0.01f, hz), vec3(0, -1, 0), vec2(0, 1));
    m.add_face({4, 7, 6, 5});

    m.compute_normals();
    return m;
}

// ---- LOFT (interpolates cross-sections along a path) ----
Mesh ParametricBuilder::build_loft(const VolumeDescription& vol,
                                   const std::vector<CrossSection>& path) {
    Mesh m; m.name = vol.name.empty() ? "ParamLoft" : vol.name;
    if (path.size() < 2) return m;
    float seed = vol.seed_offset;
    float na = vol.noise_amount;
    float ns = vol.noise_scale;

    int profile_pts = (int)path[0].profile.size();
    if (profile_pts < 3) return m;

    // Generate vertices for each cross-section
    for (size_t cs = 0; cs < path.size(); cs++) {
        for (int p = 0; p < profile_pts; p++) {
            vec3 local = path[cs].profile[p];
            vec3 world = path[cs].center + local;
            float n = vol_noise(world.x * ns, world.y * ns, world.z * ns, seed + (float)cs);
            world += vec3(n * na);
            m.add_vertex(world, vec3(0, 1, 0), vec2((float)p / profile_pts, (float)cs / (path.size() - 1)));
        }
    }

    // Create quad faces between cross-sections
    for (size_t cs = 0; cs + 1 < path.size(); cs++) {
        for (int p = 0; p < profile_pts; p++) {
            uint32_t a = (uint32_t)(cs * profile_pts + p);
            uint32_t b = (uint32_t)((cs + 1) * profile_pts + p);
            uint32_t c = (uint32_t)((cs + 1) * profile_pts + (p + 1) % profile_pts);
            uint32_t d = (uint32_t)(cs * profile_pts + (p + 1) % profile_pts);
            m.add_face({a, b, c, d});
        }
    }

    m.compute_normals();
    return m;
}

// ---- TRANSFORM ----
void ParametricBuilder::transform_mesh(Mesh& m, const VolumeDescription& vol) {
    mat4 T = glm::translate(mat4(1.0f), vol.position);
    mat4 Ry = glm::rotate(mat4(1.0f), vol.rotation.y, vec3(0, 1, 0));
    mat4 Rx = glm::rotate(mat4(1.0f), vol.rotation.x, vec3(1, 0, 0));
    mat4 Rz = glm::rotate(mat4(1.0f), vol.rotation.z, vec3(0, 0, 1));
    mat4 S = glm::scale(mat4(1.0f), vol.scale);
    mat4 M = T * Ry * Rx * Rz * S;

    mat3 nm = mat3_normal_from_mat4(M);
    for (auto& v : m.vertices) {
        v.position = vec3(M * vec4(v.position, 1.0f));
        v.normal = glm::normalize(nm * v.normal);
    }
    m.dirty = true;
    m.compute_normals();
}

// ---- BUILD DISPATCH ----
Mesh ParametricBuilder::build(const DecomposedObject& obj) {
    Mesh result;
    result.name = obj.name;

    for (auto& vol : obj.volumes) {
        Mesh part;
        switch (vol.type) {
            case VolumeType::Sphere: part = build_sphere(vol); break;
            case VolumeType::Box: part = build_box(vol); break;
            case VolumeType::Cylinder: part = build_cylinder(vol); break;
            case VolumeType::Cone: part = build_cone(vol); break;
            case VolumeType::Torus: part = build_torus(vol); break;
            case VolumeType::Capsule: part = build_capsule(vol); break;
            case VolumeType::Hemisphere: part = build_hemisphere(vol); break;
            case VolumeType::Ring: part = build_ring(vol); break;
            case VolumeType::Wedge: part = build_wedge(vol); break;
            case VolumeType::Disc: part = build_disc(vol); break;
            case VolumeType::Arrow: part = build_arrow(vol); break;
            case VolumeType::Plane: part = build_plane(vol); break;
            default: part = build_box(vol); break;
        }

        transform_mesh(part, vol);

        // Apply material to each vertex color based on material
        vec4 mat_color = vol.material.diffuse;
        if (mat_color.w > 0.0f) {
            for (auto& v : part.vertices) {
                v.color = mat_color;
            }
        }

        // Merge into result
        uint32_t base = (uint32_t)result.vertices.size();
        for (auto& v : part.vertices) {
            result.add_vertex(v.position, v.normal, v.uv, v.color);
        }
        for (auto& f : part.faces) {
            std::vector<uint32_t> fi;
            for (auto& vi : f.vertices) fi.push_back(vi + base);
            result.add_face(fi, f.normal);
        }
    }

    result.compute_normals();
    return result;
}

// ============================================================
// DECOMPOSER — maps object types to volume decomposition rules
// ============================================================

Decomposer::Decomposer() { init_rules(); }

VolumeDescription Decomposer::make_sphere(vec3 pos, float r, int seg) {
    VolumeDescription v; v.type = VolumeType::Sphere;
    v.position = pos; v.scale = vec3(1);
    v.params.sphere.radius = r; v.segments = seg; v.rings = seg / 2;
    return v;
}

VolumeDescription Decomposer::make_box(vec3 pos, vec3 sc) {
    VolumeDescription v; v.type = VolumeType::Box;
    v.position = pos; v.scale = sc;
    return v;
}

VolumeDescription Decomposer::make_cylinder(vec3 pos, float r, float h, int seg) {
    VolumeDescription v; v.type = VolumeType::Cylinder;
    v.position = pos; v.scale = vec3(1);
    v.params.cylinder.radius = r; v.params.cylinder.height = h; v.segments = seg;
    return v;
}

VolumeDescription Decomposer::make_cone(vec3 pos, float r, float h, int seg) {
    VolumeDescription v; v.type = VolumeType::Cone;
    v.position = pos; v.scale = vec3(1);
    v.params.cone.radius = r; v.params.cone.height = h; v.segments = seg;
    return v;
}

VolumeDescription Decomposer::make_torus(vec3 pos, float major_r, float minor_r, int seg) {
    VolumeDescription v; v.type = VolumeType::Torus;
    v.position = pos; v.scale = vec3(1);
    v.params.torus.major_r = major_r; v.params.torus.minor_r = minor_r;
    v.segments = seg; v.rings = seg / 2;
    return v;
}

VolumeDescription Decomposer::make_capsule(vec3 pos, float r, float h, int seg) {
    VolumeDescription v; v.type = VolumeType::Capsule;
    v.position = pos; v.scale = vec3(1);
    v.params.capsule.radius = r; v.params.capsule.height = h; v.segments = seg;
    return v;
}

VolumeDescription Decomposer::make_hemisphere(vec3 pos, float r, int seg) {
    VolumeDescription v; v.type = VolumeType::Hemisphere;
    v.position = pos; v.scale = vec3(1);
    v.params.hemisphere.radius = r; v.segments = seg;
    return v;
}

VolumeDescription Decomposer::make_ring(vec3 pos, float inner_r, float outer_r, int seg) {
    VolumeDescription v; v.type = VolumeType::Ring;
    v.position = pos; v.scale = vec3(1);
    v.params.ring.inner_r = inner_r; v.params.ring.outer_r = outer_r; v.segments = seg;
    return v;
}

VolumeDescription Decomposer::make_disc(vec3 pos, float r, float thickness, int seg) {
    VolumeDescription v; v.type = VolumeType::Disc;
    v.position = pos; v.scale = vec3(1);
    v.params.disc.radius = r; v.params.disc.thickness = thickness; v.segments = seg;
    return v;
}

VolumeDescription Decomposer::make_wedge(vec3 pos, float r, float h, float angle, int seg) {
    VolumeDescription v; v.type = VolumeType::Wedge;
    v.position = pos; v.scale = vec3(1);
    v.params.wedge.radius = r; v.params.wedge.height = h;
    v.params.wedge.angle = angle; v.segments = seg;
    return v;
}

VolumeDescription Decomposer::make_arrow(vec3 pos, float shaft_r, float shaft_h, float head_r, float head_h, int seg) {
    VolumeDescription v; v.type = VolumeType::Arrow;
    v.position = pos; v.scale = vec3(1);
    v.params.cylinder.radius = shaft_r; v.params.cylinder.height = shaft_h;
    v.params.cone.radius = head_r; v.params.cone.height = head_h;
    v.segments = seg;
    return v;
}

VolumeDescription Decomposer::make_plane(vec3 pos, vec3 scale) {
    VolumeDescription v; v.type = VolumeType::Plane;
    v.position = pos; v.scale = scale;
    return v;
}

void Decomposer::init_rules() {
    // Each rule maps keywords to a lambda that produces a DecomposedObject

    rules.push_back({"car", {"car", "voiture", "auto", "automobile"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Car";
            float s = sz * 1.5f;
            float n = 0.08f;
            float ns = 2.5f;

            // Body (lower box)
            auto body = make_box(vec3(0, s*0.25f, 0), vec3(s*1.8f, s*0.35f, s*0.8f));
            body.noise_amount = n; body.noise_scale = ns; body.seed_offset = seed;
            body.name = "Body";
            obj.volumes.push_back(body);

            // Cabin (upper box)
            auto cabin = make_box(vec3(-s*0.15f, s*0.55f, 0), vec3(s*0.9f, s*0.3f, s*0.7f));
            cabin.noise_amount = n; cabin.noise_scale = ns; cabin.seed_offset = seed + 1;
            cabin.name = "Cabin";
            obj.volumes.push_back(cabin);

            // Hood (front wedge-ish)
            auto hood = make_box(vec3(s*0.65f, s*0.35f, 0), vec3(s*0.5f, s*0.15f, s*0.75f));
            hood.noise_amount = n*0.5f; hood.noise_scale = ns; hood.seed_offset = seed + 2;
            hood.name = "Hood";
            obj.volumes.push_back(hood);

            // Trunk (rear box)
            auto trunk = make_box(vec3(-s*0.7f, s*0.35f, 0), vec3(s*0.35f, s*0.2f, s*0.7f));
            trunk.noise_amount = n*0.5f; trunk.noise_scale = ns; trunk.seed_offset = seed + 3;
            trunk.name = "Trunk";
            obj.volumes.push_back(trunk);

            // 4 wheels (tori)
            float wx = s*0.6f, wz = s*0.45f, wy = s*0.08f;
            auto w1 = make_torus(vec3(wx, wy, wz), s*0.15f, s*0.06f, 16);
            auto w2 = make_torus(vec3(wx, wy, -wz), s*0.15f, s*0.06f, 16);
            auto w3 = make_torus(vec3(-wx, wy, wz), s*0.15f, s*0.06f, 16);
            auto w4 = make_torus(vec3(-wx, wy, -wz), s*0.15f, s*0.06f, 16);
            for (auto* w : {&w1,&w2,&w3,&w4}) {
                w->noise_amount = 0.02f; w->noise_scale = 4.0f; w->seed_offset = seed + 10;
                w->rotation = vec3((float)M_PI*0.5f, 0, 0);
                w->material.diffuse = vec4(0.15f, 0.15f, 0.15f, 1);
            }
            w1.name = "WheelFR"; w2.name = "WheelFL";
            w3.name = "WheelRR"; w4.name = "WheelRL";
            obj.volumes.push_back(w1); obj.volumes.push_back(w2);
            obj.volumes.push_back(w3); obj.volumes.push_back(w4);

            // Headlights (small spheres)
            auto hl1 = make_sphere(vec3(s*0.9f, s*0.3f, s*0.25f), s*0.05f, 12);
            auto hl2 = make_sphere(vec3(s*0.9f, s*0.3f, -s*0.25f), s*0.05f, 12);
            hl1.material.diffuse = vec4(1, 1, 0.8f, 1); hl1.material.emission = vec4(1, 1, 0.6f, 1);
            hl2.material.diffuse = vec4(1, 1, 0.8f, 1); hl2.material.emission = vec4(1, 1, 0.6f, 1);
            hl1.name = "HeadlightL"; hl2.name = "HeadlightR";
            obj.volumes.push_back(hl1); obj.volumes.push_back(hl2);

            // Taillights (small spheres)
            auto tl1 = make_sphere(vec3(-s*0.87f, s*0.3f, s*0.25f), s*0.04f, 10);
            auto tl2 = make_sphere(vec3(-s*0.87f, s*0.3f, -s*0.25f), s*0.04f, 10);
            tl1.material.diffuse = vec4(0.8f, 0.05f, 0.05f, 1); tl1.material.emission = vec4(0.6f, 0, 0, 1);
            tl2.material.diffuse = vec4(0.8f, 0.05f, 0.05f, 1); tl2.material.emission = vec4(0.6f, 0, 0, 1);
            tl1.name = "TaillightL"; tl2.name = "TaillightR";
            obj.volumes.push_back(tl1); obj.volumes.push_back(tl2);

            if (col.w > 0) { body.material.diffuse = col; cabin.material.diffuse = col; }
            return obj;
        }
    });

    rules.push_back({"house", {"house", "maison", "home"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "House";
            float s = sz;
            float n = 0.06f;

            // Walls (box)
            auto walls = make_box(vec3(0, s*0.5f, 0), vec3(s*1.5f, s*1.0f, s*1.2f));
            walls.noise_amount = n; walls.noise_scale = 3.0f; walls.seed_offset = seed;
            walls.name = "Walls";
            if (col.w > 0) walls.material.diffuse = col;
            obj.volumes.push_back(walls);

            // Roof (cone)
            auto roof = make_cone(vec3(0, s*1.3f, 0), s*1.2f, s*0.8f, 4);
            roof.noise_amount = 0.03f; roof.noise_scale = 2.0f; roof.seed_offset = seed + 5;
            roof.rotation.y = (float)M_PI * 0.25f;
            roof.material.diffuse = vec4(0.6f, 0.15f, 0.1f, 1);
            roof.name = "Roof";
            obj.volumes.push_back(roof);

            // Door (box)
            auto door = make_box(vec3(s*0.4f, s*0.35f, s*0.601f), vec3(s*0.2f, s*0.5f, s*0.05f));
            door.noise_amount = 0.02f; door.noise_scale = 5.0f; door.seed_offset = seed + 10;
            door.material.diffuse = vec4(0.4f, 0.25f, 0.1f, 1);
            door.name = "Door";
            obj.volumes.push_back(door);

            // Window left (box)
            auto win_l = make_box(vec3(-s*0.3f, s*0.6f, s*0.601f), vec3(s*0.2f, s*0.2f, s*0.05f));
            win_l.material.diffuse = vec4(0.6f, 0.8f, 1.0f, 0.8f);
            win_l.name = "WindowL";
            obj.volumes.push_back(win_l);

            // Window right (box)
            auto win_r = make_box(vec3(s*0.3f, s*0.6f, -s*0.601f), vec3(s*0.2f, s*0.2f, s*0.05f));
            win_r.material.diffuse = vec4(0.6f, 0.8f, 1.0f, 0.8f);
            win_r.name = "WindowR";
            obj.volumes.push_back(win_r);

            // Chimney (cylinder)
            auto chimney = make_cylinder(vec3(-s*0.4f, s*1.2f, -s*0.2f), s*0.08f, s*0.4f, 8);
            chimney.noise_amount = 0.04f; chimney.noise_scale = 4.0f; chimney.seed_offset = seed + 15;
            chimney.material.diffuse = vec4(0.45f, 0.25f, 0.15f, 1);
            chimney.name = "Chimney";
            obj.volumes.push_back(chimney);

            // Foundation (box)
            auto foundation = make_box(vec3(0, -s*0.02f, 0), vec3(s*1.6f, s*0.06f, s*1.3f));
            foundation.noise_amount = 0.03f; foundation.noise_scale = 4.0f; foundation.seed_offset = seed + 20;
            foundation.material.diffuse = vec4(0.5f, 0.5f, 0.5f, 1);
            foundation.name = "Foundation";
            obj.volumes.push_back(foundation);

            return obj;
        }
    });

    rules.push_back({"tree", {"tree", "arbre"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Tree";
            float s = sz;

            // Trunk (cylinder, tapered)
            auto trunk = make_cylinder(vec3(0, s*0.4f, 0), s*0.1f, s*0.8f, 10);
            trunk.noise_amount = 0.08f; trunk.noise_scale = 4.0f; trunk.seed_offset = seed;
            trunk.material.diffuse = vec4(0.38f, 0.24f, 0.09f, 1);
            trunk.name = "Trunk";
            obj.volumes.push_back(trunk);

            // Foliage layers (3 spheres, different sizes and heights)
            float foliage_colors[3][3] = {{0.08f,0.45f,0.08f},{0.12f,0.5f,0.1f},{0.06f,0.4f,0.06f}};
            for (int i = 0; i < 3; i++) {
                float h = s * (0.9f + i * 0.35f);
                float r = s * (0.55f - i * 0.08f);
                float x_off = fbm3d((float)i * 3.0f + seed, 0, 0, 3) * s * 0.15f;
                float z_off = fbm3d(0, (float)i * 3.0f + seed, 0, 3) * s * 0.15f;
                auto foliage = make_sphere(vec3(x_off, h, z_off), r, 16);
                foliage.noise_amount = 0.25f; foliage.noise_scale = 2.5f;
                foliage.seed_offset = seed + (float)(i + 1) * 7.0f;
                foliage.material.diffuse = vec4(foliage_colors[i][0], foliage_colors[i][1], foliage_colors[i][2], 1);
                foliage.name = "Foliage" + std::to_string(i);
                obj.volumes.push_back(foliage);
            }

            // Root bulges
            for (int i = 0; i < 4; i++) {
                float angle = (float)M_PI * 2.0f * i / 4.0f + seed * 0.1f;
                float x = cosf(angle) * s * 0.15f;
                float z = sinf(angle) * s * 0.15f;
                auto root = make_sphere(vec3(x, -s*0.05f, z), s*0.08f, 8);
                root.noise_amount = 0.15f; root.noise_scale = 3.0f; root.seed_offset = seed + 30 + (float)i;
                root.scale = vec3(1.5f, 0.5f, 1.5f);
                root.material.diffuse = vec4(0.35f, 0.22f, 0.08f, 1);
                root.name = "Root" + std::to_string(i);
                obj.volumes.push_back(root);
            }

            return obj;
        }
    });

    rules.push_back({"robot", {"robot"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Robot";
            float s = sz;
            float n = 0.04f;

            // Head (box)
            auto head = make_box(vec3(0, s*1.6f, 0), vec3(s*0.5f, s*0.45f, s*0.45f));
            head.noise_amount = n; head.noise_scale = 3.0f; head.seed_offset = seed;
            head.material.diffuse = vec4(0.6f, 0.65f, 0.7f, 1);
            head.name = "Head";
            obj.volumes.push_back(head);

            // Eyes (spheres, emissive)
            auto eye_l = make_sphere(vec3(-s*0.12f, s*1.65f, s*0.23f), s*0.06f, 10);
            auto eye_r = make_sphere(vec3(s*0.12f, s*1.65f, s*0.23f), s*0.06f, 10);
            eye_l.material.diffuse = vec4(0, 1, 0.5f, 1); eye_l.material.emission = vec4(0, 0.8f, 0.4f, 1);
            eye_r.material.diffuse = vec4(0, 1, 0.5f, 1); eye_r.material.emission = vec4(0, 0.8f, 0.4f, 1);
            eye_l.name = "EyeL"; eye_r.name = "EyeR";
            obj.volumes.push_back(eye_l); obj.volumes.push_back(eye_r);

            // Neck (cylinder)
            auto neck = make_cylinder(vec3(0, s*1.3f, 0), s*0.08f, s*0.15f, 8);
            neck.noise_amount = 0.02f; neck.seed_offset = seed + 5;
            neck.material.diffuse = vec4(0.4f, 0.42f, 0.45f, 1);
            neck.name = "Neck";
            obj.volumes.push_back(neck);

            // Torso (box)
            auto torso = make_box(vec3(0, s*0.85f, 0), vec3(s*0.6f, s*0.8f, s*0.4f));
            torso.noise_amount = n; torso.noise_scale = 3.0f; torso.seed_offset = seed + 1;
            torso.material.diffuse = vec4(0.55f, 0.6f, 0.65f, 1);
            torso.name = "Torso";
            obj.volumes.push_back(torso);

            // Arms (capsules)
            auto arm_l = make_capsule(vec3(-s*0.45f, s*0.8f, 0), s*0.08f, s*0.6f, 10);
            auto arm_r = make_capsule(vec3(s*0.45f, s*0.8f, 0), s*0.08f, s*0.6f, 10);
            arm_l.noise_amount = n; arm_l.seed_offset = seed + 10;
            arm_r.noise_amount = n; arm_r.seed_offset = seed + 11;
            arm_l.material.diffuse = vec4(0.5f, 0.55f, 0.6f, 1);
            arm_r.material.diffuse = vec4(0.5f, 0.55f, 0.6f, 1);
            arm_l.name = "ArmL"; arm_r.name = "ArmR";
            obj.volumes.push_back(arm_l); obj.volumes.push_back(arm_r);

            // Legs (capsules)
            auto leg_l = make_capsule(vec3(-s*0.15f, s*0.15f, 0), s*0.1f, s*0.6f, 10);
            auto leg_r = make_capsule(vec3(s*0.15f, s*0.15f, 0), s*0.1f, s*0.6f, 10);
            leg_l.noise_amount = n; leg_l.seed_offset = seed + 20;
            leg_r.noise_amount = n; leg_r.seed_offset = seed + 21;
            leg_l.material.diffuse = vec4(0.5f, 0.55f, 0.6f, 1);
            leg_r.material.diffuse = vec4(0.5f, 0.55f, 0.6f, 1);
            leg_l.name = "LegL"; leg_r.name = "LegR";
            obj.volumes.push_back(leg_l); obj.volumes.push_back(leg_r);

            // Feet (boxes)
            auto foot_l = make_box(vec3(-s*0.15f, -s*0.05f, s*0.05f), vec3(s*0.15f, s*0.1f, s*0.2f));
            auto foot_r = make_box(vec3(s*0.15f, -s*0.05f, s*0.05f), vec3(s*0.15f, s*0.1f, s*0.2f));
            foot_l.material.diffuse = vec4(0.4f, 0.42f, 0.45f, 1);
            foot_r.material.diffuse = vec4(0.4f, 0.42f, 0.45f, 1);
            foot_l.name = "FootL"; foot_r.name = "FootR";
            obj.volumes.push_back(foot_l); obj.volumes.push_back(foot_r);

            return obj;
        }
    });

    rules.push_back({"castle", {"castle", "chateau"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Castle";
            float s = sz * 2.0f;
            float n = 0.05f;
            vec4 stone_col(0.55f, 0.52f, 0.48f, 1);

            // Main wall (box)
            auto wall = make_box(vec3(0, s*0.5f, 0), vec3(s*2.0f, s*1.0f, s*2.0f));
            wall.noise_amount = n; wall.noise_scale = 3.0f; wall.seed_offset = seed;
            wall.material.diffuse = stone_col; wall.name = "MainWall";
            obj.volumes.push_back(wall);

            // 4 towers (cylinders)
            float tw = s*0.35f, th = s*1.8f;
            vec3 tower_pos[4] = {vec3(tw,tw,0),vec3(-tw,tw,0),vec3(0,tw,tw),vec3(0,tw,-tw)};
            for (int i = 0; i < 4; i++) {
                auto tower = make_cylinder(tower_pos[i], s*0.3f, th, 12);
                tower.noise_amount = n; tower.noise_scale = 3.5f; tower.seed_offset = seed + (float)(i+1)*5;
                tower.material.diffuse = stone_col;
                tower.name = "Tower" + std::to_string(i);
                obj.volumes.push_back(tower);

                // Cone roof on each tower
                auto tower_roof = make_cone(vec3(tower_pos[i].x, th*0.5f+s*0.3f, tower_pos[i].z), s*0.4f, s*0.6f, 12);
                tower_roof.noise_amount = 0.03f; tower_roof.seed_offset = seed + (float)(i+1)*10;
                tower_roof.material.diffuse = vec4(0.5f, 0.12f, 0.08f, 1);
                tower_roof.name = "TowerRoof" + std::to_string(i);
                obj.volumes.push_back(tower_roof);
            }

            // Gate (box)
            auto gate = make_box(vec3(0, s*0.35f, s*1.001f), vec3(s*0.4f, s*0.7f, s*0.05f));
            gate.noise_amount = 0.02f; gate.seed_offset = seed + 50;
            gate.material.diffuse = vec4(0.35f, 0.2f, 0.08f, 1);
            gate.name = "Gate";
            obj.volumes.push_back(gate);

            return obj;
        }
    });

    rules.push_back({"sword", {"sword", "epee", "blade"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Sword";
            float s = sz;

            // Blade (box, thin and long)
            auto blade = make_box(vec3(0, s*0.6f, 0), vec3(s*0.06f, s*1.2f, s*0.01f));
            blade.noise_amount = 0.02f; blade.noise_scale = 5.0f; blade.seed_offset = seed;
            blade.material.diffuse = vec4(0.8f, 0.82f, 0.85f, 1);
            blade.material.metallic = 0.9f; blade.material.roughness = 0.15f;
            blade.name = "Blade";
            obj.volumes.push_back(blade);

            // Guard (box)
            auto guard = make_box(vec3(0, s*0.0f, 0), vec3(s*0.25f, s*0.04f, s*0.06f));
            guard.noise_amount = 0.02f; guard.seed_offset = seed + 5;
            guard.material.diffuse = vec4(0.75f, 0.6f, 0.1f, 1);
            guard.material.metallic = 0.85f;
            guard.name = "Guard";
            obj.volumes.push_back(guard);

            // Handle (cylinder)
            auto handle = make_cylinder(vec3(0, -s*0.15f, 0), s*0.025f, s*0.25f, 8);
            handle.noise_amount = 0.03f; handle.seed_offset = seed + 10;
            handle.material.diffuse = vec4(0.3f, 0.18f, 0.08f, 1);
            handle.name = "Handle";
            obj.volumes.push_back(handle);

            // Pommel (sphere)
            auto pommel = make_sphere(vec3(0, -s*0.3f, 0), s*0.04f, 10);
            pommel.material.diffuse = vec4(0.75f, 0.6f, 0.1f, 1);
            pommel.material.metallic = 0.85f;
            pommel.name = "Pommel";
            obj.volumes.push_back(pommel);

            return obj;
        }
    });

    rules.push_back({"spaceship", {"spaceship", "vaisseau", "ship", "rocket"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Spaceship";
            float s = sz * 1.5f;

            // Main hull (capsule)
            auto hull = make_capsule(vec3(0, 0, 0), s*0.25f, s*0.8f, 16);
            hull.noise_amount = 0.03f; hull.noise_scale = 3.0f; hull.seed_offset = seed;
            hull.material.diffuse = vec4(0.7f, 0.72f, 0.75f, 1);
            hull.material.metallic = 0.7f;
            hull.name = "Hull";
            obj.volumes.push_back(hull);

            // Wings (boxes, swept back)
            auto wing_l = make_box(vec3(-s*0.5f, -s*0.05f, 0), vec3(s*0.6f, s*0.03f, s*0.3f));
            auto wing_r = make_box(vec3(s*0.5f, -s*0.05f, 0), vec3(s*0.6f, s*0.03f, s*0.3f));
            wing_l.rotation.z = 0.1f; wing_r.rotation.z = -0.1f;
            wing_l.noise_amount = 0.02f; wing_r.noise_amount = 0.02f;
            wing_l.seed_offset = seed + 5; wing_r.seed_offset = seed + 6;
            wing_l.material.diffuse = vec4(0.6f, 0.62f, 0.65f, 1);
            wing_r.material.diffuse = vec4(0.6f, 0.62f, 0.65f, 1);
            wing_l.name = "WingL"; wing_r.name = "WingR";
            obj.volumes.push_back(wing_l); obj.volumes.push_back(wing_r);

            // Engine pods (cylinders)
            auto eng_l = make_cylinder(vec3(-s*0.35f, -s*0.1f, -s*0.3f), s*0.08f, s*0.3f, 10);
            auto eng_r = make_cylinder(vec3(s*0.35f, -s*0.1f, -s*0.3f), s*0.08f, s*0.3f, 10);
            eng_l.noise_amount = 0.04f; eng_r.noise_amount = 0.04f;
            eng_l.seed_offset = seed + 10; eng_r.seed_offset = seed + 11;
            eng_l.material.diffuse = vec4(0.4f, 0.42f, 0.45f, 1);
            eng_r.material.diffuse = vec4(0.4f, 0.42f, 0.45f, 1);
            eng_l.name = "EngineL"; eng_r.name = "EngineR";
            obj.volumes.push_back(eng_l); obj.volumes.push_back(eng_r);

            // Cockpit (hemisphere)
            auto cockpit = make_hemisphere(vec3(0, s*0.15f, s*0.4f), s*0.12f, 12);
            cockpit.material.diffuse = vec4(0.3f, 0.6f, 0.9f, 0.8f);
            cockpit.name = "Cockpit";
            obj.volumes.push_back(cockpit);

            // Thrusters (discs, emissive)
            auto thr_l = make_disc(vec3(-s*0.35f, -s*0.1f, -s*0.46f), s*0.06f, s*0.02f, 10);
            auto thr_r = make_disc(vec3(s*0.35f, -s*0.1f, -s*0.46f), s*0.06f, s*0.02f, 10);
            thr_l.material.diffuse = vec4(0.2f, 0.5f, 1.0f, 1); thr_l.material.emission = vec4(0.1f, 0.3f, 0.8f, 1);
            thr_r.material.diffuse = vec4(0.2f, 0.5f, 1.0f, 1); thr_r.material.emission = vec4(0.1f, 0.3f, 0.8f, 1);
            thr_l.name = "ThrusterL"; thr_r.name = "ThrusterR";
            obj.volumes.push_back(thr_l); obj.volumes.push_back(thr_r);

            return obj;
        }
    });

    rules.push_back({"chair", {"chair", "chaise"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Chair";
            float s = sz;

            // Seat (box)
            auto seat = make_box(vec3(0, s*0.45f, 0), vec3(s*0.5f, s*0.04f, s*0.5f));
            seat.noise_amount = 0.03f; seat.seed_offset = seed;
            seat.name = "Seat";
            if (col.w > 0) seat.material.diffuse = col;
            obj.volumes.push_back(seat);

            // Backrest (box)
            auto back = make_box(vec3(0, s*0.75f, -s*0.23f), vec3(s*0.5f, s*0.55f, s*0.04f));
            back.noise_amount = 0.03f; back.seed_offset = seed + 1;
            back.name = "Backrest";
            if (col.w > 0) back.material.diffuse = col;
            obj.volumes.push_back(back);

            // 4 legs (cylinders)
            float lx = s*0.2f, lz = s*0.2f;
            vec3 leg_pos[4] = {vec3(lx,0.22f*s,lz), vec3(-lx,0.22f*s,lz), vec3(lx,0.22f*s,-lz), vec3(-lx,0.22f*s,-lz)};
            for (int i = 0; i < 4; i++) {
                auto leg = make_cylinder(leg_pos[i], s*0.025f, s*0.45f, 8);
                leg.noise_amount = 0.02f; leg.seed_offset = seed + (float)(i + 10);
                leg.name = "Leg" + std::to_string(i);
                if (col.w > 0) leg.material.diffuse = col;
                obj.volumes.push_back(leg);
            }
            return obj;
        }
    });

    rules.push_back({"table", {"table"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Table";
            float s = sz;

            // Top (box)
            auto top = make_box(vec3(0, s*0.5f, 0), vec3(s*1.0f, s*0.05f, s*0.6f));
            top.noise_amount = 0.03f; top.seed_offset = seed;
            top.name = "TableTop";
            if (col.w > 0) top.material.diffuse = col;
            obj.volumes.push_back(top);

            // 4 legs
            float lx = s*0.42f, lz = s*0.24f;
            vec3 leg_pos[4] = {vec3(lx,0.24f*s,lz), vec3(-lx,0.24f*s,lz), vec3(lx,0.24f*s,-lz), vec3(-lx,0.24f*s,-lz)};
            for (int i = 0; i < 4; i++) {
                auto leg = make_cylinder(leg_pos[i], s*0.03f, s*0.48f, 8);
                leg.noise_amount = 0.02f; leg.seed_offset = seed + (float)(i + 10);
                leg.name = "Leg" + std::to_string(i);
                if (col.w > 0) leg.material.diffuse = col;
                obj.volumes.push_back(leg);
            }
            return obj;
        }
    });

    rules.push_back({"donut", {"donut", "donut", "beignet", "ring donut"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Donut";
            float s = sz;

            // Main torus
            auto torus = make_torus(vec3(0, 0, 0), s*0.35f, s*0.15f, 24);
            torus.noise_amount = 0.08f; torus.noise_scale = 3.0f; torus.seed_offset = seed;
            torus.name = "DonutBody";
            torus.material.diffuse = col.w > 0 ? col : vec4(0.85f, 0.55f, 0.3f, 1);
            obj.volumes.push_back(torus);

            // Frosting torus (slightly larger, top half)
            auto frost = make_torus(vec3(0, s*0.03f, 0), s*0.35f, s*0.155f, 24);
            frost.noise_amount = 0.12f; frost.noise_scale = 2.5f; frost.seed_offset = seed + 5;
            frost.name = "Frosting";
            frost.material.diffuse = vec4(0.95f, 0.7f, 0.85f, 1);
            obj.volumes.push_back(frost);

            // Sprinkles (tiny spheres)
            for (int i = 0; i < 12; i++) {
                float angle = (float)M_PI * 2.0f * i / 12.0f;
                float cx = cosf(angle) * s * 0.35f;
                float cz = sinf(angle) * s * 0.35f;
                auto sp = make_sphere(vec3(cx, s*0.12f, cz), s*0.02f, 6);
                sp.material.diffuse = vec4(
                    0.5f + 0.5f * noise2d(seed + (float)i, 0),
                    0.5f + 0.5f * noise2d(seed + (float)i, 1),
                    0.5f + 0.5f * noise2d(seed + (float)i, 2), 1);
                sp.name = "Sprinkle" + std::to_string(i);
                obj.volumes.push_back(sp);
            }
            return obj;
        }
    });

    rules.push_back({"star", {"star", "etoile"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Star";
            float s = sz;

            // Central sphere
            auto center = make_sphere(vec3(0, 0, 0), s*0.25f, 16);
            center.noise_amount = 0.05f; center.seed_offset = seed;
            center.material.diffuse = col.w > 0 ? col : vec4(1.0f, 0.9f, 0.2f, 1);
            center.material.emission = vec4(0.5f, 0.45f, 0.1f, 1);
            center.name = "Center";
            obj.volumes.push_back(center);

            // 5 points (wedges)
            for (int i = 0; i < 5; i++) {
                float angle = (float)M_PI * 2.0f * i / 5.0f - (float)M_PI * 0.5f;
                float px = cosf(angle) * s * 0.3f;
                float py = sinf(angle) * s * 0.3f;
                auto point = make_wedge(vec3(px, py, 0), s*0.08f, s*0.04f, (float)M_PI*2, 12);
                point.noise_amount = 0.05f; point.seed_offset = seed + (float)(i+1)*3;
                point.rotation.z = angle + (float)M_PI * 0.5f;
                point.material.diffuse = vec4(1.0f, 0.85f, 0.1f, 1);
                point.material.emission = vec4(0.4f, 0.35f, 0.05f, 1);
                point.name = "Point" + std::to_string(i);
                obj.volumes.push_back(point);
            }
            return obj;
        }
    });

    rules.push_back({"heart", {"heart", "coeur"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Heart";
            float s = sz;

            // Two upper lobes (spheres)
            auto lobe_l = make_sphere(vec3(-s*0.15f, s*0.1f, 0), s*0.2f, 16);
            auto lobe_r = make_sphere(vec3(s*0.15f, s*0.1f, 0), s*0.2f, 16);
            lobe_l.noise_amount = 0.06f; lobe_r.noise_amount = 0.06f;
            lobe_l.seed_offset = seed; lobe_r.seed_offset = seed + 5;
            lobe_l.material.diffuse = col.w > 0 ? col : vec4(0.9f, 0.1f, 0.15f, 1);
            lobe_r.material.diffuse = col.w > 0 ? col : vec4(0.9f, 0.1f, 0.15f, 1);
            lobe_l.name = "LobeL"; lobe_r.name = "LobeR";
            obj.volumes.push_back(lobe_l); obj.volumes.push_back(lobe_r);

            // Lower cone (bottom point)
            auto bottom = make_cone(vec3(0, -s*0.15f, 0), s*0.2f, s*0.4f, 16);
            bottom.noise_amount = 0.06f; bottom.seed_offset = seed + 10;
            bottom.rotation.x = (float)M_PI;
            bottom.material.diffuse = col.w > 0 ? col : vec4(0.9f, 0.1f, 0.15f, 1);
            bottom.name = "BottomPoint";
            obj.volumes.push_back(bottom);

            return obj;
        }
    });

    rules.push_back({"mushroom", {"mushroom", "champignon"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Mushroom";
            float s = sz;

            // Stem (cylinder, slightly tapered)
            auto stem = make_cylinder(vec3(0, s*0.2f, 0), s*0.08f, s*0.4f, 10);
            stem.noise_amount = 0.06f; stem.noise_scale = 3.0f; stem.seed_offset = seed;
            stem.material.diffuse = vec4(0.9f, 0.88f, 0.82f, 1);
            stem.name = "Stem";
            obj.volumes.push_back(stem);

            // Cap (hemisphere)
            auto cap = make_hemisphere(vec3(0, s*0.5f, 0), s*0.3f, 16);
            cap.noise_amount = 0.1f; cap.noise_scale = 2.5f; cap.seed_offset = seed + 5;
            cap.material.diffuse = col.w > 0 ? col : vec4(0.8f, 0.15f, 0.1f, 1);
            cap.name = "Cap";
            obj.volumes.push_back(cap);

            // Spots on cap (tiny spheres)
            for (int i = 0; i < 6; i++) {
                float angle = (float)M_PI * 2.0f * i / 6.0f + seed * 0.2f;
                float r = s * 0.15f * (0.5f + 0.5f * noise2d(seed + (float)i, 100));
                float x = cosf(angle) * r;
                float z = sinf(angle) * r;
                auto spot = make_sphere(vec3(x, s*0.55f + s*0.08f, z), s*0.035f, 8);
                spot.material.diffuse = vec4(0.95f, 0.93f, 0.88f, 1);
                spot.name = "Spot" + std::to_string(i);
                obj.volumes.push_back(spot);
            }
            return obj;
        }
    });

    rules.push_back({"crystal", {"crystal", "cristal"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Crystal";
            float s = sz;

            // Main crystal (cone)
            auto main = make_cone(vec3(0, s*0.4f, 0), s*0.12f, s*0.8f, 6);
            main.noise_amount = 0.03f; main.noise_scale = 4.0f; main.seed_offset = seed;
            main.material.diffuse = col.w > 0 ? col : vec4(0.4f, 0.6f, 0.95f, 0.85f);
            main.material.metallic = 0.1f; main.material.roughness = 0.05f;
            main.material.opacity = 0.85f;
            main.name = "MainCrystal";
            obj.volumes.push_back(main);

            // Secondary crystals
            for (int i = 0; i < 4; i++) {
                float angle = (float)M_PI * 2.0f * i / 4.0f + seed * 0.3f;
                float dist = s * 0.15f;
                float x = cosf(angle) * dist;
                float z = sinf(angle) * dist;
                float h = s * (0.3f + 0.3f * noise2d(seed + (float)i, 50));
                auto cr = make_cone(vec3(x, h*0.5f, z), s*0.06f, h, 6);
                cr.noise_amount = 0.04f; cr.seed_offset = seed + (float)(i+1)*7;
                cr.rotation.x = (noise2d(seed+(float)i, 60) - 0.5f) * 0.3f;
                cr.rotation.z = (noise2d(seed+(float)i, 70) - 0.5f) * 0.3f;
                cr.material.diffuse = vec4(0.5f, 0.65f, 0.95f, 0.8f);
                cr.material.opacity = 0.8f;
                cr.name = "Crystal" + std::to_string(i);
                obj.volumes.push_back(cr);
            }

            // Base rock
            auto base = make_sphere(vec3(0, -s*0.05f, 0), s*0.18f, 10);
            base.noise_amount = 0.15f; base.noise_scale = 4.0f; base.seed_offset = seed + 50;
            base.scale = vec3(1.5f, 0.4f, 1.5f);
            base.material.diffuse = vec4(0.4f, 0.38f, 0.35f, 1);
            base.name = "Base";
            obj.volumes.push_back(base);

            return obj;
        }
    });

    rules.push_back({"skull", {"skull", "crane", "tete de mort"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Skull";
            float s = sz;

            // Cranium (sphere, slightly elongated)
            auto cranium = make_sphere(vec3(0, s*0.25f, 0), s*0.35f, 20);
            cranium.noise_amount = 0.06f; cranium.noise_scale = 3.0f; cranium.seed_offset = seed;
            cranium.scale = vec3(1.0f, 1.1f, 1.0f);
            cranium.material.diffuse = vec4(0.9f, 0.88f, 0.82f, 1);
            cranium.name = "Cranium";
            obj.volumes.push_back(cranium);

            // Eye sockets (spheres, dark)
            auto eye_l = make_sphere(vec3(-s*0.1f, s*0.3f, s*0.28f), s*0.06f, 10);
            auto eye_r = make_sphere(vec3(s*0.1f, s*0.3f, s*0.28f), s*0.06f, 10);
            eye_l.material.diffuse = vec4(0.05f, 0.05f, 0.05f, 1);
            eye_r.material.diffuse = vec4(0.05f, 0.05f, 0.05f, 1);
            eye_l.name = "EyeSocketL"; eye_r.name = "EyeSocketR";
            obj.volumes.push_back(eye_l); obj.volumes.push_back(eye_r);

            // Nose (cone)
            auto nose = make_cone(vec3(0, s*0.2f, s*0.33f), s*0.03f, s*0.08f, 6);
            nose.material.diffuse = vec4(0.85f, 0.82f, 0.76f, 1);
            nose.name = "Nose";
            obj.volumes.push_back(nose);

            // Jaw (box)
            auto jaw = make_box(vec3(0, s*0.02f, s*0.05f), vec3(s*0.25f, s*0.08f, s*0.2f));
            jaw.noise_amount = 0.04f; jaw.seed_offset = seed + 10;
            jaw.material.diffuse = vec4(0.88f, 0.85f, 0.8f, 1);
            jaw.name = "Jaw";
            obj.volumes.push_back(jaw);

            return obj;
        }
    });

    rules.push_back({"brain", {"brain", "cerveau"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Brain";
            float s = sz;

            // Left hemisphere
            auto left = make_sphere(vec3(-s*0.12f, s*0.1f, 0), s*0.28f, 20);
            left.noise_amount = 0.15f; left.noise_scale = 5.0f; left.seed_offset = seed;
            left.scale = vec3(1.0f, 0.85f, 1.1f);
            left.material.diffuse = vec4(0.85f, 0.55f, 0.55f, 1);
            left.name = "LeftHemi";
            obj.volumes.push_back(left);

            // Right hemisphere
            auto right = make_sphere(vec3(s*0.12f, s*0.1f, 0), s*0.28f, 20);
            right.noise_amount = 0.15f; right.noise_scale = 5.0f; right.seed_offset = seed + 5;
            right.scale = vec3(1.0f, 0.85f, 1.1f);
            right.material.diffuse = vec4(0.85f, 0.55f, 0.55f, 1);
            right.name = "RightHemi";
            obj.volumes.push_back(right);

            // Brain stem
            auto stem = make_cylinder(vec3(0, -s*0.1f, -s*0.1f), s*0.06f, s*0.15f, 8);
            stem.noise_amount = 0.08f; stem.seed_offset = seed + 10;
            stem.material.diffuse = vec4(0.8f, 0.5f, 0.5f, 1);
            stem.name = "BrainStem";
            obj.volumes.push_back(stem);

            return obj;
        }
    });

    rules.push_back({"flame", {"flame", "flamme", "fire", "feu"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Flame";
            float s = sz;

            // Core (sphere, emissive)
            auto core = make_sphere(vec3(0, s*0.2f, 0), s*0.1f, 12);
            core.noise_amount = 0.2f; core.noise_scale = 4.0f; core.seed_offset = seed;
            core.material.diffuse = vec4(1.0f, 0.9f, 0.3f, 1);
            core.material.emission = vec4(1.0f, 0.7f, 0.1f, 1);
            core.name = "Core";
            obj.volumes.push_back(core);

            // Outer flame (cone)
            auto outer = make_cone(vec3(0, s*0.35f, 0), s*0.15f, s*0.6f, 12);
            outer.noise_amount = 0.25f; outer.noise_scale = 3.0f; outer.seed_offset = seed + 5;
            outer.material.diffuse = vec4(1.0f, 0.4f, 0.05f, 1);
            outer.material.emission = vec4(0.8f, 0.3f, 0.0f, 1);
            outer.material.opacity = 0.85f;
            outer.name = "OuterFlame";
            obj.volumes.push_back(outer);

            // Tip (small sphere)
            auto tip = make_sphere(vec3(0, s*0.55f, 0), s*0.04f, 8);
            tip.noise_amount = 0.3f; tip.seed_offset = seed + 10;
            tip.material.diffuse = vec4(1.0f, 0.95f, 0.6f, 1);
            tip.material.emission = vec4(1.0f, 0.8f, 0.2f, 1);
            tip.material.opacity = 0.7f;
            tip.name = "Tip";
            obj.volumes.push_back(tip);

            return obj;
        }
    });

    rules.push_back({"cloud", {"cloud", "nuage"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Cloud";
            float s = sz;

            // Multiple spheres
            for (int i = 0; i < 7; i++) {
                float x = fbm3d((float)i*3.0f+seed, 0, 0, 3) * s * 0.5f;
                float y = fbm3d(0, (float)i*3.0f+seed, 0, 3) * s * 0.1f + s*0.1f;
                float z = fbm3d(0, 0, (float)i*3.0f+seed, 3) * s * 0.3f;
                float r = s * (0.2f + 0.15f * noise2d(seed + (float)i, 0));
                auto puff = make_sphere(vec3(x, y, z), r, 12);
                puff.noise_amount = 0.15f; puff.noise_scale = 2.5f;
                puff.seed_offset = seed + (float)(i+1)*5;
                puff.material.diffuse = vec4(0.95f, 0.96f, 0.98f, 1);
                puff.name = "Puff" + std::to_string(i);
                obj.volumes.push_back(puff);
            }
            return obj;
        }
    });

    rules.push_back({"mountain", {"mountain", "montagne"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Mountain";
            float s = sz * 2.0f;

            auto peak = make_cone(vec3(0, s*0.5f, 0), s*0.8f, s*1.0f, 20);
            peak.noise_amount = 0.12f; peak.noise_scale = 3.0f; peak.seed_offset = seed;
            peak.material.diffuse = vec4(0.45f, 0.42f, 0.38f, 1);
            peak.name = "Peak";
            obj.volumes.push_back(peak);

            auto snow = make_hemisphere(vec3(0, s*0.9f, 0), s*0.2f, 12);
            snow.noise_amount = 0.1f; snow.seed_offset = seed + 5;
            snow.material.diffuse = vec4(0.95f, 0.96f, 0.98f, 1);
            snow.name = "SnowCap";
            obj.volumes.push_back(snow);

            auto sec = make_cone(vec3(s*0.3f, s*0.3f, s*0.2f), s*0.4f, s*0.6f, 14);
            sec.noise_amount = 0.1f; sec.seed_offset = seed + 10;
            sec.material.diffuse = vec4(0.48f, 0.44f, 0.4f, 1);
            sec.name = "SecondaryPeak";
            obj.volumes.push_back(sec);

            auto base = make_disc(vec3(0, -s*0.05f, 0), s*1.2f, s*0.1f, 20);
            base.noise_amount = 0.08f; base.seed_offset = seed + 15;
            base.material.diffuse = vec4(0.35f, 0.33f, 0.28f, 1);
            base.name = "Base";
            obj.volumes.push_back(base);

            return obj;
        }
    });

    // ============================================================
    // ADDITIONAL DECOMPOSITION RULES — missing scene types
    // ============================================================

    rules.push_back({"snowman", {"snowman", "bonhomme"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Snowman";
            float s = sz; vec4 w(0.95f, 0.96f, 0.98f, 1);
            auto body = make_sphere(vec3(0, s*0.2f, 0), s*0.35f, 16);
            body.noise_amount = 0.03f; body.seed_offset = seed; body.material.diffuse = w; body.name = "Body";
            obj.volumes.push_back(body);
            auto mid = make_sphere(vec3(0, s*0.65f, 0), s*0.25f, 16);
            mid.noise_amount = 0.03f; mid.seed_offset = seed+1; mid.material.diffuse = w; mid.name = "Mid";
            obj.volumes.push_back(mid);
            auto head = make_sphere(vec3(0, s*1.0f, 0), s*0.18f, 14);
            head.noise_amount = 0.03f; head.seed_offset = seed+2; head.material.diffuse = w; head.name = "Head";
            obj.volumes.push_back(head);
            auto nose = make_cone(vec3(0, s*1.0f, s*0.18f), s*0.03f, s*0.15f, 8);
            nose.rotation.x = (float)M_PI*0.5f; nose.material.diffuse = vec4(1,0.5f,0,1); nose.name = "Nose";
            obj.volumes.push_back(nose);
            for (int i = 0; i < 2; i++) {
                float side = i == 0 ? -1.0f : 1.0f;
                auto eye = make_sphere(vec3(side*s*0.06f, s*1.05f, s*0.16f), s*0.025f, 8);
                eye.material.diffuse = vec4(0.05f,0.05f,0.05f,1); eye.name = "Eye" + std::to_string(i);
                obj.volumes.push_back(eye);
            }
            auto hat_brim = make_disc(vec3(0, s*1.18f, 0), s*0.22f, s*0.02f, 12);
            hat_brim.material.diffuse = vec4(0.15f,0.1f,0.08f,1); hat_brim.name = "HatBrim";
            obj.volumes.push_back(hat_brim);
            auto hat_top = make_cylinder(vec3(0, s*1.32f, 0), s*0.14f, s*0.15f, 10);
            hat_top.material.diffuse = vec4(0.15f,0.1f,0.08f,1); hat_top.name = "HatTop";
            obj.volumes.push_back(hat_top);
            auto scarf = make_torus(vec3(0, s*0.82f, 0), s*0.18f, s*0.03f, 16);
            scarf.material.diffuse = vec4(0.8f,0.1f,0.1f,1); scarf.name = "Scarf";
            obj.volumes.push_back(scarf);
            return obj;
        }
    });

    rules.push_back({"diamond", {"diamond", "diamant"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Diamond";
            float s = sz;
            auto top = make_cone(vec3(0, s*0.15f, 0), s*0.25f, s*0.35f, 8);
            top.noise_amount = 0.02f; top.seed_offset = seed;
            top.material.diffuse = col.w > 0 ? col : vec4(0.7f, 0.85f, 1.0f, 0.85f);
            top.material.metallic = 0.1f; top.material.roughness = 0.02f;
            top.name = "Top";
            obj.volumes.push_back(top);
            auto bot = make_cone(vec3(0, -s*0.1f, 0), s*0.25f, s*0.25f, 8);
            bot.noise_amount = 0.02f; bot.seed_offset = seed+5;
            bot.rotation.x = (float)M_PI;
            bot.material.diffuse = col.w > 0 ? col : vec4(0.7f, 0.85f, 1.0f, 0.85f);
            bot.material.metallic = 0.1f; bot.material.roughness = 0.02f;
            bot.name = "Bottom";
            obj.volumes.push_back(bot);
            auto girdle = make_ring(vec3(0, 0, 0), s*0.22f, s*0.25f, 8);
            girdle.noise_amount = 0.01f; girdle.seed_offset = seed+10;
            girdle.material.diffuse = vec4(0.8f, 0.9f, 1.0f, 0.9f);
            girdle.name = "Girdle";
            obj.volumes.push_back(girdle);
            return obj;
        }
    });

    rules.push_back({"spiral", {"spiral", "spirale"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Spiral";
            float s = sz;
            for (int i = 0; i < 20; i++) {
                float t = (float)i / 20.0f;
                float angle = t * (float)M_PI * 6.0f;
                float r = s * 0.1f + t * s * 0.3f;
                float x = cosf(angle) * r;
                float z = sinf(angle) * r;
                float y = t * s * 0.8f - s*0.3f;
                float sp = s * 0.04f * (1.0f - t * 0.5f);
                auto bead = make_sphere(vec3(x, y, z), sp, 8);
                bead.noise_amount = 0.05f; bead.seed_offset = seed + (float)i;
                bead.material.diffuse = col.w > 0 ? col : vec4(0.5f + 0.5f*noise2d(seed+(float)i,0), 0.5f+0.5f*noise2d(seed+(float)i,1), 0.5f+0.5f*noise2d(seed+(float)i,2), 1);
                bead.name = "Bead" + std::to_string(i);
                obj.volumes.push_back(bead);
            }
            return obj;
        }
    });

    rules.push_back({"dna", {"dna", "adn", "helix"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "DNA";
            float s = sz;
            for (int strand = 0; strand < 2; strand++) {
                float offset = strand * (float)M_PI;
                for (int i = 0; i < 16; i++) {
                    float t = (float)i / 16.0f;
                    float angle = t * (float)M_PI * 4.0f + offset;
                    float x = cosf(angle) * s * 0.2f;
                    float z = sinf(angle) * s * 0.2f;
                    float y = t * s * 1.2f - s*0.6f;
                    auto bead = make_sphere(vec3(x, y, z), s*0.035f, 8);
                    bead.material.diffuse = strand == 0 ? vec4(0.2f,0.4f,0.9f,1) : vec4(0.9f,0.2f,0.2f,1);
                    bead.seed_offset = seed + (float)(strand*16+i);
                    bead.name = "Strand" + std::to_string(strand) + "_" + std::to_string(i);
                    obj.volumes.push_back(bead);
                }
            }
            for (int i = 0; i < 8; i++) {
                float t = (float)i / 8.0f;
                float angle = t * (float)M_PI * 4.0f;
                float x1 = cosf(angle) * s * 0.2f;
                float z1 = sinf(angle) * s * 0.2f;
                float x2 = cosf(angle + (float)M_PI) * s * 0.2f;
                float z2 = sinf(angle + (float)M_PI) * s * 0.2f;
                float y = t * s * 1.2f - s*0.6f;
                float dx = x2-x1, dz = z2-z1;
                float len = sqrtf(dx*dx+dz*dz);
                auto rung = make_cylinder(vec3((x1+x2)*0.5f, y, (z1+z2)*0.5f), s*0.015f, len, 6);
                rung.rotation.y = atan2f(dz, dx);
                rung.rotation.z = (float)M_PI*0.5f;
                rung.material.diffuse = vec4(0.8f,0.8f,0.3f,1);
                rung.seed_offset = seed + 100 + (float)i;
                rung.name = "Rung" + std::to_string(i);
                obj.volumes.push_back(rung);
            }
            return obj;
        }
    });

    rules.push_back({"airplane", {"airplane", "avion", "plane", "jet"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Airplane";
            float s = sz * 1.5f;
            auto fuselage = make_cylinder(vec3(0, 0, 0), s*0.12f, s*1.2f, 14);
            fuselage.noise_amount = 0.03f; fuselage.seed_offset = seed;
            fuselage.rotation.z = (float)M_PI*0.5f;
            fuselage.material.diffuse = col.w > 0 ? col : vec4(0.85f,0.87f,0.9f,1);
            fuselage.name = "Fuselage";
            obj.volumes.push_back(fuselage);
            auto nose = make_cone(vec3(s*0.65f, 0, 0), s*0.12f, s*0.2f, 12);
            nose.rotation.z = -(float)M_PI*0.5f;
            nose.material.diffuse = vec4(0.7f,0.72f,0.75f,1); nose.name = "Nose";
            obj.volumes.push_back(nose);
            auto wing_l = make_box(vec3(0, -s*0.02f, -s*0.35f), vec3(s*0.5f, s*0.02f, s*0.3f));
            wing_l.noise_amount = 0.02f; wing_l.seed_offset = seed+5;
            wing_l.material.diffuse = vec4(0.8f,0.82f,0.85f,1); wing_l.name = "WingL";
            obj.volumes.push_back(wing_l);
            auto wing_r = make_box(vec3(0, -s*0.02f, s*0.35f), vec3(s*0.5f, s*0.02f, s*0.3f));
            wing_r.noise_amount = 0.02f; wing_r.seed_offset = seed+6;
            wing_r.material.diffuse = vec4(0.8f,0.82f,0.85f,1); wing_r.name = "WingR";
            obj.volumes.push_back(wing_r);
            auto tail_v = make_box(vec3(-s*0.55f, s*0.12f, 0), vec3(s*0.15f, s*0.15f, s*0.02f));
            tail_v.material.diffuse = vec4(0.8f,0.82f,0.85f,1); tail_v.name = "TailV";
            obj.volumes.push_back(tail_v);
            auto tail_h = make_box(vec3(-s*0.55f, 0, 0), vec3(s*0.1f, s*0.02f, s*0.25f));
            tail_h.material.diffuse = vec4(0.8f,0.82f,0.85f,1); tail_h.name = "TailH";
            obj.volumes.push_back(tail_h);
            auto eng_l = make_cylinder(vec3(-s*0.1f, -s*0.04f, -s*0.25f), s*0.04f, s*0.15f, 8);
            eng_l.material.diffuse = vec4(0.5f,0.52f,0.55f,1); eng_l.name = "EngineL";
            obj.volumes.push_back(eng_l);
            auto eng_r = make_cylinder(vec3(-s*0.1f, -s*0.04f, s*0.25f), s*0.04f, s*0.15f, 8);
            eng_r.material.diffuse = vec4(0.5f,0.52f,0.55f,1); eng_r.name = "EngineR";
            obj.volumes.push_back(eng_r);
            return obj;
        }
    });

    rules.push_back({"boat", {"boat", "bateau", "ship", "voilier"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Boat";
            float s = sz;
            auto hull = make_box(vec3(0, 0, 0), vec3(s*1.2f, s*0.25f, s*0.5f));
            hull.noise_amount = 0.04f; hull.seed_offset = seed;
            hull.material.diffuse = col.w > 0 ? col : vec4(0.45f,0.3f,0.15f,1); hull.name = "Hull";
            obj.volumes.push_back(hull);
            auto bow = make_cone(vec3(s*0.65f, s*0.05f, 0), s*0.15f, s*0.3f, 8);
            bow.rotation.z = -(float)M_PI*0.5f;
            bow.material.diffuse = vec4(0.45f,0.3f,0.15f,1); bow.name = "Bow";
            obj.volumes.push_back(bow);
            auto mast = make_cylinder(vec3(0, s*0.6f, 0), s*0.02f, s*1.0f, 8);
            mast.material.diffuse = vec4(0.4f,0.28f,0.12f,1); mast.name = "Mast";
            obj.volumes.push_back(mast);
            auto sail = make_box(vec3(s*0.1f, s*0.6f, 0), vec3(s*0.4f, s*0.5f, s*0.01f));
            sail.noise_amount = 0.08f; sail.seed_offset = seed+10;
            sail.material.diffuse = vec4(0.95f,0.93f,0.88f,1); sail.name = "Sail";
            obj.volumes.push_back(sail);
            return obj;
        }
    });

    rules.push_back({"door", {"door", "porte"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Door";
            float s = sz;
            auto frame_l = make_box(vec3(-s*0.35f, s*0.5f, 0), vec3(s*0.06f, s*1.0f, s*0.06f));
            frame_l.material.diffuse = vec4(0.35f,0.22f,0.1f,1); frame_l.name = "FrameL";
            obj.volumes.push_back(frame_l);
            auto frame_r = make_box(vec3(s*0.35f, s*0.5f, 0), vec3(s*0.06f, s*1.0f, s*0.06f));
            frame_r.material.diffuse = vec4(0.35f,0.22f,0.1f,1); frame_r.name = "FrameR";
            obj.volumes.push_back(frame_r);
            auto frame_t = make_box(vec3(0, s*1.02f, 0), vec3(s*0.76f, s*0.06f, s*0.06f));
            frame_t.material.diffuse = vec4(0.35f,0.22f,0.1f,1); frame_t.name = "FrameT";
            obj.volumes.push_back(frame_t);
            auto panel = make_box(vec3(0, s*0.48f, s*0.02f), vec3(s*0.62f, s*0.92f, s*0.04f));
            panel.noise_amount = 0.03f; panel.seed_offset = seed;
            panel.material.diffuse = col.w > 0 ? col : vec4(0.5f,0.32f,0.15f,1); panel.name = "Panel";
            obj.volumes.push_back(panel);
            auto handle = make_sphere(vec3(s*0.22f, s*0.48f, s*0.06f), s*0.03f, 8);
            handle.material.diffuse = vec4(0.8f,0.7f,0.1f,1); handle.material.metallic = 0.9f;
            handle.name = "Handle";
            obj.volumes.push_back(handle);
            return obj;
        }
    });

    rules.push_back({"window", {"window", "fenetre"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Window";
            float s = sz;
            auto frame = make_box(vec3(0, s*0.4f, 0), vec3(s*0.8f, s*0.8f, s*0.06f));
            frame.material.diffuse = vec4(0.35f,0.22f,0.1f,1); frame.name = "Frame";
            obj.volumes.push_back(frame);
            auto glass = make_box(vec3(0, s*0.4f, s*0.01f), vec3(s*0.68f, s*0.68f, s*0.02f));
            glass.material.diffuse = vec4(0.6f,0.8f,1.0f,0.5f); glass.name = "Glass";
            obj.volumes.push_back(glass);
            auto bar_h = make_box(vec3(0, s*0.4f, s*0.02f), vec3(s*0.68f, s*0.03f, s*0.03f));
            bar_h.material.diffuse = vec4(0.35f,0.22f,0.1f,1); bar_h.name = "BarH";
            obj.volumes.push_back(bar_h);
            auto bar_v = make_box(vec3(0, s*0.4f, s*0.02f), vec3(s*0.03f, s*0.68f, s*0.03f));
            bar_v.material.diffuse = vec4(0.35f,0.22f,0.1f,1); bar_v.name = "BarV";
            obj.volumes.push_back(bar_v);
            return obj;
        }
    });

    rules.push_back({"shelf", {"shelf", "etagere"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Shelf";
            float s = sz;
            for (int i = 0; i < 3; i++) {
                float y = (float)i * s * 0.35f;
                auto plank = make_box(vec3(0, y, 0), vec3(s*0.8f, s*0.03f, s*0.25f));
                plank.noise_amount = 0.03f; plank.seed_offset = seed + (float)i;
                plank.material.diffuse = col.w > 0 ? col : vec4(0.5f,0.32f,0.15f,1);
                plank.name = "Plank" + std::to_string(i);
                obj.volumes.push_back(plank);
            }
            auto side_l = make_box(vec3(-s*0.4f, s*0.35f, 0), vec3(s*0.03f, s*0.73f, s*0.25f));
            side_l.material.diffuse = vec4(0.45f,0.28f,0.12f,1); side_l.name = "SideL";
            obj.volumes.push_back(side_l);
            auto side_r = make_box(vec3(s*0.4f, s*0.35f, 0), vec3(s*0.03f, s*0.73f, s*0.25f));
            side_r.material.diffuse = vec4(0.45f,0.28f,0.12f,1); side_r.name = "SideR";
            obj.volumes.push_back(side_r);
            return obj;
        }
    });

    rules.push_back({"lamp", {"lamp", "lumiere", "lampe", "light"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Lamp";
            float s = sz;
            auto base = make_disc(vec3(0, 0, 0), s*0.2f, s*0.04f, 12);
            base.material.diffuse = vec4(0.3f,0.3f,0.32f,1); base.material.metallic = 0.8f; base.name = "Base";
            obj.volumes.push_back(base);
            auto pole = make_cylinder(vec3(0, s*0.4f, 0), s*0.025f, s*0.7f, 8);
            pole.material.diffuse = vec4(0.4f,0.4f,0.42f,1); pole.material.metallic = 0.7f; pole.name = "Pole";
            obj.volumes.push_back(pole);
            auto shade = make_cone(vec3(0, s*0.85f, 0), s*0.2f, s*0.25f, 16);
            shade.noise_amount = 0.03f; shade.seed_offset = seed;
            shade.material.diffuse = col.w > 0 ? col : vec4(0.9f,0.85f,0.7f,1);
            shade.name = "Shade";
            obj.volumes.push_back(shade);
            auto bulb = make_sphere(vec3(0, s*0.78f, 0), s*0.04f, 8);
            bulb.material.diffuse = vec4(1,0.95f,0.7f,1); bulb.material.emission = vec4(0.8f,0.75f,0.3f,1);
            bulb.name = "Bulb";
            obj.volumes.push_back(bulb);
            return obj;
        }
    });

    rules.push_back({"flower", {"flower", "fleur", "rose"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Flower";
            float s = sz;
            auto stem = make_cylinder(vec3(0, s*0.15f, 0), s*0.015f, s*0.6f, 8);
            stem.noise_amount = 0.06f; stem.seed_offset = seed;
            stem.material.diffuse = vec4(0.2f,0.55f,0.15f,1); stem.name = "Stem";
            obj.volumes.push_back(stem);
            auto center = make_sphere(vec3(0, s*0.5f, 0), s*0.06f, 10);
            center.material.diffuse = vec4(1.0f,0.85f,0.1f,1); center.name = "Center";
            obj.volumes.push_back(center);
            vec4 petal_col = col.w > 0 ? col : vec4(0.9f,0.2f,0.35f,1);
            for (int i = 0; i < 6; i++) {
                float angle = (float)M_PI * 2.0f * i / 6.0f;
                float px = cosf(angle) * s * 0.1f;
                float pz = sinf(angle) * s * 0.1f;
                auto petal = make_sphere(vec3(px, s*0.5f, pz), s*0.05f, 8);
                petal.scale = vec3(1.0f, 0.5f, 1.5f);
                petal.rotation.y = angle;
                petal.material.diffuse = petal_col;
                petal.seed_offset = seed + (float)(i+1)*3;
                petal.name = "Petal" + std::to_string(i);
                obj.volumes.push_back(petal);
            }
            auto leaf = make_disc(vec3(s*0.08f, s*0.1f, 0), s*0.08f, s*0.01f, 8);
            leaf.rotation.z = 0.4f;
            leaf.material.diffuse = vec4(0.15f,0.5f,0.1f,1); leaf.name = "Leaf";
            obj.volumes.push_back(leaf);
            return obj;
        }
    });

    rules.push_back({"bone", {"bone", "os"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Bone";
            float s = sz;
            auto shaft = make_cylinder(vec3(0, 0, 0), s*0.04f, s*0.6f, 8);
            shaft.noise_amount = 0.03f; shaft.seed_offset = seed;
            shaft.material.diffuse = vec4(0.92f,0.9f,0.82f,1); shaft.name = "Shaft";
            obj.volumes.push_back(shaft);
            for (int i = 0; i < 2; i++) {
                float side = i == 0 ? -1.0f : 1.0f;
                auto knob1 = make_sphere(vec3(side*s*0.32f, s*0.03f, 0), s*0.05f, 8);
                knob1.material.diffuse = vec4(0.92f,0.9f,0.82f,1); knob1.name = "Knob" + std::to_string(i*2);
                obj.volumes.push_back(knob1);
                auto knob2 = make_sphere(vec3(side*s*0.32f, -s*0.03f, 0), s*0.05f, 8);
                knob2.material.diffuse = vec4(0.92f,0.9f,0.82f,1); knob2.name = "Knob" + std::to_string(i*2+1);
                obj.volumes.push_back(knob2);
            }
            return obj;
        }
    });

    rules.push_back({"key", {"key", "cle"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Key";
            float s = sz;
            auto shaft = make_box(vec3(s*0.15f, 0, 0), vec3(s*0.5f, s*0.03f, s*0.02f));
            shaft.material.diffuse = vec4(0.8f,0.7f,0.1f,1); shaft.material.metallic = 0.9f; shaft.name = "Shaft";
            obj.volumes.push_back(shaft);
            auto bow = make_torus(vec3(-s*0.1f, 0, 0), s*0.1f, s*0.02f, 12);
            bow.material.diffuse = vec4(0.8f,0.7f,0.1f,1); bow.material.metallic = 0.9f; bow.name = "Bow";
            obj.volumes.push_back(bow);
            for (int i = 0; i < 3; i++) {
                float x = s*0.3f + (float)i * s*0.08f;
                auto tooth = make_box(vec3(x, -s*0.04f, 0), vec3(s*0.02f, s*0.05f, s*0.02f));
                tooth.material.diffuse = vec4(0.8f,0.7f,0.1f,1); tooth.material.metallic = 0.9f;
                tooth.name = "Tooth" + std::to_string(i);
                obj.volumes.push_back(tooth);
            }
            return obj;
        }
    });

    rules.push_back({"helmet", {"helmet", "casque"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Helmet";
            float s = sz;
            auto dome = make_hemisphere(vec3(0, s*0.15f, 0), s*0.35f, 18);
            dome.noise_amount = 0.04f; dome.seed_offset = seed;
            dome.material.diffuse = col.w > 0 ? col : vec4(0.5f,0.52f,0.55f,1);
            dome.material.metallic = 0.7f; dome.name = "Dome";
            obj.volumes.push_back(dome);
            auto rim = make_ring(vec3(0, 0, 0), s*0.32f, s*0.38f, 16);
            rim.material.diffuse = vec4(0.45f,0.47f,0.5f,1); rim.material.metallic = 0.8f; rim.name = "Rim";
            obj.volumes.push_back(rim);
            auto visor = make_disc(vec3(0, s*0.15f, s*0.28f), s*0.18f, s*0.01f, 12);
            visor.rotation.x = 0.3f;
            visor.material.diffuse = vec4(0.3f,0.5f,0.8f,0.6f); visor.name = "Visor";
            obj.volumes.push_back(visor);
            return obj;
        }
    });

    rules.push_back({"battery", {"battery", "pile", "batterie"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Battery";
            float s = sz;
            auto body = make_cylinder(vec3(0, 0, 0), s*0.12f, s*0.5f, 12);
            body.noise_amount = 0.02f; body.seed_offset = seed;
            body.material.diffuse = col.w > 0 ? col : vec4(0.2f,0.6f,0.2f,1); body.name = "Body";
            obj.volumes.push_back(body);
            auto terminal = make_cylinder(vec3(0, s*0.28f, 0), s*0.04f, s*0.06f, 8);
            terminal.material.diffuse = vec4(0.8f,0.8f,0.82f,1); terminal.material.metallic = 0.9f;
            terminal.name = "Terminal";
            obj.volumes.push_back(terminal);
            auto stripe = make_ring(vec3(0, 0, 0), s*0.121f, s*0.13f, 12);
            stripe.position.y = s*0.1f;
            stripe.material.diffuse = vec4(0.1f,0.1f,0.1f,1); stripe.name = "Stripe";
            obj.volumes.push_back(stripe);
            return obj;
        }
    });

    rules.push_back({"ice", {"ice", "cristal", "glacier", "glace", "crystal"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Ice";
            float s = sz;
            for (int i = 0; i < 5; i++) {
                float x = fbm3d((float)i*5.0f+seed, 0, 0, 3) * s*0.2f;
                float z = fbm3d(0, (float)i*5.0f+seed, 0, 3) * s*0.2f;
                float h = s * (0.2f + 0.4f * noise2d(seed+(float)i, 200));
                auto shard = make_cone(vec3(x, h*0.5f, z), s*0.05f + s*0.03f*noise2d(seed+(float)i,210), h, 6);
                shard.noise_amount = 0.04f; shard.seed_offset = seed + (float)(i+1)*7;
                shard.rotation.x = (noise2d(seed+(float)i,220)-0.5f)*0.3f;
                shard.rotation.z = (noise2d(seed+(float)i,230)-0.5f)*0.3f;
                shard.material.diffuse = vec4(0.6f,0.85f,1.0f,0.7f);
                shard.material.roughness = 0.05f; shard.material.opacity = 0.7f;
                shard.name = "Shard" + std::to_string(i);
                obj.volumes.push_back(shard);
            }
            return obj;
        }
    });

    rules.push_back({"pyramid", {"pyramide", "pyramid"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Pyramid";
            float s = sz;
            auto main = make_cone(vec3(0, s*0.4f, 0), s*0.7f, s*0.8f, 4);
            main.noise_amount = 0.04f; main.seed_offset = seed; main.rotation.y = (float)M_PI*0.25f;
            main.material.diffuse = col.w > 0 ? col : vec4(0.85f,0.78f,0.55f,1); main.name = "Main";
            obj.volumes.push_back(main);
            auto base = make_box(vec3(0, -s*0.01f, 0), vec3(s*1.42f, s*0.04f, s*1.42f));
            base.material.diffuse = vec4(0.75f,0.68f,0.45f,1); base.name = "Base";
            obj.volumes.push_back(base);
            auto cap = make_sphere(vec3(0, s*0.82f, 0), s*0.03f, 8);
            cap.material.diffuse = vec4(1.0f,0.84f,0.0f,1); cap.material.metallic = 0.9f;
            cap.name = "Cap";
            obj.volumes.push_back(cap);
            return obj;
        }
    });

    rules.push_back({"satellite", {"satellite", "sputnik"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Satellite";
            float s = sz;
            auto body = make_box(vec3(0, 0, 0), vec3(s*0.3f, s*0.25f, s*0.25f));
            body.noise_amount = 0.02f; body.seed_offset = seed;
            body.material.diffuse = vec4(0.7f,0.72f,0.75f,1); body.material.metallic = 0.6f; body.name = "Body";
            obj.volumes.push_back(body);
            auto panel_l = make_box(vec3(-s*0.5f, 0, 0), vec3(s*0.5f, s*0.02f, s*0.3f));
            panel_l.material.diffuse = vec4(0.2f,0.3f,0.8f,1); panel_l.name = "PanelL";
            obj.volumes.push_back(panel_l);
            auto panel_r = make_box(vec3(s*0.5f, 0, 0), vec3(s*0.5f, s*0.02f, s*0.3f));
            panel_r.material.diffuse = vec4(0.2f,0.3f,0.8f,1); panel_r.name = "PanelR";
            obj.volumes.push_back(panel_r);
            auto dish = make_hemisphere(vec3(0, s*0.2f, s*0.15f), s*0.1f, 10);
            dish.material.diffuse = vec4(0.8f,0.8f,0.82f,1); dish.name = "Dish";
            obj.volumes.push_back(dish);
            auto antenna = make_cylinder(vec3(0, s*0.3f, s*0.15f), s*0.008f, s*0.15f, 6);
            antenna.material.diffuse = vec4(0.6f,0.6f,0.62f,1); antenna.name = "Antenna";
            obj.volumes.push_back(antenna);
            return obj;
        }
    });

    rules.push_back({"jewelry", {"jewelry", "bijou", "bague", "necklace"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Jewelry";
            float s = sz;
            auto ring = make_torus(vec3(0, 0, 0), s*0.2f, s*0.025f, 20);
            ring.noise_amount = 0.01f; ring.seed_offset = seed;
            ring.material.diffuse = vec4(1.0f,0.84f,0.0f,1); ring.material.metallic = 0.95f;
            ring.material.roughness = 0.1f; ring.name = "Band";
            obj.volumes.push_back(ring);
            auto gem = make_sphere(vec3(0, s*0.2f, 0), s*0.06f, 12);
            gem.material.diffuse = col.w > 0 ? col : vec4(0.2f,0.5f,0.9f,0.85f);
            gem.material.roughness = 0.02f; gem.name = "Gem";
            obj.volumes.push_back(gem);
            auto setting = make_cylinder(vec3(0, s*0.1f, 0), s*0.03f, s*0.06f, 6);
            setting.material.diffuse = vec4(1.0f,0.84f,0.0f,1); setting.material.metallic = 0.95f;
            setting.name = "Setting";
            obj.volumes.push_back(setting);
            return obj;
        }
    });

    rules.push_back({"throne", {"throne", "trone"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Throne";
            float s = sz;
            auto seat = make_box(vec3(0, s*0.45f, 0), vec3(s*0.6f, s*0.05f, s*0.5f));
            seat.noise_amount = 0.03f; seat.seed_offset = seed;
            seat.material.diffuse = vec4(0.6f,0.1f,0.1f,1); seat.name = "Seat";
            obj.volumes.push_back(seat);
            auto back = make_box(vec3(0, s*0.9f, -s*0.23f), vec3(s*0.6f, s*0.9f, s*0.05f));
            back.noise_amount = 0.03f; back.seed_offset = seed+1;
            back.material.diffuse = vec4(0.6f,0.1f,0.1f,1); back.name = "Back";
            obj.volumes.push_back(back);
            auto crown = make_cone(vec3(0, s*1.45f, -s*0.23f), s*0.35f, s*0.15f, 6);
            crown.material.diffuse = vec4(1.0f,0.84f,0.0f,1); crown.material.metallic = 0.9f; crown.name = "Crown";
            obj.volumes.push_back(crown);
            for (int i = 0; i < 2; i++) {
                float side = i == 0 ? -1.0f : 1.0f;
                auto arm = make_box(vec3(side*s*0.35f, s*0.6f, 0), vec3(s*0.05f, s*0.2f, s*0.45f));
                arm.material.diffuse = vec4(0.55f,0.08f,0.08f,1); arm.name = "Arm" + std::to_string(i);
                obj.volumes.push_back(arm);
                auto knob = make_sphere(vec3(side*s*0.35f, s*0.72f, s*0.22f), s*0.04f, 8);
                knob.material.diffuse = vec4(1.0f,0.84f,0.0f,1); knob.material.metallic = 0.9f;
                knob.name = "ArmKnob" + std::to_string(i);
                obj.volumes.push_back(knob);
            }
            for (int i = 0; i < 4; i++) {
                float lx = (i%2 == 0 ? -1.0f : 1.0f) * s*0.25f;
                float lz = (i < 2 ? -1.0f : 1.0f) * s*0.18f;
                auto leg = make_cylinder(vec3(lx, s*0.22f, lz), s*0.035f, s*0.45f, 8);
                leg.material.diffuse = vec4(0.5f,0.35f,0.1f,1); leg.name = "Leg" + std::to_string(i);
                obj.volumes.push_back(leg);
            }
            return obj;
        }
    });

    rules.push_back({"altar", {"altar", "autel"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Altar";
            float s = sz;
            auto base = make_box(vec3(0, s*0.2f, 0), vec3(s*0.8f, s*0.4f, s*0.4f));
            base.noise_amount = 0.04f; base.seed_offset = seed;
            base.material.diffuse = vec4(0.55f,0.52f,0.48f,1); base.name = "Base";
            obj.volumes.push_back(base);
            auto top = make_box(vec3(0, s*0.42f, 0), vec3(s*0.85f, s*0.04f, s*0.45f));
            top.material.diffuse = vec4(0.6f,0.57f,0.53f,1); top.name = "Top";
            obj.volumes.push_back(top);
            for (int i = 0; i < 2; i++) {
                float side = i == 0 ? -1.0f : 1.0f;
                auto candle = make_cylinder(vec3(side*s*0.3f, s*0.55f, 0), s*0.02f, s*0.15f, 8);
                candle.material.diffuse = vec4(0.9f,0.88f,0.8f,1); candle.name = "Candle" + std::to_string(i);
                obj.volumes.push_back(candle);
                auto flame = make_cone(vec3(side*s*0.3f, s*0.65f, 0), s*0.015f, s*0.05f, 6);
                flame.material.diffuse = vec4(1,0.8f,0.1f,1); flame.material.emission = vec4(0.8f,0.6f,0.05f,1);
                flame.name = "Flame" + std::to_string(i);
                obj.volumes.push_back(flame);
            }
            return obj;
        }
    });

    rules.push_back({"campfire", {"campfire", "feu de camp", "bonfire"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Campfire";
            float s = sz;
            for (int i = 0; i < 4; i++) {
                float angle = (float)M_PI * 2.0f * i / 4.0f + seed * 0.1f;
                auto log = make_cylinder(vec3(cosf(angle)*s*0.12f, s*0.04f, sinf(angle)*s*0.12f), s*0.03f, s*0.35f, 8);
                log.rotation.z = 0.3f; log.rotation.y = angle;
                log.noise_amount = 0.08f; log.seed_offset = seed + (float)i;
                log.material.diffuse = vec4(0.35f,0.2f,0.08f,1); log.name = "Log" + std::to_string(i);
                obj.volumes.push_back(log);
            }
            auto core = make_sphere(vec3(0, s*0.15f, 0), s*0.08f, 10);
            core.noise_amount = 0.2f; core.noise_scale = 4.0f; core.seed_offset = seed + 20;
            core.material.diffuse = vec4(1,0.8f,0.1f,1); core.material.emission = vec4(0.9f,0.6f,0.05f,1);
            core.name = "Core";
            obj.volumes.push_back(core);
            auto flame = make_cone(vec3(0, s*0.3f, 0), s*0.1f, s*0.35f, 10);
            flame.noise_amount = 0.25f; flame.noise_scale = 3.0f; flame.seed_offset = seed + 25;
            flame.material.diffuse = vec4(1,0.4f,0.05f,1); flame.material.emission = vec4(0.8f,0.3f,0,1);
            flame.material.opacity = 0.85f; flame.name = "Flame";
            obj.volumes.push_back(flame);
            return obj;
        }
    });

    rules.push_back({"crate", {"crate", "caisse"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Crate";
            float s = sz;
            auto box = make_box(vec3(0, s*0.3f, 0), vec3(s*0.6f, s*0.6f, s*0.6f));
            box.noise_amount = 0.05f; box.seed_offset = seed;
            box.material.diffuse = col.w > 0 ? col : vec4(0.55f,0.38f,0.18f,1); box.name = "Box";
            obj.volumes.push_back(box);
            for (int i = 0; i < 2; i++) {
                float side = i == 0 ? -1.0f : 1.0f;
                auto plank = make_box(vec3(0, s*0.3f, side*s*0.301f), vec3(s*0.55f, s*0.04f, s*0.01f));
                plank.material.diffuse = vec4(0.45f,0.3f,0.12f,1); plank.name = "Plank" + std::to_string(i);
                obj.volumes.push_back(plank);
            }
            return obj;
        }
    });

    rules.push_back({"barrel", {"barrel", "tonneau"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Barrel";
            float s = sz;
            auto body = make_cylinder(vec3(0, s*0.3f, 0), s*0.22f, s*0.6f, 16);
            body.noise_amount = 0.03f; body.seed_offset = seed;
            body.material.diffuse = vec4(0.5f,0.32f,0.12f,1); body.name = "Body";
            obj.volumes.push_back(body);
            for (int i = 0; i < 2; i++) {
                float y = s*0.15f + (float)i * s*0.3f;
                auto band = make_torus(vec3(0, y, 0), s*0.225f, s*0.012f, 16);
                band.material.diffuse = vec4(0.3f,0.3f,0.32f,1); band.material.metallic = 0.8f;
                band.name = "Band" + std::to_string(i);
                obj.volumes.push_back(band);
            }
            return obj;
        }
    });

    rules.push_back({"bench", {"bench", "banc"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Bench";
            float s = sz;
            auto seat = make_box(vec3(0, s*0.4f, 0), vec3(s*0.8f, s*0.04f, s*0.3f));
            seat.noise_amount = 0.03f; seat.seed_offset = seed;
            seat.material.diffuse = col.w > 0 ? col : vec4(0.5f,0.32f,0.15f,1); seat.name = "Seat";
            obj.volumes.push_back(seat);
            auto back = make_box(vec3(0, s*0.65f, -s*0.13f), vec3(s*0.8f, s*0.45f, s*0.03f));
            back.material.diffuse = vec4(0.48f,0.3f,0.13f,1); back.name = "Back";
            obj.volumes.push_back(back);
            for (int i = 0; i < 4; i++) {
                float lx = (i%2 == 0 ? -1.0f : 1.0f) * s*0.32f;
                float lz = (i < 2 ? -1.0f : 1.0f) * s*0.1f;
                auto leg = make_cylinder(vec3(lx, s*0.19f, lz), s*0.025f, s*0.38f, 8);
                leg.material.diffuse = vec4(0.35f,0.35f,0.37f,1); leg.material.metallic = 0.6f;
                leg.name = "Leg" + std::to_string(i);
                obj.volumes.push_back(leg);
            }
            return obj;
        }
    });

    rules.push_back({"fountain", {"fountain", "fontaine"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Fountain";
            float s = sz;
            auto basin = make_cylinder(vec3(0, s*0.1f, 0), s*0.4f, s*0.2f, 16);
            basin.noise_amount = 0.03f; basin.seed_offset = seed;
            basin.material.diffuse = vec4(0.6f,0.57f,0.53f,1); basin.name = "Basin";
            obj.volumes.push_back(basin);
            auto water = make_disc(vec3(0, s*0.18f, 0), s*0.38f, s*0.02f, 16);
            water.material.diffuse = vec4(0.2f,0.5f,0.9f,0.7f); water.name = "Water";
            obj.volumes.push_back(water);
            auto pillar = make_cylinder(vec3(0, s*0.45f, 0), s*0.04f, s*0.5f, 8);
            pillar.material.diffuse = vec4(0.55f,0.52f,0.48f,1); pillar.name = "Pillar";
            obj.volumes.push_back(pillar);
            auto jet = make_cylinder(vec3(0, s*0.75f, 0), s*0.02f, s*0.2f, 6);
            jet.noise_amount = 0.15f; jet.seed_offset = seed + 10;
            jet.material.diffuse = vec4(0.4f,0.7f,1.0f,0.6f); jet.material.emission = vec4(0.1f,0.2f,0.4f,0.3f);
            jet.name = "Jet";
            obj.volumes.push_back(jet);
            return obj;
        }
    });

    rules.push_back({"cart", {"cart", "chariot", "wagon"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Cart";
            float s = sz;
            auto body = make_box(vec3(0, s*0.3f, 0), vec3(s*0.7f, s*0.3f, s*0.45f));
            body.noise_amount = 0.04f; body.seed_offset = seed;
            body.material.diffuse = col.w > 0 ? col : vec4(0.5f,0.32f,0.15f,1); body.name = "Body";
            obj.volumes.push_back(body);
            for (int i = 0; i < 2; i++) {
                float side = i == 0 ? -1.0f : 1.0f;
                auto wheel = make_torus(vec3(side*s*0.28f, s*0.08f, 0), s*0.12f, s*0.03f, 12);
                wheel.rotation.z = (float)M_PI*0.5f;
                wheel.material.diffuse = vec4(0.35f,0.25f,0.1f,1); wheel.name = "Wheel" + std::to_string(i);
                obj.volumes.push_back(wheel);
            }
            auto handle = make_cylinder(vec3(s*0.5f, s*0.35f, 0), s*0.02f, s*0.5f, 8);
            handle.rotation.z = 0.3f;
            handle.material.diffuse = vec4(0.4f,0.28f,0.12f,1); handle.name = "Handle";
            obj.volumes.push_back(handle);
            return obj;
        }
    });

    rules.push_back({"lantern", {"lantern", "lanterne"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Lantern";
            float s = sz;
            auto frame = make_box(vec3(0, s*0.35f, 0), vec3(s*0.2f, s*0.5f, s*0.2f));
            frame.noise_amount = 0.02f; frame.seed_offset = seed;
            frame.material.diffuse = vec4(0.3f,0.3f,0.32f,1); frame.material.metallic = 0.7f; frame.name = "Frame";
            obj.volumes.push_back(frame);
            auto glass = make_box(vec3(0, s*0.35f, 0), vec3(s*0.16f, s*0.4f, s*0.16f));
            glass.material.diffuse = vec4(1.0f,0.9f,0.6f,0.4f); glass.name = "Glass";
            obj.volumes.push_back(glass);
            auto flame = make_cone(vec3(0, s*0.35f, 0), s*0.025f, s*0.1f, 6);
            flame.material.diffuse = vec4(1,0.8f,0.1f,1); flame.material.emission = vec4(0.8f,0.6f,0.05f,1);
            flame.name = "Flame";
            obj.volumes.push_back(flame);
            auto top = make_cone(vec3(0, s*0.65f, 0), s*0.12f, s*0.1f, 8);
            top.material.diffuse = vec4(0.3f,0.3f,0.32f,1); top.material.metallic = 0.7f; top.name = "Top";
            obj.volumes.push_back(top);
            auto handle = make_torus(vec3(0, s*0.72f, 0), s*0.06f, s*0.01f, 10);
            handle.rotation.x = (float)M_PI*0.5f;
            handle.material.diffuse = vec4(0.35f,0.35f,0.37f,1); handle.material.metallic = 0.8f;
            handle.name = "Handle";
            obj.volumes.push_back(handle);
            return obj;
        }
    });

    rules.push_back({"gun", {"gun", "pistolet", "pistol", "blaster"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Gun";
            float s = sz;
            auto barrel = make_box(vec3(s*0.15f, s*0.05f, 0), vec3(s*0.5f, s*0.05f, s*0.04f));
            barrel.material.diffuse = vec4(0.25f,0.25f,0.27f,1); barrel.material.metallic = 0.8f; barrel.name = "Barrel";
            obj.volumes.push_back(barrel);
            auto body = make_box(vec3(0, -s*0.02f, 0), vec3(s*0.35f, s*0.08f, s*0.05f));
            body.material.diffuse = vec4(0.2f,0.2f,0.22f,1); body.material.metallic = 0.7f; body.name = "Body";
            obj.volumes.push_back(body);
            auto grip = make_box(vec3(-s*0.05f, -s*0.15f, 0), vec3(s*0.06f, s*0.18f, s*0.05f));
            grip.rotation.z = 0.2f;
            grip.material.diffuse = vec4(0.35f,0.25f,0.12f,1); grip.name = "Grip";
            obj.volumes.push_back(grip);
            auto trigger = make_box(vec3(-s*0.02f, -s*0.06f, 0), vec3(s*0.02f, s*0.05f, s*0.02f));
            trigger.material.diffuse = vec4(0.3f,0.3f,0.32f,1); trigger.material.metallic = 0.8f; trigger.name = "Trigger";
            obj.volumes.push_back(trigger);
            auto sight = make_box(vec3(s*0.3f, s*0.09f, 0), vec3(s*0.02f, s*0.02f, s*0.02f));
            sight.material.diffuse = vec4(0.2f,0.2f,0.22f,1); sight.name = "Sight";
            obj.volumes.push_back(sight);
            return obj;
        }
    });

    rules.push_back({"pistol", {"pistol", "gun"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Pistol";
            float s = sz;
            auto slide = make_box(vec3(s*0.1f, s*0.04f, 0), vec3(s*0.4f, s*0.06f, s*0.04f));
            slide.material.diffuse = vec4(0.22f,0.22f,0.24f,1); slide.material.metallic = 0.85f; slide.name = "Slide";
            obj.volumes.push_back(slide);
            auto frame = make_box(vec3(0, -s*0.02f, 0), vec3(s*0.3f, s*0.05f, s*0.045f));
            frame.material.diffuse = vec4(0.18f,0.18f,0.2f,1); frame.material.metallic = 0.7f; frame.name = "Frame";
            obj.volumes.push_back(frame);
            auto grip = make_box(vec3(-s*0.04f, -s*0.14f, 0), vec3(s*0.06f, s*0.16f, s*0.045f));
            grip.rotation.z = 0.2f;
            grip.material.diffuse = vec4(0.3f,0.28f,0.25f,1); grip.name = "Grip";
            obj.volumes.push_back(grip);
            auto trigger_guard = make_torus(vec3(-s*0.02f, -s*0.06f, 0), s*0.04f, s*0.005f, 8);
            trigger_guard.rotation.x = (float)M_PI*0.5f;
            trigger_guard.material.diffuse = vec4(0.22f,0.22f,0.24f,1); trigger_guard.material.metallic = 0.8f;
            trigger_guard.name = "TriggerGuard";
            obj.volumes.push_back(trigger_guard);
            return obj;
        }
    });

    rules.push_back({"turret", {"turret", "tourelle"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Turret";
            float s = sz;
            auto base = make_cylinder(vec3(0, s*0.1f, 0), s*0.3f, s*0.2f, 12);
            base.noise_amount = 0.03f; base.seed_offset = seed;
            base.material.diffuse = vec4(0.4f,0.42f,0.45f,1); base.material.metallic = 0.7f; base.name = "Base";
            obj.volumes.push_back(base);
            auto housing = make_box(vec3(0, s*0.35f, 0), vec3(s*0.25f, s*0.25f, s*0.25f));
            housing.noise_amount = 0.03f; housing.seed_offset = seed+1;
            housing.material.diffuse = vec4(0.45f,0.47f,0.5f,1); housing.material.metallic = 0.6f; housing.name = "Housing";
            obj.volumes.push_back(housing);
            auto barrel1 = make_cylinder(vec3(s*0.25f, s*0.38f, s*0.05f), s*0.03f, s*0.4f, 8);
            barrel1.rotation.z = (float)M_PI*0.5f;
            barrel1.material.diffuse = vec4(0.3f,0.3f,0.32f,1); barrel1.material.metallic = 0.9f; barrel1.name = "Barrel1";
            obj.volumes.push_back(barrel1);
            auto barrel2 = make_cylinder(vec3(s*0.25f, s*0.38f, -s*0.05f), s*0.03f, s*0.4f, 8);
            barrel2.rotation.z = (float)M_PI*0.5f;
            barrel2.material.diffuse = vec4(0.3f,0.3f,0.32f,1); barrel2.material.metallic = 0.9f; barrel2.name = "Barrel2";
            obj.volumes.push_back(barrel2);
            auto eye = make_sphere(vec3(s*0.13f, s*0.4f, 0), s*0.04f, 8);
            eye.material.diffuse = vec4(1,0.1f,0.1f,1); eye.material.emission = vec4(0.8f,0.05f,0.05f,1);
            eye.name = "Eye";
            obj.volumes.push_back(eye);
            return obj;
        }
    });

    rules.push_back({"coral", {"coral", "corail"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Coral";
            float s = sz;
            for (int i = 0; i < 7; i++) {
                float x = fbm3d((float)i*4.0f+seed, 0, 0, 3) * s*0.25f;
                float z = fbm3d(0, (float)i*4.0f+seed, 0, 3) * s*0.25f;
                float h = s * (0.15f + 0.35f * noise2d(seed+(float)i, 300));
                auto branch = make_cylinder(vec3(x, h*0.5f, z), s*0.02f + s*0.015f*noise2d(seed+(float)i,310), h, 6);
                branch.noise_amount = 0.15f; branch.seed_offset = seed + (float)(i+1)*5;
                branch.rotation.x = (noise2d(seed+(float)i,320)-0.5f)*0.4f;
                branch.rotation.z = (noise2d(seed+(float)i,330)-0.5f)*0.4f;
                vec4 c = col.w > 0 ? col : vec4(0.9f, 0.3f+0.4f*noise2d(seed+(float)i,340), 0.3f, 1);
                branch.material.diffuse = c; branch.name = "Branch" + std::to_string(i);
                obj.volumes.push_back(branch);
                auto tip = make_sphere(vec3(x, h, z), s*0.025f, 6);
                tip.material.diffuse = c; tip.name = "Tip" + std::to_string(i);
                obj.volumes.push_back(tip);
            }
            auto base = make_sphere(vec3(0, -s*0.02f, 0), s*0.15f, 8);
            base.noise_amount = 0.12f; base.scale = vec3(2.0f, 0.3f, 2.0f);
            base.seed_offset = seed + 50;
            base.material.diffuse = vec4(0.6f,0.55f,0.5f,1); base.name = "Base";
            obj.volumes.push_back(base);
            return obj;
        }
    });

    rules.push_back({"butterfly", {"butterfly", "papillon"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Butterfly";
            float s = sz;
            auto body = make_capsule(vec3(0, 0, 0), s*0.03f, s*0.2f, 8);
            body.rotation.z = (float)M_PI*0.5f;
            body.material.diffuse = vec4(0.2f,0.15f,0.1f,1); body.name = "Body";
            obj.volumes.push_back(body);
            auto head = make_sphere(vec3(s*0.12f, 0, 0), s*0.04f, 8);
            head.material.diffuse = vec4(0.2f,0.15f,0.1f,1); head.name = "Head";
            obj.volumes.push_back(head);
            vec4 wc = col.w > 0 ? col : vec4(0.9f,0.4f,0.1f,1);
            for (int i = 0; i < 4; i++) {
                float side = i%2 == 0 ? -1.0f : 1.0f;
                float front = i < 2 ? 1.0f : -1.0f;
                float wx = front * s*0.05f;
                float wz = side * s*0.2f;
                float wy = front * s*0.05f;
                auto wing = make_disc(vec3(wx, wy, wz), s*0.12f, s*0.005f, 10);
                wing.scale = vec3(1.0f, 1.0f, 0.7f);
                wing.rotation.y = side * 0.3f;
                wing.noise_amount = 0.06f; wing.seed_offset = seed + (float)(i+1)*5;
                wing.material.diffuse = wc;
                wing.material.emission = vec4(wc.x*0.2f, wc.y*0.2f, wc.z*0.2f, 1);
                wing.name = "Wing" + std::to_string(i);
                obj.volumes.push_back(wing);
            }
            for (int i = 0; i < 2; i++) {
                float side = i == 0 ? -1.0f : 1.0f;
                auto antenna = make_cylinder(vec3(s*0.12f, s*0.04f, side*s*0.02f), s*0.003f, s*0.1f, 4);
                antenna.rotation.z = side * 0.5f;
                antenna.material.diffuse = vec4(0.2f,0.15f,0.1f,1); antenna.name = "Antenna" + std::to_string(i);
                obj.volumes.push_back(antenna);
                auto tip = make_sphere(vec3(s*0.17f, s*0.08f, side*s*0.06f), s*0.008f, 6);
                tip.material.diffuse = vec4(0.2f,0.15f,0.1f,1); tip.name = "AntennaTip" + std::to_string(i);
                obj.volumes.push_back(tip);
            }
            return obj;
        }
    });

    rules.push_back({"cat", {"cat", "chat"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Cat";
            float s = sz;
            auto body = make_capsule(vec3(0, s*0.2f, 0), s*0.12f, s*0.3f, 12);
            body.noise_amount = 0.04f; body.seed_offset = seed;
            body.material.diffuse = col.w > 0 ? col : vec4(0.7f,0.55f,0.3f,1); body.name = "Body";
            obj.volumes.push_back(body);
            auto head = make_sphere(vec3(s*0.2f, s*0.4f, 0), s*0.1f, 12);
            head.noise_amount = 0.03f; head.seed_offset = seed+1;
            head.material.diffuse = col.w > 0 ? col : vec4(0.7f,0.55f,0.3f,1); head.name = "Head";
            obj.volumes.push_back(head);
            for (int i = 0; i < 2; i++) {
                float side = i == 0 ? -1.0f : 1.0f;
                auto ear = make_cone(vec3(s*0.2f, s*0.52f, side*s*0.06f), s*0.03f, s*0.06f, 6);
                ear.material.diffuse = vec4(0.7f,0.55f,0.3f,1); ear.name = "Ear" + std::to_string(i);
                obj.volumes.push_back(ear);
                auto eye = make_sphere(vec3(s*0.28f, s*0.42f, side*s*0.04f), s*0.02f, 8);
                eye.material.diffuse = vec4(0.2f,0.8f,0.2f,1); eye.name = "Eye" + std::to_string(i);
                obj.volumes.push_back(eye);
            }
            auto nose = make_sphere(vec3(s*0.3f, s*0.38f, 0), s*0.012f, 6);
            nose.material.diffuse = vec4(0.9f,0.4f,0.4f,1); nose.name = "Nose";
            obj.volumes.push_back(nose);
            auto tail = make_cylinder(vec3(-s*0.18f, s*0.35f, 0), s*0.02f, s*0.25f, 6);
            tail.noise_amount = 0.12f; tail.seed_offset = seed+10;
            tail.rotation.z = -0.8f;
            tail.material.diffuse = col.w > 0 ? col : vec4(0.7f,0.55f,0.3f,1); tail.name = "Tail";
            obj.volumes.push_back(tail);
            for (int i = 0; i < 4; i++) {
                float fx = (i%2 == 0 ? 1.0f : -1.0f) * s*0.06f;
                float fz = (i < 2 ? 1.0f : -1.0f) * s*0.06f;
                auto leg = make_cylinder(vec3(fx, s*0.05f, fz), s*0.025f, s*0.2f, 6);
                leg.material.diffuse = col.w > 0 ? col : vec4(0.65f,0.5f,0.28f,1); leg.name = "Leg" + std::to_string(i);
                obj.volumes.push_back(leg);
            }
            return obj;
        }
    });

    rules.push_back({"dog", {"dog", "chien"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Dog";
            float s = sz;
            auto body = make_capsule(vec3(0, s*0.25f, 0), s*0.12f, s*0.35f, 12);
            body.noise_amount = 0.04f; body.seed_offset = seed;
            body.material.diffuse = col.w > 0 ? col : vec4(0.55f,0.38f,0.2f,1); body.name = "Body";
            obj.volumes.push_back(body);
            auto head = make_box(vec3(s*0.25f, s*0.4f, 0), vec3(s*0.15f, s*0.12f, s*0.12f));
            head.noise_amount = 0.03f; head.seed_offset = seed+1;
            head.material.diffuse = col.w > 0 ? col : vec4(0.55f,0.38f,0.2f,1); head.name = "Head";
            obj.volumes.push_back(head);
            auto snout = make_box(vec3(s*0.35f, s*0.37f, 0), vec3(s*0.08f, s*0.06f, s*0.06f));
            snout.material.diffuse = vec4(0.5f,0.35f,0.18f,1); snout.name = "Snout";
            obj.volumes.push_back(snout);
            auto nose = make_sphere(vec3(s*0.4f, s*0.38f, 0), s*0.018f, 6);
            nose.material.diffuse = vec4(0.1f,0.1f,0.1f,1); nose.name = "Nose";
            obj.volumes.push_back(nose);
            for (int i = 0; i < 2; i++) {
                float side = i == 0 ? -1.0f : 1.0f;
                auto ear = make_sphere(vec3(s*0.22f, s*0.48f, side*s*0.07f), s*0.04f, 8);
                ear.scale = vec3(0.8f, 1.2f, 0.5f);
                ear.material.diffuse = vec4(0.45f,0.3f,0.15f,1); ear.name = "Ear" + std::to_string(i);
                obj.volumes.push_back(ear);
                auto eye = make_sphere(vec3(s*0.32f, s*0.43f, side*s*0.04f), s*0.018f, 8);
                eye.material.diffuse = vec4(0.15f,0.1f,0.05f,1); eye.name = "Eye" + std::to_string(i);
                obj.volumes.push_back(eye);
            }
            auto tail = make_cylinder(vec3(-s*0.2f, s*0.4f, 0), s*0.018f, s*0.2f, 6);
            tail.noise_amount = 0.1f; tail.seed_offset = seed+10;
            tail.rotation.z = -1.0f;
            tail.material.diffuse = col.w > 0 ? col : vec4(0.55f,0.38f,0.2f,1); tail.name = "Tail";
            obj.volumes.push_back(tail);
            for (int i = 0; i < 4; i++) {
                float fx = (i%2 == 0 ? 1.0f : -1.0f) * s*0.06f;
                float fz = (i < 2 ? 1.0f : -1.0f) * s*0.06f;
                auto leg = make_cylinder(vec3(fx, s*0.08f, fz), s*0.028f, s*0.25f, 6);
                leg.material.diffuse = col.w > 0 ? col : vec4(0.5f,0.35f,0.18f,1); leg.name = "Leg" + std::to_string(i);
                obj.volumes.push_back(leg);
            }
            return obj;
        }
    });

    rules.push_back({"bird", {"bird", "oiseau"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Bird";
            float s = sz;
            auto body = make_sphere(vec3(0, 0, 0), s*0.15f, 14);
            body.noise_amount = 0.04f; body.seed_offset = seed;
            body.scale = vec3(1.2f, 0.9f, 1.0f);
            body.material.diffuse = col.w > 0 ? col : vec4(0.3f,0.5f,0.8f,1); body.name = "Body";
            obj.volumes.push_back(body);
            auto head = make_sphere(vec3(s*0.18f, s*0.12f, 0), s*0.08f, 10);
            head.material.diffuse = col.w > 0 ? col : vec4(0.3f,0.5f,0.8f,1); head.name = "Head";
            obj.volumes.push_back(head);
            auto beak = make_cone(vec3(s*0.28f, s*0.1f, 0), s*0.02f, s*0.06f, 6);
            beak.rotation.z = -(float)M_PI*0.5f;
            beak.material.diffuse = vec4(0.9f,0.7f,0.1f,1); beak.name = "Beak";
            obj.volumes.push_back(beak);
            auto eye = make_sphere(vec3(s*0.24f, s*0.15f, s*0.03f), s*0.012f, 6);
            eye.material.diffuse = vec4(0.05f,0.05f,0.05f,1); eye.name = "Eye";
            obj.volumes.push_back(eye);
            for (int i = 0; i < 2; i++) {
                float side = i == 0 ? -1.0f : 1.0f;
                auto wing = make_disc(vec3(-s*0.02f, s*0.05f, side*s*0.18f), s*0.15f, s*0.005f, 10);
                wing.scale = vec3(1.3f, 0.7f, 1.0f);
                wing.rotation.x = side * 0.2f;
                wing.noise_amount = 0.06f; wing.seed_offset = seed + (float)(i+1)*5;
                wing.material.diffuse = col.w > 0 ? col : vec4(0.25f,0.45f,0.75f,1);
                wing.name = "Wing" + std::to_string(i);
                obj.volumes.push_back(wing);
            }
            auto tail = make_cone(vec3(-s*0.2f, s*0.05f, 0), s*0.04f, s*0.12f, 6);
            tail.rotation.z = (float)M_PI*0.5f;
            tail.material.diffuse = col.w > 0 ? col : vec4(0.25f,0.45f,0.75f,1); tail.name = "Tail";
            obj.volumes.push_back(tail);
            for (int i = 0; i < 2; i++) {
                float side = i == 0 ? -1.0f : 1.0f;
                auto leg = make_cylinder(vec3(side*s*0.04f, -s*0.15f, 0), s*0.01f, s*0.1f, 4);
                leg.material.diffuse = vec4(0.8f,0.6f,0.2f,1); leg.name = "Leg" + std::to_string(i);
                obj.volumes.push_back(leg);
            }
            return obj;
        }
    });

    rules.push_back({"stump", {"stump", "souche"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Stump";
            float s = sz;
            auto trunk = make_cylinder(vec3(0, s*0.2f, 0), s*0.2f, s*0.4f, 14);
            trunk.noise_amount = 0.08f; trunk.noise_scale = 3.0f; trunk.seed_offset = seed;
            trunk.material.diffuse = vec4(0.38f,0.24f,0.09f,1); trunk.name = "Trunk";
            obj.volumes.push_back(trunk);
            auto top = make_disc(vec3(0, s*0.41f, 0), s*0.2f, s*0.02f, 14);
            top.material.diffuse = vec4(0.55f,0.4f,0.2f,1); top.name = "Top";
            obj.volumes.push_back(top);
            for (int i = 0; i < 3; i++) {
                float angle = (float)M_PI * 2.0f * i / 3.0f + seed * 0.1f;
                auto root = make_sphere(vec3(cosf(angle)*s*0.22f, -s*0.02f, sinf(angle)*s*0.22f), s*0.06f, 6);
                root.scale = vec3(1.5f, 0.4f, 1.5f);
                root.noise_amount = 0.1f; root.seed_offset = seed + (float)(i+1)*5;
                root.material.diffuse = vec4(0.35f,0.22f,0.08f,1); root.name = "Root" + std::to_string(i);
                obj.volumes.push_back(root);
            }
            return obj;
        }
    });

    rules.push_back({"wall", {"wall", "mur"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Wall";
            float s = sz;
            auto wall = make_box(vec3(0, s*0.4f, 0), vec3(s*1.5f, s*0.8f, s*0.15f));
            wall.noise_amount = 0.05f; wall.noise_scale = 3.0f; wall.seed_offset = seed;
            wall.material.diffuse = col.w > 0 ? col : vec4(0.6f,0.55f,0.5f,1); wall.name = "Wall";
            obj.volumes.push_back(wall);
            return obj;
        }
    });

    rules.push_back({"grave", {"grave", "tombe", "tombstone"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Grave";
            float s = sz;
            auto stone = make_box(vec3(0, s*0.35f, 0), vec3(s*0.35f, s*0.7f, s*0.08f));
            stone.noise_amount = 0.04f; stone.seed_offset = seed;
            stone.material.diffuse = vec4(0.5f,0.48f,0.45f,1); stone.name = "Stone";
            obj.volumes.push_back(stone);
            auto top = make_hemisphere(vec3(0, s*0.72f, 0), s*0.175f, 10);
            top.rotation.z = (float)M_PI*0.5f;
            top.material.diffuse = vec4(0.5f,0.48f,0.45f,1); top.name = "Top";
            obj.volumes.push_back(top);
            auto cross_h = make_box(vec3(0, s*0.55f, s*0.045f), vec3(s*0.15f, s*0.03f, s*0.03f));
            cross_h.material.diffuse = vec4(0.45f,0.43f,0.4f,1); cross_h.name = "CrossH";
            obj.volumes.push_back(cross_h);
            auto cross_v = make_box(vec3(0, s*0.6f, s*0.045f), vec3(s*0.03f, s*0.2f, s*0.03f));
            cross_v.material.diffuse = vec4(0.45f,0.43f,0.4f,1); cross_v.name = "CrossV";
            obj.volumes.push_back(cross_v);
            auto mound = make_disc(vec3(0, s*0.02f, s*0.1f), s*0.25f, s*0.08f, 10);
            mound.scale = vec3(1.5f, 0.5f, 1.0f);
            mound.material.diffuse = vec4(0.35f,0.3f,0.2f,1); mound.name = "Mound";
            obj.volumes.push_back(mound);
            return obj;
        }
    });

    rules.push_back({"flag", {"flag", "drapeau"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Flag";
            float s = sz;
            auto pole = make_cylinder(vec3(-s*0.3f, s*0.4f, 0), s*0.015f, s*1.0f, 8);
            pole.material.diffuse = vec4(0.5f,0.4f,0.2f,1); pole.name = "Pole";
            obj.volumes.push_back(pole);
            auto cloth = make_box(vec3(s*0.05f, s*0.65f, 0), vec3(s*0.5f, s*0.35f, s*0.01f));
            cloth.noise_amount = 0.1f; cloth.noise_scale = 2.5f; cloth.seed_offset = seed;
            cloth.material.diffuse = col.w > 0 ? col : vec4(0.8f,0.15f,0.1f,1); cloth.name = "Cloth";
            obj.volumes.push_back(cloth);
            auto finial = make_sphere(vec3(-s*0.3f, s*0.92f, 0), s*0.025f, 8);
            finial.material.diffuse = vec4(1.0f,0.84f,0.0f,1); finial.material.metallic = 0.9f;
            finial.name = "Finial";
            obj.volumes.push_back(finial);
            return obj;
        }
    });

    rules.push_back({"bookshelf", {"bookshelf", "bibliotheque"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Bookshelf";
            float s = sz;
            auto back = make_box(vec3(0, s*0.5f, -s*0.12f), vec3(s*0.8f, s*1.0f, s*0.02f));
            back.material.diffuse = vec4(0.45f,0.3f,0.13f,1); back.name = "Back";
            obj.volumes.push_back(back);
            auto side_l = make_box(vec3(-s*0.4f, s*0.5f, 0), vec3(s*0.03f, s*1.0f, s*0.25f));
            side_l.material.diffuse = vec4(0.48f,0.32f,0.15f,1); side_l.name = "SideL";
            obj.volumes.push_back(side_l);
            auto side_r = make_box(vec3(s*0.4f, s*0.5f, 0), vec3(s*0.03f, s*1.0f, s*0.25f));
            side_r.material.diffuse = vec4(0.48f,0.32f,0.15f,1); side_r.name = "SideR";
            obj.volumes.push_back(side_r);
            for (int i = 0; i < 4; i++) {
                float y = (float)i * s*0.28f;
                auto shelf = make_box(vec3(0, y, 0), vec3(s*0.78f, s*0.025f, s*0.25f));
                shelf.material.diffuse = vec4(0.5f,0.33f,0.15f,1); shelf.name = "Shelf" + std::to_string(i);
                obj.volumes.push_back(shelf);
            }
            vec4 book_colors[] = {vec4(0.8f,0.1f,0.1f,1), vec4(0.1f,0.3f,0.8f,1), vec4(0.1f,0.6f,0.15f,1), vec4(0.8f,0.5f,0.1f,1), vec4(0.6f,0.1f,0.6f,1)};
            for (int i = 0; i < 6; i++) {
                float shelf_y = s*0.28f + (float)(i/3) * s*0.28f;
                float x = -s*0.2f + (float)(i%3) * s*0.15f;
                float h = s*0.15f + s*0.05f * noise2d(seed+(float)i, 400);
                auto book = make_box(vec3(x, shelf_y + h*0.5f + s*0.015f, 0), vec3(s*0.04f, h, s*0.18f));
                book.noise_amount = 0.02f; book.seed_offset = seed + (float)i;
                book.material.diffuse = book_colors[i % 5]; book.name = "Book" + std::to_string(i);
                obj.volumes.push_back(book);
            }
            return obj;
        }
    });

    rules.push_back({"potion", {"potion"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Potion";
            float s = sz;
            auto flask = make_sphere(vec3(0, s*0.15f, 0), s*0.15f, 12);
            flask.noise_amount = 0.03f; flask.seed_offset = seed;
            flask.scale = vec3(1.0f, 0.8f, 1.0f);
            flask.material.diffuse = col.w > 0 ? col : vec4(0.3f,0.7f,0.3f,0.7f);
            flask.material.opacity = 0.7f; flask.name = "Flask";
            obj.volumes.push_back(flask);
            auto neck = make_cylinder(vec3(0, s*0.32f, 0), s*0.04f, s*0.12f, 8);
            neck.material.diffuse = vec4(0.6f,0.8f,0.6f,0.8f); neck.material.opacity = 0.8f; neck.name = "Neck";
            obj.volumes.push_back(neck);
            auto cork = make_cylinder(vec3(0, s*0.4f, 0), s*0.035f, s*0.05f, 6);
            cork.material.diffuse = vec4(0.6f,0.4f,0.2f,1); cork.name = "Cork";
            obj.volumes.push_back(cork);
            return obj;
        }
    });

    rules.push_back({"cave", {"cave", "grotte"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Cave";
            float s = sz * 1.5f;
            auto arch = make_torus(vec3(0, s*0.15f, 0), s*0.4f, s*0.15f, 16);
            arch.rotation.x = (float)M_PI*0.5f;
            arch.noise_amount = 0.15f; arch.noise_scale = 2.0f; arch.seed_offset = seed;
            arch.material.diffuse = vec4(0.4f,0.38f,0.35f,1); arch.name = "Arch";
            obj.volumes.push_back(arch);
            auto wall_l = make_box(vec3(-s*0.55f, s*0.15f, 0), vec3(s*0.3f, s*0.6f, s*0.5f));
            wall_l.noise_amount = 0.12f; wall_l.seed_offset = seed+5;
            wall_l.material.diffuse = vec4(0.38f,0.36f,0.33f,1); wall_l.name = "WallL";
            obj.volumes.push_back(wall_l);
            auto wall_r = make_box(vec3(s*0.55f, s*0.15f, 0), vec3(s*0.3f, s*0.6f, s*0.5f));
            wall_r.noise_amount = 0.12f; wall_r.seed_offset = seed+6;
            wall_r.material.diffuse = vec4(0.38f,0.36f,0.33f,1); wall_r.name = "WallR";
            obj.volumes.push_back(wall_r);
            auto ceiling = make_box(vec3(0, s*0.5f, 0), vec3(s*1.2f, s*0.2f, s*0.5f));
            ceiling.noise_amount = 0.15f; ceiling.seed_offset = seed+10;
            ceiling.material.diffuse = vec4(0.35f,0.33f,0.3f,1); ceiling.name = "Ceiling";
            obj.volumes.push_back(ceiling);
            return obj;
        }
    });

    rules.push_back({"tent", {"tent", "tente"},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Tent";
            float s = sz;
            auto body = make_cone(vec3(0, s*0.35f, 0), s*0.5f, s*0.7f, 4);
            body.noise_amount = 0.06f; body.seed_offset = seed; body.rotation.y = (float)M_PI*0.25f;
            body.material.diffuse = col.w > 0 ? col : vec4(0.6f,0.45f,0.2f,1); body.name = "Body";
            obj.volumes.push_back(body);
            auto floor = make_disc(vec3(0, -s*0.01f, 0), s*0.5f, s*0.02f, 10);
            floor.material.diffuse = vec4(0.35f,0.3f,0.2f,1); floor.name = "Floor";
            obj.volumes.push_back(floor);
            auto pole = make_cylinder(vec3(0, s*0.4f, 0), s*0.015f, s*0.8f, 6);
            pole.material.diffuse = vec4(0.4f,0.28f,0.12f,1); pole.name = "Pole";
            obj.volumes.push_back(pole);
            auto door_flap = make_box(vec3(s*0.25f, s*0.15f, s*0.02f), vec3(s*0.18f, s*0.3f, s*0.01f));
            door_flap.noise_amount = 0.08f; door_flap.seed_offset = seed+10;
            door_flap.material.diffuse = col.w > 0 ? col : vec4(0.55f,0.4f,0.18f,1); door_flap.name = "DoorFlap";
            obj.volumes.push_back(door_flap);
            return obj;
        }
    });

    // Generic rule: builds a parametric shape from the prompt's shape list
    rules.push_back({"_generic", {},
        [](float sz, uint32_t seed, const vec4& col) -> DecomposedObject {
            DecomposedObject obj; obj.name = "Object";
            auto vol = make_box(vec3(0), vec3(sz));
            vol.noise_amount = 0.05f; vol.seed_offset = seed;
            vol.material.diffuse = col.w > 0 ? col : vec4(0.6f, 0.6f, 0.65f, 1);
            vol.name = "Shape";
            obj.volumes.push_back(vol);
            return obj;
        }
    });
}

DecomposedObject Decomposer::decompose(const ParsedPrompt& prompt) {
    std::string lt = to_lower(prompt.original);

    for (auto& rule : rules) {
        if (rule.keywords.empty()) continue;
        for (auto& kw : rule.keywords) {
            if (contains_word(lt, kw)) {
                return rule.builder(prompt.size, prompt.seed, prompt.color);
            }
        }
    }

    // Fallback: if no scene_type matched, try generic shapes
    if (!prompt.shapes.empty()) {
        DecomposedObject obj; obj.name = "Custom";
        for (auto& sh : prompt.shapes) {
            VolumeDescription vol;
            if (sh == "cube") {
                vol = make_box(vec3(0), vec3(prompt.size));
            } else if (sh == "sphere") {
                vol = make_sphere(vec3(0), prompt.size * 0.5f, 20);
            } else if (sh == "cylinder") {
                vol = make_cylinder(vec3(0), prompt.size * 0.3f, prompt.size, 16);
            } else if (sh == "cone") {
                vol = make_cone(vec3(0), prompt.size * 0.3f, prompt.size, 16);
            } else if (sh == "torus") {
                vol = make_torus(vec3(0), prompt.size * 0.3f, prompt.size * 0.1f, 20);
            } else if (sh == "arrow") {
                vol = make_arrow(vec3(0), prompt.size * 0.05f, prompt.size * 0.6f, prompt.size * 0.15f, prompt.size * 0.2f);
            } else if (sh == "plane") {
                vol = make_plane(vec3(0), vec3(prompt.size));
            } else {
                vol = make_box(vec3(0), vec3(prompt.size));
            }
            vol.noise_amount = 0.05f; vol.seed_offset = prompt.seed;
            if (prompt.color.w > 0) vol.material.diffuse = prompt.color;
            vol.name = sh;
            obj.volumes.push_back(vol);
        }
        return obj;
    }

    // Absolute fallback
    return rules.back().builder(prompt.size, prompt.seed, prompt.color);
}

std::string Decomposer::get_description(const DecomposedObject& obj) {
    return "Procedurally modeled " + obj.name + " from " +
           std::to_string(obj.volumes.size()) + " parametric volumes with noise deformation";
}

// ============================================================
// OBJECT GENERATOR
// ============================================================

ObjectGenerator::ObjectGenerator() {}

Scene* ObjectGenerator::generate_from_prompt(const std::string& prompt, Scene* scene) {
    if (!scene) scene = new Scene();

    ParsedPrompt pp = prompt_engine.parse(prompt);
    last_description = prompt_engine.get_description(pp);

    rng_engine = std::mt19937(pp.seed);
    builder.set_rng(&rng_engine);

    DecomposedObject decomposed = decomposer.decompose(pp);
    last_description = decomposer.get_description(decomposed);

    Mesh result = builder.build(decomposed);

    // Apply modifiers
    for (auto& mod : pp.modifiers) {
        if (mod == "subdivide") result.subdivide();
        else if (mod == "smooth") result.smooth_vertices(0.5f, 2);
        else if (mod == "mirror") result.mirror(0);
    }

    // Determine primary material from first volume
    Material primary_mat;
    if (!decomposed.volumes.empty() && decomposed.volumes[0].material.diffuse.w > 0) {
        primary_mat = decomposed.volumes[0].material;
    }
    if (pp.color.w > 0) primary_mat.diffuse = pp.color;

    add_mesh_to_scene(scene, result, decomposed.name, primary_mat);

    return scene;
}

void ObjectGenerator::add_mesh_to_scene(Scene* scene, const Mesh& mesh,
                                        const std::string& name, const Material& mat) {
    auto node = std::make_unique<SceneNode>();
    node->name = name;
    node->mesh = std::make_unique<Mesh>(mesh);
    node->material = std::make_unique<Material>(mat);
    scene->root.add_child(std::move(node));
}

std::string ObjectGenerator::get_last_description() const { return last_description; }

} // namespace ocp
