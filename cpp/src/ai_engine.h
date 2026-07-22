#pragma once
#include "scene.h"
#include <string>
#include <vector>
#include <map>
#include <random>

namespace ocp {

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
    uint32_t seed = 0;
};

class PromptEngine {
public:
    ParsedPrompt parse(const std::string& prompt);
    std::string get_description(const ParsedPrompt& p);
};

class ProceduralEngine {
public:
    struct Part {
        std::string name;
        Mesh mesh;
        Material material;
        vec3 position = vec3(0);
        vec3 rotation = vec3(0);
        vec3 scale = vec3(1);
    };

    std::mt19937& rng();

    // === ORIGINAL SCENE GENERATORS (ORGANIC REWRITE) ===
    std::vector<Part> generate_tree(float height = 3.0f);
    std::vector<Part> generate_house(float size = 1.0f);
    std::vector<Part> generate_car(float size = 1.0f);
    std::vector<Part> generate_robot(float size = 1.0f);
    std::vector<Part> generate_castle(float size = 1.0f);
    std::vector<Part> generate_spaceship(float size = 1.0f);
    std::vector<Part> generate_sword(float size = 1.0f);
    std::vector<Part> generate_chair(float size = 1.0f);
    std::vector<Part> generate_table(float size = 1.0f);
    std::vector<Part> generate_star(float size = 1.0f);
    std::vector<Part> generate_heart(float size = 1.0f);
    std::vector<Part> generate_mountain(float size = 1.0f);
    std::vector<Part> generate_snowman(float size = 1.0f);
    std::vector<Part> generate_diamond(float size = 1.0f);
    std::vector<Part> generate_spiral(float size = 1.0f);
    std::vector<Part> generate_dna(float size = 1.0f);
    std::vector<Part> generate_skull(float size = 1.0f);
    std::vector<Part> generate_airplane(float size = 1.0f);
    std::vector<Part> generate_boat(float size = 1.0f);
    std::vector<Part> generate_door(float size = 1.0f);
    std::vector<Part> generate_window(float size = 1.0f);
    std::vector<Part> generate_shelf(float size = 1.0f);
    std::vector<Part> generate_lamp(float size = 1.0f);
    std::vector<Part> generate_brain(float size = 1.0f);
    std::vector<Part> generate_flower(float size = 1.0f);
    std::vector<Part> generate_bone(float size = 1.0f);
    std::vector<Part> generate_key(float size = 1.0f);
    std::vector<Part> generate_helmet(float size = 1.0f);
    std::vector<Part> generate_battery(float size = 1.0f);
    std::vector<Part> generate_ice_crystal(float size = 1.0f);
    std::vector<Part> generate_mushroom(float size = 1.0f);
    std::vector<Part> generate_pyramid(float size = 1.0f);
    std::vector<Part> generate_donut(float size = 1.0f);
    std::vector<Part> generate_satellite(float size = 1.0f);
    std::vector<Part> generate_jewelry(float size = 1.0f);

    // === NEW SCENE GENERATORS ===
    std::vector<Part> generate_throne(float size = 1.0f);
    std::vector<Part> generate_altar(float size = 1.0f);
    std::vector<Part> generate_campfire(float size = 1.0f);
    std::vector<Part> generate_crate(float size = 1.0f);
    std::vector<Part> generate_barrel(float size = 1.0f);
    std::vector<Part> generate_bench(float size = 1.0f);
    std::vector<Part> generate_fountain(float size = 1.0f);
    std::vector<Part> generate_cart(float size = 1.0f);
    std::vector<Part> generate_lantern(float size = 1.0f);
    std::vector<Part> generate_gun(float size = 1.0f);
    std::vector<Part> generate_scifi_turret(float size = 1.0f);
    std::vector<Part> generate_crystal(float size = 1.0f);
    std::vector<Part> generate_coral(float size = 1.0f);
    std::vector<Part> generate_butterfly(float size = 1.0f);
    std::vector<Part> generate_cat(float size = 1.0f);
    std::vector<Part> generate_dog(float size = 1.0f);
    std::vector<Part> generate_bird(float size = 1.0f);
    std::vector<Part> generate_tree_stump(float size = 1.0f);
    std::vector<Part> generate_rock_wall(float size = 1.0f);
    std::vector<Part> generate_grave(float size = 1.0f);
    std::vector<Part> generate_flag(float size = 1.0f);
    std::vector<Part> generate_bookshelf(float size = 1.0f);
    std::vector<Part> generate_potion(float size = 1.0f);
    std::vector<Part> generate_cave(float size = 1.0f);
    std::vector<Part> generate_tent(float size = 1.0f);

private:
    std::mt19937 rng_engine{42};

