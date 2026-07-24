#pragma once
#include "mesh.h"
#include "types.h"
#include "camera.h"
#include <vector>
#include <functional>
#include <memory>
#include <unordered_set>
#include <stack>

namespace ocp {

class SceneNode {
public:
    std::string name;
    SceneNode* parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> children;

    vec3 position = vec3(0);
    vec3 rotation = vec3(0);
    vec3 scale = vec3(1);
    bool visible = true;
    bool locked = false;

    std::unique_ptr<Mesh> mesh;
    std::unique_ptr<Material> material;

    SceneNode() = default;
    ~SceneNode() = default;
    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;
    SceneNode(SceneNode&&) = default;
    SceneNode& operator=(SceneNode&&) = default;

    SceneNode* add_child(std::unique_ptr<SceneNode> child);
    void remove_child(SceneNode* child);
    std::unique_ptr<SceneNode> steal_child(SceneNode* child);
    void update_matrices(const mat4& parent_world = mat4(1.0f));

    mat4 get_local_matrix() const { return local_matrix; }
    mat4 get_world_matrix() const { return world_matrix; }

    void translate(float dx, float dy, float dz) { position += vec3(dx, dy, dz); }
    void rotate(float rx, float ry, float rz) { rotation += vec3(rx, ry, rz); }
    void scale_by(float sx, float sy, float sz) { scale *= vec3(sx, sy, sz); }

    vec3 get_world_position() const { return vec3(world_matrix[3]); }
    vec3 get_bounding_box_min() const;
    vec3 get_bounding_box_max() const;

    std::unique_ptr<SceneNode> clone_recursive() const;
    void delete_children();

    SceneNode* find_child(const std::string& name) const;

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
    std::vector<SceneNode*> multi_selection;
    vec4 background_color = vec4(0.15f, 0.15f, 0.2f, 1.0f);
    bool grid_enabled = true;
    float grid_size = 20.0f;
    int grid_subdivisions = 20;
    bool global_wireframe = false;
    bool xray_mode = false;

    Scene();

    SceneNode* add_node(std::unique_ptr<SceneNode> node, SceneNode* parent = nullptr);
    SceneNode* add_node_raw(SceneNode* node, SceneNode* parent = nullptr);
    void remove_node(SceneNode* node);
    std::vector<SceneNode*> get_all_nodes() const;
    SceneNode* find_node(const std::string& name) const;
    int get_node_count() const;
    void update();
    void select(SceneNode* node);
    void toggle_multi_select(SceneNode* node);
    void deselect();
    void delete_selected();
    void duplicate_selected();
    void clear();

    void copy_selected();
    void paste_clipboard();
    void group_selected();
    void ungroup_selected();

    bool raycast(const vec3& origin, const vec3& direction, SceneNode*& hit_node, float& hit_dist) const;

    std::string save_to_string() const;
    bool load_from_string(const std::string& data);

private:
    std::vector<std::unique_ptr<SceneNode>> clipboard;
    bool raycast_node(const vec3& origin, const vec3& direction,
                      SceneNode* node, float& dist) const;
};

} // namespace ocp
