#pragma once
#include "mesh.h"
#include "types.h"
#include "camera.h"
#include <vector>
#include <functional>

namespace ocp {

class SceneNode {
public:
    std::string name;
    SceneNode* parent = nullptr;
    std::vector<SceneNode*> children;

    vec3 position = vec3(0);
    vec3 rotation = vec3(0); // Euler degrees
    vec3 scale = vec3(1);
    bool visible = true;
    bool locked = false;

    Mesh* mesh = nullptr;
    Material* material = nullptr;

    void add_child(SceneNode* child);
    void remove_child(SceneNode* child);
    void update_matrices(const mat4& parent_world = mat4(1.0f));

    mat4 get_local_matrix() const { return local_matrix; }
    mat4 get_world_matrix() const { return world_matrix; }

    void translate(float dx, float dy, float dz) { position += vec3(dx, dy, dz); }
    void rotate(float rx, float ry, float rz) { rotation += vec3(rx, ry, rz); }
    void scale_by(float sx, float sy, float sz) { scale *= vec3(sx, sy, sz); }

    vec3 get_world_position() const { return vec3(world_matrix[3]); }
    vec3 get_bounding_box_min() const;
    vec3 get_bounding_box_max() const;

    SceneNode* clone_recursive() const;
    void delete_children();

private:
    mat4 local_matrix = mat4(1.0f);
    mat4 world_matrix = mat4(1.0f);
};

class Scene {
public:
    std::string name = "Scene";
    SceneNode root;
    std::vector<Camera> cameras;
    std::vector<Light> lights;
    SceneNode* selected_node = nullptr;
    vec4 background_color = vec4(0.15f, 0.15f, 0.2f, 1.0f);
    bool grid_enabled = true;
    float grid_size = 20.0f;
    int grid_subdivisions = 20;

    Scene();

    SceneNode* add_node(SceneNode* node, SceneNode* parent = nullptr);
    void remove_node(SceneNode* node);
    std::vector<SceneNode*> get_all_nodes() const;
    SceneNode* find_node(const std::string& name) const;
    void update();
    void select(SceneNode* node) { selected_node = node; }
    void deselect() { selected_node = nullptr; }
    void delete_selected();
    void duplicate_selected();
    void clear();

    bool raycast(const vec3& origin, const vec3& direction, SceneNode*& hit_node, float& hit_dist) const;

private:
    bool raycast_node(const vec3& origin, const vec3& direction,
                      SceneNode* node, float& dist) const;
};

} // namespace ocp
