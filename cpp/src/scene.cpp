#include "scene.h"
#include <algorithm>
#include <cmath>
#include <stack>
#include <sstream>

namespace ocp {

// SceneNode
SceneNode* SceneNode::add_child(std::unique_ptr<SceneNode> child) {
    child->parent = this;
    SceneNode* raw = child.get();
    children.push_back(std::move(child));
    return raw;
}

void SceneNode::remove_child(SceneNode* child) {
    if (!child) return;
    child->parent = nullptr;
    children.erase(
        std::remove_if(children.begin(), children.end(),
            [child](const std::unique_ptr<SceneNode>& c) { return c.get() == child; }),
        children.end());
}

void SceneNode::update_matrices(const mat4& parent_world) {
    mat4 T = glm::translate(mat4(1.0f), position);
    mat4 Rx = glm::rotate(mat4(1.0f), deg2rad(rotation.x), vec3(1, 0, 0));
    mat4 Ry = glm::rotate(mat4(1.0f), deg2rad(rotation.y), vec3(0, 1, 0));
    mat4 Rz = glm::rotate(mat4(1.0f), deg2rad(rotation.z), vec3(0, 0, 1));
    mat4 S = glm::scale(mat4(1.0f), scale);
    local_matrix = T * Ry * Rx * Rz * S;
    world_matrix = parent_world * local_matrix;
    for (auto& c : children) c->update_matrices(world_matrix);
}

vec3 SceneNode::get_bounding_box_min() const {
    if (mesh && !mesh->vertices.empty()) {
        vec3 mn = mesh->get_bounding_box_min();
        mn = vec3(world_matrix * vec4(mn, 1.0f));
        return mn;
    }
    vec3 mn = position;
    for (auto& c : children) {
        vec3 cmn = c->get_bounding_box_min();
        mn = glm::min(mn, cmn);
    }
    return children.empty() ? position - glm::abs(scale) : mn;
}

vec3 SceneNode::get_bounding_box_max() const {
    if (mesh && !mesh->vertices.empty()) {
        vec3 mx = mesh->get_bounding_box_max();
        mx = vec3(world_matrix * vec4(mx, 1.0f));
        return mx;
    }
    vec3 mx = position;
    for (auto& c : children) {
        vec3 cmx = c->get_bounding_box_max();
        mx = glm::max(mx, cmx);
    }
    return children.empty() ? position + glm::abs(scale) : mx;
}

std::unique_ptr<SceneNode> SceneNode::clone_recursive() const {
    auto n = std::make_unique<SceneNode>();
    n->name = name + "_copy";
    n->position = position;
    n->rotation = rotation;
    n->scale = scale;
    n->visible = visible;
    n->locked = locked;
    if (mesh) n->mesh = std::make_unique<Mesh>(mesh->clone());
    if (material) {
        n->material = std::make_unique<Material>();
        *n->material = *material;
    }
    for (auto& c : children) {
        SceneNode* cc = n->add_child(c->clone_recursive());
        (void)cc;
    }
    return n;
}

void SceneNode::delete_children() {
    for (auto& c : children) {
        c->delete_children();
    }
    children.clear();
}

SceneNode* SceneNode::find_child(const std::string& nm) const {
    for (auto& c : children) {
        if (c->name == nm) return c.get();
        SceneNode* found = c->find_child(nm);
        if (found) return found;
    }
    return nullptr;
}

// Scene
Scene::Scene() {
    root.name = "Root";
}

SceneNode* Scene::add_node(std::unique_ptr<SceneNode> node, SceneNode* parent) {
    if (!parent) parent = &root;
    SceneNode* raw = parent->add_child(std::move(node));
    return raw;
}

SceneNode* Scene::add_node_raw(SceneNode* node, SceneNode* parent) {
    if (!parent) parent = &root;
    std::unique_ptr<SceneNode> ptr(node);
    return parent->add_child(std::move(ptr));
}

void Scene::remove_node(SceneNode* node) {
    if (!node || node == &root) return;
    if (node->parent) node->parent->remove_child(node);
    if (selected_node == node) selected_node = nullptr;
    multi_selection.erase(
        std::remove(multi_selection.begin(), multi_selection.end(), node),
        multi_selection.end());
}

std::vector<SceneNode*> Scene::get_all_nodes() const {
    std::vector<SceneNode*> result;
    std::stack<SceneNode*> stk;
    for (auto& c : root.children) stk.push(c.get());
    while (!stk.empty()) {
        SceneNode* n = stk.top(); stk.pop();
        result.push_back(n);
        for (auto& c : n->children) stk.push(c.get());
    }
    return result;
}

SceneNode* Scene::find_node(const std::string& nm) const {
    for (auto* n : get_all_nodes()) {
        if (n->name == nm) return n;
    }
    return nullptr;
}

int Scene::get_node_count() const {
    return (int)get_all_nodes().size();
}

void Scene::update() {
    root.update_matrices();
}

void Scene::select(SceneNode* node) {
    selected_node = node;
    multi_selection.clear();
    if (node) multi_selection.push_back(node);
}

void Scene::toggle_multi_select(SceneNode* node) {
    if (!node) return;
    for (auto it = multi_selection.begin(); it != multi_selection.end(); ++it) {
        if (*it == node) {
            multi_selection.erase(it);
            if (selected_node == node) {
                selected_node = multi_selection.empty() ? nullptr : multi_selection.back();
            }
            return;
        }
    }
    multi_selection.push_back(node);
    selected_node = node;
}

void Scene::deselect() {
    selected_node = nullptr;
    multi_selection.clear();
}

void Scene::delete_selected() {
    if (selected_node) {
        remove_node(selected_node);
        selected_node = nullptr;
    }
}

void Scene::duplicate_selected() {
    if (!selected_node) return;
    auto dup = selected_node->clone_recursive();
    SceneNode* dup_raw = dup.get();
    dup_raw->position.x += 1.0f;
    if (selected_node->parent) {
        selected_node->parent->add_child(std::move(dup));
    } else {
        root.add_child(std::move(dup));
    }
    select(dup_raw);
}

void Scene::copy_selected() {
    clipboard.clear();
    for (auto* node : multi_selection) {
        clipboard.push_back(node->clone_recursive());
    }
    if (clipboard.empty() && selected_node) {
        clipboard.push_back(selected_node->clone_recursive());
    }
}

void Scene::paste_clipboard() {
    if (clipboard.empty()) return;
    std::vector<SceneNode*> new_nodes;
    for (auto& c : clipboard) {
        auto dup = c->clone_recursive();
        SceneNode* raw = dup.get();
        raw->position.x += 1.5f;
        root.add_child(std::move(dup));
        new_nodes.push_back(raw);
    }
    if (!new_nodes.empty()) {
        select(new_nodes.front());
    }
}

void Scene::group_selected() {
    if (multi_selection.size() < 2) return;
    auto group = std::make_unique<SceneNode>();
    group->name = "Group";
    SceneNode* group_raw = group.get();
    SceneNode* first_parent = multi_selection[0]->parent ? multi_selection[0]->parent : &root;
    first_parent->add_child(std::move(group));
    std::vector<SceneNode*> to_move = multi_selection;
    for (auto* node : to_move) {
        if (!node->parent) continue;
        vec3 world_pos = node->get_world_position();
        node->parent->remove_child(node);
        node->parent = nullptr;
        node->position = world_pos;
        auto ptr = std::unique_ptr<SceneNode>(node);
        group_raw->add_child(std::move(ptr));
    }
    select(group_raw);
}

void Scene::ungroup_selected() {
    if (!selected_node || selected_node->children.empty()) return;
    SceneNode* parent = selected_node->parent ? selected_node->parent : &root;
    vec3 group_world = selected_node->get_world_position();
    std::vector<std::unique_ptr<SceneNode>> kids;
    for (auto& c : selected_node->children) {
        vec3 child_world = c->get_world_position();
        c->position = child_world - group_world;
        kids.push_back(std::move(c));
    }
    selected_node->children.clear();
    SceneNode* to_remove = selected_node;
    for (auto& k : kids) {
        SceneNode* raw = k.get();
        parent->add_child(std::move(k));
    }
    parent->remove_child(to_remove);
    select(parent->children.empty() ? nullptr : parent->children.back().get());
}

void Scene::clear() {
    root.delete_children();
    cameras.clear();
    lights.clear();
    selected_node = nullptr;
    multi_selection.clear();
}

bool Scene::raycast(const vec3& origin, const vec3& direction,
                    SceneNode*& hit_node, float& hit_dist) const {
    hit_node = nullptr;
    hit_dist = 1e30f;
    for (auto& c : root.children) {
        float d;
        if (raycast_node(origin, direction, c.get(), d) && d < hit_dist) {
            hit_dist = d;
            hit_node = c.get();
        }
    }
    return hit_node != nullptr;
}

bool Scene::raycast_node(const vec3& origin, const vec3& direction,
                         SceneNode* node, float& dist) const {
    if (!node || !node->visible) return false;

    bool hit_this = false;
    float this_dist = 1e30f;

    if (node->mesh && node->mesh->get_vertex_count() > 0) {
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
            this_dist = t_near > 0 ? t_near : t_far;
            hit_this = true;
        }
    }

