#pragma once
#include "scene.h"
#include "mesh.h"
#include <string>
#include <vector>
#include <map>
#include <random>

namespace ocp {

// ============================================================
// PROMPT PARSING
// ============================================================

struct ParsedPrompt {
    std::string original;
    std::vector<std::string> shapes;
    std::string scene_type;
    float size = 1.0f;
    vec4 color = vec4(-1);
    std::string material_type;
    int count = 1;
    std::string arrangement;
    std::vector<std::string> modifiers;
    std::string style;
    uint32_t seed = 0;
};

class PromptEngine {
public:
    ParsedPrompt parse(const std::string& prompt);
    std::string get_description(const ParsedPrompt& p);
};

// ============================================================
// VOLUME-BASED DECOMPOSITION (replaces hardcoded generators)
// ============================================================

enum class VolumeType {
    Sphere, Box, Cylinder, Cone, Torus, Capsule,
    Hemisphere, Loft, Ring, Wedge, Disc, Plane,
    Arrow, TorusKnot, Custom
};

struct VolumeDescription {
    VolumeType type = VolumeType::Box;
    vec3 position = vec3(0);
    vec3 rotation = vec3(0);
    vec3 scale = vec3(1);
    float seed_offset = 0.0f;
    float noise_amount = 0.05f;
    float noise_scale = 3.0f;
    int segments = 24;
    int rings = 16;

    union {
        struct { float radius; } sphere;
        struct { float radius; float height; } cylinder;
        struct { float radius; float height; } cone;
        struct { float major_r; float minor_r; } torus;
        struct { float radius; float height; } capsule;
        struct { float radius; float half_angle; } hemisphere;
        struct { float inner_r; float outer_r; } ring;
        struct { float radius; float height; float angle; } wedge;
        struct { float radius; float thickness; } disc;
    } params = {};

    Material material;
    std::string name;
};

struct DecomposedObject {
    std::string name;
    std::string description;
    std::vector<VolumeDescription> volumes;
};

// ============================================================
// PARAMETRIC MESH BUILDER (vertex-by-vertex, no pre-made parts)
// ============================================================

class ParametricBuilder {
public:
    ParametricBuilder();

    Mesh build(const DecomposedObject& obj);

    void set_rng(std::mt19937* r) { rng_engine = r; }

private:
    std::mt19937* rng_engine = nullptr;

    // Volume mesh builders (parametric surfaces with noise)
    Mesh build_sphere(const VolumeDescription& vol);
    Mesh build_box(const VolumeDescription& vol);
    Mesh build_cylinder(const VolumeDescription& vol);
    Mesh build_cone(const VolumeDescription& vol);
    Mesh build_torus(const VolumeDescription& vol);
    Mesh build_capsule(const VolumeDescription& vol);
    Mesh build_hemisphere(const VolumeDescription& vol);
    Mesh build_ring(const VolumeDescription& vol);
    Mesh build_wedge(const VolumeDescription& vol);
    Mesh build_disc(const VolumeDescription& vol);
    Mesh build_arrow(const VolumeDescription& vol);
    Mesh build_plane(const VolumeDescription& vol);

    // Loft: interpolates cross-sections along a path
    struct CrossSection {
        vec3 center;
        std::vector<vec3> profile;
    };
    Mesh build_loft(const VolumeDescription& vol,
                    const std::vector<CrossSection>& path);

    // Transform a volume mesh by position/rotation/scale
    void transform_mesh(Mesh& m, const VolumeDescription& vol);

    // Noise helper
    float vol_noise(float x, float y, float z, float seed);

    // Ring builder (parametric circle)
    Mesh build_ring_primitive(float inner_r, float outer_r, int segments);

    // Disc builder
    Mesh build_disc_primitive(float radius, float thickness, int segments);
};

// ============================================================
// DECOMPOSER (prompt text → volume decomposition rules)
// ============================================================

class Decomposer {
public:
    Decomposer();

    DecomposedObject decompose(const ParsedPrompt& prompt);
    std::string get_description(const DecomposedObject& obj);

private:
    struct DecompositionRule {
        std::string object_type;
        std::vector<std::string> keywords;
        std::function<DecomposedObject(float size, uint32_t seed, const vec4& color)> builder;
    };

    std::vector<DecompositionRule> rules;

    void init_rules();

    // Shared mesh-building helpers for rules
    static VolumeDescription make_sphere(vec3 pos, float r, int seg = 24);
    static VolumeDescription make_box(vec3 pos, vec3 scale);
    static VolumeDescription make_cylinder(vec3 pos, float r, float h, int seg = 24);
    static VolumeDescription make_cone(vec3 pos, float r, float h, int seg = 24);
    static VolumeDescription make_torus(vec3 pos, float major_r, float minor_r, int seg = 24);
    static VolumeDescription make_capsule(vec3 pos, float r, float h, int seg = 24);
    static VolumeDescription make_hemisphere(vec3 pos, float r, int seg = 24);
    static VolumeDescription make_ring(vec3 pos, float inner_r, float outer_r, int seg = 24);
    static VolumeDescription make_disc(vec3 pos, float r, float thickness, int seg = 24);
    static VolumeDescription make_wedge(vec3 pos, float r, float h, float angle, int seg = 24);
    static VolumeDescription make_arrow(vec3 pos, float shaft_r, float shaft_h, float head_r, float head_h, int seg = 12);
    static VolumeDescription make_plane(vec3 pos, vec3 scale);

    // Fallback: build a generic shape from the prompt shapes
    DecomposedObject build_generic(const ParsedPrompt& p);
};

// ============================================================
// OBJECT GENERATOR (orchestrates everything)
// ============================================================

class ObjectGenerator {
public:
    ObjectGenerator();
    Scene* generate_from_prompt(const std::string& prompt, Scene* scene = nullptr);
    std::string get_last_description() const;

private:
    PromptEngine prompt_engine;
    Decomposer decomposer;
    ParametricBuilder builder;
    std::mt19937 rng_engine{42};
    std::string last_description;

    void add_mesh_to_scene(Scene* scene, const Mesh& mesh,
                          const std::string& name, const Material& mat);
};

} // namespace ocp
