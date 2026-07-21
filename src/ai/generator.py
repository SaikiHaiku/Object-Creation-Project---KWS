import numpy as np
import math
from ..core.scene import Scene, SceneNode
from ..core.material import Material
from ..core.primitives import (
    create_cube, create_sphere, create_cylinder, create_cone,
    create_torus, create_plane, create_torus_knot, create_ico_sphere
)
from .prompt_engine import PromptEngine
from .procedural import ProceduralEngine


class ObjectGenerator:
    def __init__(self):
        self.prompt_engine = PromptEngine()
        self.procedural_engine = ProceduralEngine()
        self._creation_history = []

    def generate_from_prompt(self, prompt, scene=None):
        if scene is None:
            scene = Scene("Generated Scene")

        parsed = self.prompt_engine.parse_prompt(prompt)
        nodes_created = []

        if parsed['scene_type']:
            nodes_created = self._generate_scene_type(parsed['scene_type'], parsed, scene)
        else:
            for shape in parsed['shapes']:
                nodes = self._generate_shape(shape, parsed, scene)
                nodes_created.extend(nodes)

        if parsed['count'] > 1 and nodes_created:
            self._apply_arrangement(nodes_created, parsed, scene)

        for mod in parsed['modifiers']:
            self._apply_modifier(mod, nodes_created, scene)

        self._creation_history.append({
            'prompt': prompt,
            'parsed': parsed,
            'nodes': nodes_created,
        })

        scene.update()
        return scene, nodes_created

    def _generate_shape(self, shape_name, parsed, scene):
        nodes = []
        size = parsed['size']
        color = parsed['color']
        mat_type = parsed['material_type']

        shape_creators = {
            'cube': lambda s: create_cube(s),
            'sphere': lambda s: create_sphere(s),
            'cylinder': lambda s: create_cylinder(s * 0.5, s),
            'cone': lambda s: create_cone(s * 0.5, s),
            'torus': lambda s: create_torus(s * 0.4, s * 0.15),
            'plane': lambda s: create_plane(s * 2),
            'torus_knot': lambda s: create_torus_knot(2, 3, s * 0.4, s * 0.08),
            'ico_sphere': lambda s: create_ico_sphere(s * 0.5, 2),
            'arrow': lambda s: create_arrow(s * 1.5),
        }

        if shape_name in shape_creators:
            mesh = shape_creators[shape_name](size)
            material = self._create_material(color, mat_type)
            node = SceneNode(shape_name.capitalize())
            node.mesh = mesh
            node.material = material
            node.position = np.array([0, size * 0.5 if shape_name != 'plane' else 0, 0], dtype=np.float32)
            scene.add_node(node)
            nodes.append(node)

        return nodes

    def _generate_scene_type(self, scene_type, parsed, scene):
        nodes = []
        size = parsed['size']
        color = parsed['color']
        mat_type = parsed['material_type']

        generators = {
            'house': self.procedural_engine.generate_house,
            'tree': self.procedural_engine.generate_tree,
            'car': self.procedural_engine.generate_car,
            'robot': self.procedural_engine.generate_robot,
            'castle': self.procedural_engine.generate_castle,
            'spaceship': self.procedural_engine.generate_spaceship,
            'sword': self.procedural_engine.generate_sword,
            'chair': self.procedural_engine.generate_chair,
            'skull': lambda s: self.procedural_engine.generate_heart(s),
            'star': self.procedural_engine.generate_star,
            'heart': self.procedural_engine.generate_heart,
            'mountain': self.procedural_engine.generate_mountain,
            'snowman': self.procedural_engine.generate_snowman,
            'diamond': self.procedural_engine.generate_diamond,
            'spiral': self.procedural_engine.generate_spiral,
            'dna': self.procedural_engine.generate_dna,
        }

        if scene_type in generators:
            try:
                parts = generators[scene_type](size)
            except TypeError:
                parts = generators[scene_type]()

            for part_name, part_mesh, part_mat, position, rotation, part_scale in parts:
                node = SceneNode(part_name)
                node.mesh = part_mesh
                mat = Material(part_name + "_mat")
                mat.diffuse = np.asarray(part_mat['diffuse'], dtype=np.float32)
                mat.shininess = part_mat.get('shininess', 32.0)
                if 'metallic' in part_mat:
                    mat.metallic = part_mat['metallic']
                if 'emission' in part_mat:
                    mat.emission = np.asarray(part_mat['emission'], dtype=np.float32)
                node.material = mat
                node.position = np.asarray(position, dtype=np.float32)
                node.rotation = np.asarray(rotation, dtype=np.float32)
                node.scale = np.asarray(part_scale, dtype=np.float32)

                if color is not None:
                    mat.diffuse[:3] = color[:3]

                scene.add_node(node)
                nodes.append(node)

        return nodes

    def _create_material(self, color, mat_type):
        mat = Material("Generated_Material")

        if color is not None:
            mat.diffuse = color.copy()

        if mat_type == 'metallic':
            mat.metallic = 0.9
            mat.roughness = 0.15
            mat.shininess = 100.0
            if color is None:
                mat.diffuse = np.array([0.7, 0.7, 0.75, 1.0], dtype=np.float32)
        elif mat_type == 'glossy':
            mat.shininess = 120.0
            mat.roughness = 0.05
        elif mat_type == 'matte':
            mat.shininess = 2.0
            mat.roughness = 1.0
        elif mat_type == 'glass':
            mat.opacity = 0.3
            mat.diffuse[3] = 0.3
            mat.shininess = 150.0
            if color is None:
                mat.diffuse = np.array([0.8, 0.9, 1.0, 0.3], dtype=np.float32)
        elif mat_type == 'emissive':
            mat.emission = mat.diffuse.copy() * 0.5
            mat.shininess = 50.0
        elif mat_type == 'wireframe':
            mat.wireframe = True
        elif mat_type == 'default':
            if color is None:
                mat.diffuse = np.array([0.8, 0.3, 0.2, 1.0], dtype=np.float32)

        return mat

    def _apply_arrangement(self, nodes, parsed, scene):
        arrangement = parsed['arrangement']
        count = parsed['count']
        spacing = parsed['size'] * 2.5

        if arrangement == 'grid':
            side = int(math.ceil(math.sqrt(count)))
            idx = 0
            for z in range(side):
                for x in range(side):
                    if idx >= count:
                        break
                    node = nodes[min(idx, len(nodes) - 1)]
                    if idx < len(nodes):
                        node.position = np.array([x * spacing, 0, z * spacing], dtype=np.float32)
                    idx += 1

        elif arrangement == 'circle':
            radius = spacing * count / (2 * math.pi)
            for i, node in enumerate(nodes):
                angle = (2 * math.pi * i) / count
                node.position = np.array([
                    radius * math.cos(angle),
                    0,
                    radius * math.sin(angle)
                ], dtype=np.float32)

        elif arrangement == 'line':
            for i, node in enumerate(nodes):
                node.position = np.array([i * spacing, 0, 0], dtype=np.float32)

        elif arrangement == 'spiral':
            for i, node in enumerate(nodes):
                t = i / max(count - 1, 1)
                angle = t * 4 * math.pi
                r = t * spacing * 2
                node.position = np.array([
                    r * math.cos(angle),
                    t * spacing * 3,
                    r * math.sin(angle)
                ], dtype=np.float32)

    def _apply_modifier(self, modifier, nodes, scene):
        if modifier == 'subdivide':
            for node in nodes:
                if node.mesh:
                    node.mesh.subdivide()

        elif modifier == 'smooth':
            for node in nodes:
                if node.mesh:
                    for _ in range(2):
                        node.mesh.subdivide()

        elif modifier == 'mirror':
            for node in nodes[:]:
                clone = node.clone()
                clone.position[0] = -clone.position[0]
                clone.name = node.name + "_mirror"
                if clone.mesh:
                    clone.mesh.invert_faces()
                scene.add_node(clone)
                nodes.append(clone)

    def get_last_description(self):
        if self._creation_history:
            last = self._creation_history[-1]
            return self.prompt_engine.get_description(last['parsed'])
        return "Nothing generated yet"