    for (auto& c : node->children) {
        float child_dist;
        if (raycast_node(origin, direction, c.get(), child_dist)) {
            if (child_dist < this_dist) {
                this_dist = child_dist;
                hit_this = true;
                dist = this_dist;
                return true;
            }
        }
    }

    if (hit_this) {
        dist = this_dist;
        return true;
    }
    return false;
}

std::string Scene::save_to_string() const {
    std::ostringstream ss;
    ss << "OCP_SCENE_V1\n";
    ss << "name:" << name << "\n";
    ss << "bg:" << background_color.r << "," << background_color.g << ","
       << background_color.b << "," << background_color.a << "\n";
    ss << "grid:" << (grid_enabled ? 1 : 0) << "," << grid_size << "," << grid_subdivisions << "\n";

    ss << "cameras:" << cameras.size() << "\n";
    for (auto& c : cameras) {
        ss << "  " << c.name << "|" << c.position.x << "," << c.position.y << "," << c.position.z
           << "|" << c.target.x << "," << c.target.y << "," << c.target.z
           << "|" << c.fov << "\n";
    }

    ss << "lights:" << lights.size() << "\n";
    for (auto& l : lights) {
        ss << "  " << l.name << "|t:" << (int)l.type
           << "|p:" << l.position.x << "," << l.position.y << "," << l.position.z
           << "|d:" << l.direction.x << "," << l.direction.y << "," << l.direction.z
           << "|c:" << l.color.r << "," << l.color.g << "," << l.color.b
           << "|i:" << l.intensity << "|e:" << (l.enabled ? 1 : 0) << "\n";
    }

    std::function<void(const SceneNode&, int)> save_node = [&](const SceneNode& n, int depth) {
        ss << std::string(depth * 2, ' ') << "node:" << n.name
           << "|p:" << n.position.x << "," << n.position.y << "," << n.position.z
           << "|r:" << n.rotation.x << "," << n.rotation.y << "," << n.rotation.z
           << "|s:" << n.scale.x << "," << n.scale.y << "," << n.scale.z
           << "|v:" << (n.visible ? 1 : 0) << "\n";
        if (n.material) {
            ss << std::string(depth * 2 + 2, ' ') << "mat:"
               << n.material->to_hex()
               << "|m:" << n.material->metallic
               << "|r:" << n.material->roughness
               << "|sh:" << n.material->shininess
               << "|w:" << (n.material->wireframe ? 1 : 0)
               << "|o:" << n.material->opacity << "\n";
        }
        for (auto& c : n.children) save_node(*c, depth + 1);
    };

    ss << "nodes:" << root.children.size() << "\n";
    for (auto& c : root.children) save_node(*c, 0);

    return ss.str();
}

