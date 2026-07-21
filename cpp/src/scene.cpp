#include "scene.h"
#include <algorithm>
#include <cmath>
#include <stack>

namespace ocp {

// SceneNode
void SceneNode::add_child(SceneNode* child) {
    child->parent = this;
    children.push_back(child);
}

void SceneNode::remove_child(SceneNode* child) {
    child->parent = nullptr;
    children.erase(std::remove(children.begin(), children.end(), child), children.end());
}

void SceneNode::update_matrices(const mat4& parent_world) {
    mat4 T = glm::translate(mat4(1.0f), position);
    mat4 Rx = glm::rotate(mat4(1.0f), deg2rad(rotation.x), vec3(1, 0, 0));
    mat4 Ry = glm::rotate(mat4(1.0f), deg2rad(rotation.y), vec3(0, 1, 0));
    mat4 Rz = glm::rotate(mat4(1.0f), deg2rad(rotation.z), vec3(0, 0, 1));
    mat4 S = glm::scale(mat4(1.0f), scale);
    local_matrix = T * Ry * Rx * Rz * S;
    world_matrix = parent_world * local_matrix;
    for (auto* c : children) c->update_matrices(world_matrix);
}

vec3 SceneNode::get_bounding_box_min() const {
    if (mesh && !mesh->vertices.empty()) {
        vec3 mn = mesh->get_bounding_box_min();
        mn = vec3(world_matrix * vec4(mn, 1.0f));
        return mn;
    }
    return position - glm::abs(scale);
}

vec3 SceneNode::get_bounding_box_max() const {
    if (mesh && !mesh->vertices.empty()) {
        vec3 mx = mesh->get_bounding_box_max();
        mx = vec3(world_matrix * vec4(mx, 1.0f));
        return mx;
    }
    return position + glm::abs(scale);
}

SceneNode* SceneNode::clone_recursive() const {
    SceneNode* n = new SceneNode();
    n->name = name;
    n->position = position;
    n->rotation = rotation;
    n->scale = scale;
    n->visible = visible;
    n->locked = locked;
    if (mesh) n->mesh = new Mesh(mesh->clone());
    if (material) {
        n->material = new Material();
        *n->material = *material;
    }
    for (auto* c : children) {
        SceneNode* cc = c->clone_recursive();
        n->add_child(cc);
    }
    return n;
}

void SceneNode::delete_children() {
    for (auto* c : children) { c->delete_children(); delete c; }
    children.clear();
}

// Scene
Scene::Scene() {
    root.name = "Root";
    Camera cam; cam.name = "Camera";
    cameras.push_back(cam);
    Light l1; l1.name = "Main Light"; l1.type = Light::DIRECTIONAL;
    l1.position = vec3(5, 10, 7); l1.direction = glm::normalize(vec3(-0.5f, -1.0f, -0.7f));
    lights.push_back(l1);
    Light l2; l2.name = "Fill Light"; l2.type = Light::POINT;
    l2.position = vec3(-3, 5, -2); l2.color = vec4(0.6f, 0.7f, 1.0f, 1.0f); l2.intensity = 0.5f;
    lights.push_back(l2);
    Light l3; l3.name = "Ambient"; l3.type = Light::AMBIENT;
    l3.color = vec4(0.15f, 0.15f, 0.2f, 1.0f);
    lights.push_back(l3);
}

SceneNode* Scene::add_node(SceneNode* node, SceneNode* parent) {
    if (!parent) parent = &root;
    parent->add_child(node);
    return node;
}

void Scene::remove_node(SceneNode* node) {
    if (node->parent) node->parent->remove_child(node);
    if (selected_node == node) selected_node = nullptr;
}

std::vector<SceneNode*> Scene::get_all_nodes() const {
    std::vector<SceneNode*> result;
    std::stack<SceneNode*> stk;
    for (auto* c : root.children) stk.push(c);
    while (!stk.empty()) {
        SceneNode* n = stk.top(); stk.pop();
        result.push_back(n);
        for (auto* c : n->children) stk.push(c);
    }
    return result;
}

SceneNode* Scene::find_node(const std::string& name) const {
    for (auto* n : get_all_nodes()) {
        if (n->name == name) return n;
    }
    return nullptr;
}

void Scene::update() {
    root.update_matrices();
}

void Scene::delete_selected() {
    if (selected_node) { remove_node(selected_node); selected_node = nullptr; }
}

void Scene::duplicate_selected() {
    if (!selected_node) return;
    SceneNode* dup = selected_node->clone_recursive();
    dup->position.x += 1.0f;
    if (selected_node->parent) selected_node->parent->add_child(dup);
    else root.add_child(dup);
    selected_node = dup;
}

void Scene::clear() {
    root.delete_children();
    cameras.clear();
    lights.clear();
    selected_node = nullptr;
    Camera cam; cam.name = "Camera";
    cameras.push_back(cam);
    Light l1; l1.name = "Main Light"; l1.type = Light::DIRECTIONAL;
    l1.position = vec3(5, 10, 7); l1.direction = glm::normalize(vec3(-0.5f, -1.0f, -0.7f));
    lights.push_back(l1);
    Light l2; l2.name = "Fill Light"; l2.type = Light::POINT;
    l2.position = vec3(-3, 5, -2); l2.intensity = 0.5f;
    lights.push_back(l2);
    Light l3; l3.name = "Ambient"; l3.type = Light::AMBIENT;
    lights.push_back(l3);
}

bool Scene::raycast(const vec3& origin, const vec3& direction,
                    SceneNode*& hit_node, float& hit_dist) const {
    hit_node = nullptr; hit_dist = 1e30f;
    for (auto* c : root.children) {
        float d;
        if (raycast_node(origin, direction, c, d) && d < hit_dist) {
            hit_dist = d; hit_node = c;
        }
    }
    return hit_node != nullptr;
}

bool Scene::raycast_node(const vec3& origin, const vec3& direction,
                         SceneNode* node, float& dist) const {
    if (!node->visible) return false;
    vec3 bmin = node->get_bounding_box_min();
    vec3 bmax = node->get_bounding_box_max();
    vec3 inv_dir = 1.0f / (direction + vec3(1e-8f));
    vec3 t1 = (bmin - origin) * inv_dir;
    vec3 t2 = (bmax - origin) * inv_dir;
    vec3 tmin = glm::min(t1, t2);
    vec3 tmax = glm::max(t1, t2);
    float t_near = std::max({tmin.x, tmin.y, tmin.z});
    float t_far = std::min({tmax.x, tmax.y, tmax.z});
    if (t_near <= t_far && t_far > 0) {
        dist = t_near > 0 ? t_near : t_far;
        return true;
    }
    return false;
}

} // namespace ocp