    // --- Deformation Utilities ---
    void deform_organic(Mesh& m, float amount, float seed);
    void bend_mesh(Mesh& m, float amount, vec3 axis);
    void taper_mesh(Mesh& m, float top_scale, float bottom_scale);
    void add_random_detail(Mesh& m, float detail_size, int count);

    // --- Organic Mesh Creators ---
    Mesh create_organic_sphere(float radius, int seg, int rings, float seed, float bumpiness);
    Mesh create_organic_cylinder(float r, float h, int seg, float seed, float bulge);
    Mesh create_stone(float size, float seed = 0.0f);
    Mesh create_log(float radius, float length, float seed = 0.0f);
    Mesh create_leaf_cluster(float size, float seed = 0.0f);
    Mesh create_rock(float size, float seed = 0.0f);
    Mesh create_cloud(float size, float seed = 0.0f);
    Mesh create_cave_entrance(float size, float seed = 0.0f);
    Mesh create_tent_mesh(float size, float seed = 0.0f);
    Mesh create_barrel_mesh(float size, float seed = 0.0f);
    Mesh create_bench_mesh(float size, float seed = 0.0f);
    Mesh create_fountain_mesh(float size, float seed = 0.0f);
    Mesh create_cart_mesh(float size, float seed = 0.0f);
    Mesh create_crate_mesh(float size, float seed = 0.0f);
    Mesh create_lantern_mesh(float size, float seed = 0.0f);
    Mesh create_gun_mesh(float size, float seed = 0.0f);
    Mesh create_scifi_turret_mesh(float size, float seed = 0.0f);
    Mesh create_crystal_mesh(float size, float seed = 0.0f);
    Mesh create_coral_mesh(float size, float seed = 0.0f);
    Mesh create_butterfly_mesh(float size, float seed = 0.0f);
    Mesh create_cat_mesh(float size, float seed = 0.0f);
    Mesh create_dog_mesh(float size, float seed = 0.0f);
    Mesh create_bird_mesh(float size, float seed = 0.0f);
    Mesh create_tree_stump_mesh(float size, float seed = 0.0f);
    Mesh create_rock_wall_mesh(float size, float seed = 0.0f);
    Mesh create_grave_mesh(float size, float seed = 0.0f);
    Mesh create_flag_mesh(float size, float seed = 0.0f);
    Mesh create_bookshelf_mesh(float size, float seed = 0.0f);
    Mesh create_potion_mesh(float size, float seed = 0.0f);
    Mesh create_campfire_mesh(float size, float seed = 0.0f);
    Mesh create_throne_mesh(float size, float seed = 0.0f);
    Mesh create_altar_mesh(float size, float seed = 0.0f);

    // --- Internal Mesh Helpers ---
    Mesh create_tapered_cylinder(float r_top, float r_bot, float height, int seg = 16);
    Mesh create_pyramid_mesh(float base_size, float height, int sides = 4);
    Mesh create_star_mesh(float size);
    Mesh create_heart_mesh(float size);
    Mesh create_skull_mesh(float size);
    Mesh create_brain_mesh(float size);
    Mesh create_l_shape(float leg1, float leg2, float thickness);
    Mesh create_ring(float outer_r, float inner_r, int segs);
    void merge_mesh(Mesh& target, const Mesh& source, const vec3& offset);
    void merge_mesh_transform(Mesh& target, const Mesh& source, const mat4& transform);
    float randf();
};

class ObjectGenerator {
public:
    ObjectGenerator();
    Scene* generate_from_prompt(const std::string& prompt, Scene* scene = nullptr);
    std::string get_last_description() const;

private:
    PromptEngine prompt_engine;
    ProceduralEngine proc_engine;
    std::string last_description;

    void add_parts_to_scene(Scene* scene, const std::vector<ProceduralEngine::Part>& parts);
    Material create_material(const vec4& color, const std::string& type);
    void apply_arrangement(std::vector<SceneNode*>& nodes, const ParsedPrompt& p, Scene* scene);
};

} // namespace ocp
