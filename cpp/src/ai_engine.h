#pragma once
#include "scene.h"
#include <string>
#include <vector>
#include <map>

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
    std::vector<Part> generate_tree(float height = 3.0f);
    std::vector<Part> generate_house(float size = 1.0f);
    std::vector<Part> generate_car(float size = 1.0f);
    std::vector<Part> generate_robot(float size = 1.0f);
    std::vector<Part> generate_castle(float size = 1.0f);
    std::vector<Part> generate_spaceship(float size = 1.0f);
    std::vector<Part> generate_sword(float size = 1.0f);
    std::vector<Part> generate_chair(float size = 1.0f);
    std::vector<Part> generate_star(float size = 1.0f);
    std::vector<Part> generate_heart(float size = 1.0f);
    std::vector<Part> generate_mountain(float size = 1.0f);
    std::vector<Part> generate_snowman(float size = 1.0f);
    std::vector<Part> generate_diamond(float size = 1.0f);
    std::vector<Part> generate_spiral(float size = 1.0f);
    std::vector<Part> generate_dna(float size = 1.0f);
    std::vector<Part> generate_skull(float size = 1.0f);

private:
    Mesh create_tapered_cylinder(float r_top, float r_bot, float height, int seg = 16);
    Mesh create_pyramid(float base_size, float height, int sides = 4);
    Mesh create_deformed_sphere(float radius, int seg = 24, int rings = 16, float deform = 0.3f);
    Mesh create_star_mesh(float size);
    Mesh create_heart_mesh(float size);
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
