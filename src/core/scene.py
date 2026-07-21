import numpy as np
from ..utils.math_utils import mat4_identity, mat4_translate, mat4_scale, mat4_rotate_euler


class SceneNode:
    def __init__(self, name="Node", parent=None):
        self.name = name
        self.parent = parent
        self.children = []
        self.position = np.zeros(3, dtype=np.float32)
        self.rotation = np.zeros(3, dtype=np.float32)
        self.scale = np.ones(3, dtype=np.float32)
        self.visible = True
        self.locked = False
        self.mesh = None
        self.material = None
        self._local_matrix = mat4_identity()
        self._world_matrix = mat4_identity()
        self._dirty = True

    def add_child(self, child):
        child.parent = self
        self.children.append(child)

    def remove_child(self, child):
        child.parent = None
        self.children.remove(child)

    def get_children(self):
        return self.children

    def set_dirty(self):
        self._dirty = True
        for child in self.children:
            child.set_dirty()

    def update_matrices(self, parent_world=None):
        self._local_matrix = (
            mat4_translate(*self.position)
            @ mat4_rotate_euler(*np.radians(self.rotation))
            @ mat4_scale(*self.scale)
        )
        if parent_world is not None:
            self._world_matrix = parent_world @ self._local_matrix
        else:
            self._world_matrix = self._local_matrix.copy()
        self._dirty = False
        for child in self.children:
            child.update_matrices(self._world_matrix)

    def get_world_matrix(self):
        return self._world_matrix

    def get_local_matrix(self):
        return self._local_matrix

    def translate(self, dx, dy, dz):
        self.position += np.array([dx, dy, dz], dtype=np.float32)
        self.set_dirty()

    def rotate(self, rx, ry, rz):
        self.rotation += np.array([rx, ry, rz], dtype=np.float32)
        self.set_dirty()

    def scale_by(self, sx, sy, sz):
        self.scale *= np.array([sx, sy, sz], dtype=np.float32)
        self.set_dirty()

    def set_position(self, x, y, z):
        self.position = np.array([x, y, z], dtype=np.float32)
        self.set_dirty()

    def set_rotation(self, rx, ry, rz):
        self.rotation = np.array([rx, ry, rz], dtype=np.float32)
        self.set_dirty()

    def set_scale(self, sx, sy, sz):
        self.scale = np.array([sx, sy, sz], dtype=np.float32)
        self.set_dirty()

    def look_at(self, target):
        direction = np.asarray(target, dtype=np.float32) - self.position
        direction = direction / (np.linalg.norm(direction) + 1e-8)
        yaw = np.degrees(np.arctan2(direction[2], direction[0]))
        pitch = np.degrees(np.arcsin(direction[1]))
        self.rotation = np.array([pitch, yaw, 0.0], dtype=np.float32)
        self.set_dirty()

    def get_world_position(self):
        return self._world_matrix[:3, 3]

    def get_bounding_box(self):
        if self.mesh is None:
            bb_min = self.position - np.abs(self.scale)
            bb_max = self.position + np.abs(self.scale)
            return bb_min, bb_max
        local_min, local_max = self.mesh.get_bounding_box()
        corners = np.array([
            [local_min[0], local_min[1], local_min[2]],
            [local_max[0], local_min[1], local_min[2]],
            [local_min[0], local_max[1], local_min[2]],
            [local_max[0], local_max[1], local_min[2]],
            [local_min[0], local_min[1], local_max[2]],
            [local_max[0], local_min[1], local_max[2]],
            [local_min[0], local_max[1], local_max[2]],
            [local_max[0], local_max[1], local_max[2]],
        ])
        world_corners = []
        for c in corners:
            p = np.append(c, 1.0)
            wp = self._world_matrix @ p
            world_corners.append(wp[:3])
        world_corners = np.array(world_corners)
        return np.min(world_corners, axis=0), np.max(world_corners, axis=0)

    def clone(self, recursive=True):
        node = SceneNode(self.name + "_copy")
        node.position = self.position.copy()
        node.rotation = self.rotation.copy()
        node.scale = self.scale.copy()
        node.visible = self.visible
        node.locked = self.locked
        if self.mesh is not None:
            node.mesh = self.mesh.clone()
        if self.material is not None:
            node.material = self.material.clone()
        if recursive:
            for child in self.children:
                node.add_child(child.clone(True))
        return node


class Scene:
    def __init__(self, name="Scene"):
        self.name = name
        self.root = SceneNode("Root")
        self.cameras = []
        self.lights = []
        self.selected_node = None
        self.background_color = np.array([0.15, 0.15, 0.2, 1.0], dtype=np.float32)
        self.grid_enabled = True
        self.grid_size = 20.0
        self.grid_subdivisions = 20

    def add_node(self, node, parent=None):
        if parent is None:
            parent = self.root
        parent.add_child(node)
        return node

    def remove_node(self, node):
        if node.parent is not None:
            node.parent.remove_child(node)

    def get_all_nodes(self):
        nodes = []
        self._collect_nodes(self.root, nodes)
        return nodes

    def _collect_nodes(self, node, result):
        for child in node.children:
            result.append(child)
            self._collect_nodes(child, result)

    def find_node(self, name):
        for node in self.get_all_nodes():
            if node.name == name:
                return node
        return None

    def update(self):
        self.root.update_matrices()

    def add_camera(self, camera=None):
        if camera is None:
            from .camera import Camera
            camera = Camera("Camera_" + str(len(self.cameras)))
        self.cameras.append(camera)
        return camera

    def add_light(self, light=None):
        if light is None:
            from .light import Light
            light = Light("Light_" + str(len(self.lights)))
        self.lights.append(light)
        return light

    def select(self, node):
        self.selected_node = node

    def deselect(self):
        self.selected_node = None

    def duplicate_selected(self):
        if self.selected_node is not None:
            dup = self.selected_node.clone()
            dup.position[0] += 1.0
            self.add_node(dup)
            self.select(dup)
            return dup
        return None

    def delete_selected(self):
        if self.selected_node is not None:
            parent = self.selected_node.parent or self.root
            self.remove_node(self.selected_node)
            self.selected_node = None

    def clear(self):
        self.root = SceneNode("Root")
        self.cameras.clear()
        self.lights.clear()
        self.selected_node = None

    def get_node_count(self):
        return len(self.get_all_nodes())

    def raycast(self, origin, direction):
        origin = np.asarray(origin, dtype=np.float32)
        direction = np.asarray(direction, dtype=np.float32)
        closest_hit = None
        closest_dist = float('inf')
        for node in self.get_all_nodes():
            if node.mesh is None or not node.visible:
                continue
            hit_dist = self._raycast_node(origin, direction, node)
            if hit_dist is not None and hit_dist < closest_dist:
                closest_dist = hit_dist
                closest_hit = node
        if closest_hit is not None:
            return closest_hit, closest_dist
        return None, None

    def _raycast_node(self, origin, direction, node):
        bb_min, bb_max = node.get_bounding_box()
        inv_dir = 1.0 / (direction + 1e-8)
        t1 = (bb_min - origin) * inv_dir
        t2 = (bb_max - origin) * inv_dir
        tmin = np.minimum(t1, t2)
        tmax = np.maximum(t1, t2)
        t_near = np.max(tmin)
        t_far = np.min(tmax)
        if t_near <= t_far and t_far > 0:
            return t_near if t_near > 0 else t_far
        return None