bool Scene::load_from_string(const std::string& data) {
    clear();
    std::istringstream ss(data);
    std::string line;
    std::getline(ss, line);
    if (line.find("OCP_SCENE_V1") == std::string::npos) return false;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        if (line.substr(0, 5) == "name:") name = line.substr(5);
        else if (line.substr(0, 3) == "bg:") {
            sscanf(line.c_str(), "bg:%f,%f,%f,%f",
                &background_color.r, &background_color.g, &background_color.b, &background_color.a);
        }
        else if (line.substr(0, 5) == "grid:") {
            int g; sscanf(line.c_str(), "grid:%d,%f,%d", &g, &grid_size, &grid_subdivisions);
            grid_enabled = g != 0;
        }
        else if (line.substr(0, 8) == "lights:") {
            int count; sscanf(line.c_str(), "lights:%d", &count);
            for (int i = 0; i < count; i++) {
                std::getline(ss, line);
                Light l;
                int t, e;
                char name_buf[64] = {};
                sscanf(line.c_str(), "  %[^|]|t:%d|p:%f,%f,%f|d:%f,%f,%f|c:%f,%f,%f|i:%f|e:%d",
                    name_buf, &t, &l.position.x, &l.position.y, &l.position.z,
                    &l.direction.x, &l.direction.y, &l.direction.z,
                    &l.color.r, &l.color.g, &l.color.b, &l.intensity, &e);
                l.name = name_buf;
                l.type = (Light::Type)t;
                l.enabled = e != 0;
                lights.push_back(l);
            }
        }
    }
    return true;
}

} // namespace ocp
