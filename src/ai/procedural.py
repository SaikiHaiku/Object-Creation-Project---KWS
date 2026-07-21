import numpy as np
import math
from ..core.mesh import Mesh


class ProceduralEngine:
    def __init__(self):
        pass

    def generate_tree(self, height=3.0, trunk_radius=0.15, branches=6, leaves_density=0.8):
        nodes_data = []
        trunk_mesh = self._create_tapered_cylinder(trunk_radius, trunk_radius * 0.5, height * 0.4, 12)
        trunk_mat = {'diffuse': np.array([0.4, 0.25, 0.1, 1.0]), 'shininess': 5.0}
        nodes_data.append(('trunk', trunk_mesh, trunk_mat, [0, height * 0.2, 0], [0, 0, 0], [1, 1, 1]))

        for i in range(branches):
            angle = (2 * math.pi * i) / branches + np.random.uniform(-0.3, 0.3)
            branch_len = height * np.random.uniform(0.2, 0.4)
            branch_r = trunk_radius * np.random.uniform(0.3, 0.6)
            y_pos = height * np.random.uniform(0.35, 0.65)
            branch_mesh = self._create_tapered_cylinder(branch_r, branch_r * 0.3, branch_len, 8)
            bx = math.cos(angle) * branch_len * 0.3
            bz = math.sin(angle) * branch_len * 0.3
            nodes_data.append(('branch', branch_mesh, trunk_mat,
                            [bx, y_pos, bz], [0, 0, math.degrees(angle)], [1, 1, 1]))

        leaf_radius = height * 0.35
        layers = 3
        for layer in range(layers):
            lr = leaf_radius * (1.0 - layer * 0.25)
            ly = height * (0.5 + layer * 0.15)
            leaf_mesh = self._create_deformed_sphere(lr, 12, 8, 0.3)
            green = np.random.uniform(0.15, 0.35)
            leaf_mat = {'diffuse': np.array([0.1, green, 0.05, 1.0]), 'shininess': 8.0}
            nodes_data.append(('leaves', leaf_mesh, leaf_mat, [0, ly, 0], [0, 0, 0], [1, 1, 1]))

        return nodes_data

    def generate_house(self, size=1.0):
        nodes_data = []
        wall_mat = {'diffuse': np.array([0.85, 0.82, 0.75, 1.0]), 'shininess': 10.0}
        roof_mat = {'diffuse': np.array([0.6, 0.15, 0.1, 1.0]), 'shininess': 15.0}
        floor_mat = {'diffuse': np.array([0.5, 0.35, 0.2, 1.0]), 'shininess': 5.0}
        window_mat = {'diffuse': np.array([0.4, 0.6, 0.9, 0.8]), 'shininess': 80.0}

        from ..core.primitives import create_cube
        body = create_cube(size)
        nodes_data.append(('body', body, wall_mat, [0, size * 0.5, 0], [0, 0, 0], [1.2, 1, 0.8]))

        roof_mesh = self._create_pyramid(size * 0.9, size * 0.5, 4)
        nodes_data.append(('roof', roof_mesh, roof_mat, [0, size * 1.25, 0], [0, 0, 0], [1, 1, 1]))

        door = create_cube(size * 0.2)
        nodes_data.append(('door', door, floor_mat, [0, size * 0.25, size * 0.41], [0, 0, 0], [0.6, 1, 0.05]))

        for side in [-1, 1]:
            win = create_cube(size * 0.12)
            nodes_data.append(('window', win, window_mat,
                            [side * size * 0.25, size * 0.65, size * 0.41], [0, 0, 0], [1, 0.8, 0.05]))

        chimney = self._create_tapered_cylinder(size * 0.08, size * 0.08, size * 0.4, 8)
        nodes_data.append(('chimney', chimney, roof_mat, [size * 0.3, size * 1.4, -size * 0.15], [0, 0, 0], [1, 1, 1]))

        return nodes_data

    def generate_car(self, size=1.0):
        nodes_data = []
        body_mat = {'diffuse': np.array([0.8, 0.1, 0.1, 1.0]), 'shininess': 60.0, 'metallic': 0.7}
        tire_mat = {'diffuse': np.array([0.15, 0.15, 0.15, 1.0]), 'shininess': 5.0}
        glass_mat = {'diffuse': np.array([0.4, 0.5, 0.8, 0.6]), 'shininess': 100.0}

        from ..core.primitives import create_cube, create_sphere
        body = create_cube(size * 0.4)
        nodes_data.append(('chassis', body, body_mat, [0, size * 0.15, 0], [0, 0, 0], [2.5, 0.5, 1.0]))

        cabin = create_cube(size * 0.25)
        nodes_data.append(('cabin', cabin, body_mat, [-size * 0.05, size * 0.35, 0], [0, 0, 0], [1.2, 0.7, 0.9]))

        windshield = create_cube(size * 0.01)
        nodes_data.append(('windshield', windshield, glass_mat, [size * 0.15, size * 0.38, 0], [0, 0, -15], [0.05, 0.55, 0.85]))

        for x in [-0.35, 0.35]:
            for z in [-0.45, 0.45]:
                wheel = create_sphere(size * 0.1, 16, 12)
                nodes_data.append(('wheel', wheel, tire_mat, [x * size, size * 0.05, z * size], [90, 0, 0], [1, 1, 0.8]))

        for x_side in [-1, 1]:
            headlight = create_sphere(size * 0.04, 12, 8)
            light_mat = {'diffuse': np.array([1.0, 1.0, 0.8, 1.0]), 'shininess': 100.0, 'emission': np.array([1, 1, 0.8, 1])}
            nodes_data.append(('headlight', headlight, light_mat, [x_side * size * 0.15, size * 0.18, size * 0.51], [0, 0, 0], [1, 1, 1]))

        return nodes_data

    def generate_robot(self, size=1.0):
        nodes_data = []
        metal_mat = {'diffuse': np.array([0.6, 0.65, 0.7, 1.0]), 'shininess': 80.0, 'metallic': 0.9}
        eye_mat = {'diffuse': np.array([0.0, 0.8, 1.0, 1.0]), 'shininess': 100.0, 'emission': np.array([0, 0.5, 1, 1])}
        dark_mat = {'diffuse': np.array([0.2, 0.2, 0.25, 1.0]), 'shininess': 20.0}

        from ..core.primitives import create_cube, create_sphere
        body = create_cube(size * 0.5)
        nodes_data.append(('body', body, metal_mat, [0, size * 0.4, 0], [0, 0, 0], [1, 1.2, 0.8]))

        head = create_cube(size * 0.35)
        nodes_data.append(('head', head, metal_mat, [0, size * 1.05, 0], [0, 0, 0], [1, 0.8, 0.9]))

        for side in [-1, 1]:
            eye = create_sphere(size * 0.06, 12, 8)
            nodes_data.append(('eye', eye, eye_mat, [side * size * 0.1, size * 1.1, size * 0.18], [0, 0, 0], [1, 1, 1]))

        for side in [-1, 1]:
            arm = create_cube(size * 0.12)
            nodes_data.append(('arm', arm, metal_mat, [side * size * 0.4, size * 0.4, 0], [0, 0, 0], [0.4, 1.5, 0.4]))

            hand = create_sphere(size * 0.08, 10, 8)
            nodes_data.append(('hand', hand, dark_mat, [side * size * 0.4, size * -0.15, 0], [0, 0, 0], [1, 1, 1]))

        for side in [-1, 1]:
            leg = create_cube(size * 0.14)
            nodes_data.append(('leg', leg, metal_mat, [side * size * 0.15, size * -0.15, 0], [0, 0, 0], [0.5, 1.3, 0.5]))

            foot = create_cube(size * 0.1)
            nodes_data.append(('foot', foot, dark_mat, [side * size * 0.15, size * -0.5, size * 0.05], [0, 0, 0], [0.6, 0.3, 0.8]))

        antenna = self._create_tapered_cylinder(size * 0.02, size * 0.01, size * 0.2, 8)
        nodes_data.append(('antenna', antenna, dark_mat, [0, size * 1.35, 0], [0, 0, 0], [1, 1, 1]))

        antenna_ball = create_sphere(size * 0.04, 10, 8)
        nodes_data.append(('antenna_ball', antenna_ball, eye_mat, [0, size * 1.47, 0], [0, 0, 0], [1, 1, 1]))

        return nodes_data

    def generate_castle(self, size=1.0):
        nodes_data = []
        stone_mat = {'diffuse': np.array([0.6, 0.55, 0.5, 1.0]), 'shininess': 5.0}
        dark_mat = {'diffuse': np.array([0.3, 0.28, 0.25, 1.0]), 'shininess': 3.0}

        from ..core.primitives import create_cube
        base = create_cube(size)
        nodes_data.append(('base', base, stone_mat, [0, size * 0.5, 0], [0, 0, 0], [2, 1, 1.5]))

        tower_positions = [
            (-size * 0.9, 0, -size * 0.6),
            (size * 0.9, 0, -size * 0.6),
            (-size * 0.9, 0, size * 0.6),
            (size * 0.9, 0, size * 0.6),
        ]
        for pos in tower_positions:
            tower = self._create_tapered_cylinder(size * 0.2, size * 0.2, size * 2.0, 8)
            nodes_data.append(('tower', tower, stone_mat, [pos[0], size * 1.0, pos[2]], [0, 0, 0], [1, 1, 1]))

            cap = create_cube(size * 0.25)
            nodes_data.append(('tower_cap', cap, dark_mat,
                            [pos[0], size * 2.1, pos[2]], [0, 45, 0], [1, 0.5, 1]))

        gate = create_cube(size * 0.3)
        nodes_data.append(('gate', gate, dark_mat, [0, size * 0.35, size * 0.76], [0, 0, 0], [0.8, 1.2, 0.05]))

        return nodes_data

    def generate_spaceship(self, size=1.0):
        nodes_data = []
        hull_mat = {'diffuse': np.array([0.6, 0.65, 0.7, 1.0]), 'shininess': 80.0, 'metallic': 0.8}
        engine_mat = {'diffuse': np.array([0.2, 0.5, 1.0, 1.0]), 'shininess': 100.0, 'emission': np.array([0, 0.3, 0.8, 1])}
        window_mat = {'diffuse': np.array([0.5, 0.7, 1.0, 0.7]), 'shininess': 120.0}

        from ..core.primitives import create_cylinder, create_cone, create_sphere
        body = create_cylinder(size * 0.3, size * 1.5, 24)
        nodes_data.append(('body', body, hull_mat, [0, 0, 0], [0, 0, 90], [1, 1, 1]))

        nose = create_cone(size * 0.3, size * 0.6, 24)
        nodes_data.append(('nose', nose, hull_mat, [size * 1.05, 0, 0], [0, 0, -90], [1, 1, 1]))

        for angle in [0, 120, 240]:
            wing = create_cube(size * 0.05)
            rad = math.radians(angle)
            wx = math.cos(rad) * size * 0.3
            wz = math.sin(rad) * size * 0.3
            nodes_data.append(('wing', wing, hull_mat, [0, 0, 0], [0, angle, 0], [1.5, 0.3, 1.0]))

        for i in range(3):
            angle = (i * 2 * math.pi) / 3
            ex = math.cos(angle) * size * 0.25
            ez = math.sin(angle) * size * 0.25
            engine = create_cylinder(size * 0.08, size * 0.3, 12)
            nodes_data.append(('engine', engine, engine_mat, [-size * 0.85, 0, ez], [90, 0, 0], [1, 1, 1]))

        cockpit = create_sphere(size * 0.18, 16, 12)
        nodes_data.append(('cockpit', cockpit, window_mat, [size * 0.5, size * 0.15, 0], [0, 0, 0], [1, 0.6, 0.8]))

        return nodes_data

    def generate_sword(self, size=1.0):
        nodes_data = []
        blade_mat = {'diffuse': np.array([0.8, 0.85, 0.9, 1.0]), 'shininess': 120.0, 'metallic': 0.95}
        handle_mat = {'diffuse': np.array([0.35, 0.2, 0.1, 1.0]), 'shininess': 10.0}
        guard_mat = {'diffuse': np.array([0.8, 0.7, 0.0, 1.0]), 'shininess': 60.0, 'metallic': 0.8}

        from ..core.primitives import create_cube
        blade = create_cube(size * 0.06)
        nodes_data.append(('blade', blade, blade_mat, [0, size * 0.8, 0], [0, 0, 0], [0.3, 3, 1]))

        tip = create_cube(size * 0.03)
        nodes_data.append(('tip', tip, blade_mat, [0, size * 1.55, 0], [0, 0, 45], [0.3, 1, 0.3]))

        guard = create_cube(size * 0.04)
        nodes_data.append(('guard', guard, guard_mat, [0, size * 0.28, 0], [0, 0, 0], [3, 0.4, 0.6]))

        handle = create_cube(size * 0.05)
        nodes_data.append(('handle', handle, handle_mat, [0, size * 0.05, 0], [0, 0, 0], [0.5, 1.5, 0.5]))

        pommel = create_sphere(size * 0.06, 12, 8)
        nodes_data.append(('pommel', pommel, guard_mat, [0, -size * 0.15, 0], [0, 0, 0], [1, 1, 1]))

        return nodes_data

    def generate_chair(self, size=1.0):
        nodes_data = []
        wood_mat = {'diffuse': np.array([0.55, 0.35, 0.15, 1.0]), 'shininess': 15.0}

        from ..core.primitives import create_cube
        seat = create_cube(size * 0.4)
        nodes_data.append(('seat', seat, wood_mat, [0, size * 0.4, 0], [0, 0, 0], [1, 0.15, 1]))

        back = create_cube(size * 0.4)
        nodes_data.append(('back', back, wood_mat, [0, size * 0.75, -size * 0.18], [0, 0, 0], [1, 0.7, 0.1]))

        for x in [-1, 1]:
            for z in [-1, 1]:
                leg = create_cube(size * 0.05)
                nodes_data.append(('leg', leg, wood_mat,
                                [x * size * 0.15, size * 0.17, z * size * 0.15],
                                [0, 0, 0], [0.3, 1, 0.3]))

        return nodes_data

    def generate_diamond(self, size=1.0):
        nodes_data = []
        gem_mat = {'diffuse': np.array([0.2, 0.6, 1.0, 0.9]), 'shininess': 150.0, 'metallic': 0.1}

        from ..core.primitives import create_ico_sphere
        gem = create_ico_sphere(size * 0.5, 2)
        nodes_data.append(('gem', gem, gem_mat, [0, size * 0.3, 0], [0, 0, 0], [1, 1.2, 1]))

        base = create_ico_sphere(size * 0.3, 1)
        nodes_data.append(('base', base, gem_mat, [0, -size * 0.1, 0], [0, 0, 0], [1, 0.5, 1]))

        return nodes_data

    def generate_snowman(self, size=1.0):
        nodes_data = []
        snow_mat = {'diffuse': np.array([0.95, 0.95, 1.0, 1.0]), 'shininess': 5.0}
        orange_mat = {'diffuse': np.array([1.0, 0.5, 0.0, 1.0]), 'shininess': 10.0}
        black_mat = {'diffuse': np.array([0.05, 0.05, 0.05, 1.0]), 'shininess': 50.0}

        from ..core.primitives import create_sphere, create_cone
        bottom = create_sphere(size * 0.45, 20, 14)
        nodes_data.append(('bottom', bottom, snow_mat, [0, size * 0.45, 0], [0, 0, 0], [1, 1, 1]))

        middle = create_sphere(size * 0.32, 18, 12)
        nodes_data.append(('middle', middle, snow_mat, [0, size * 1.1, 0], [0, 0, 0], [1, 1, 1]))

        head = create_sphere(size * 0.22, 16, 10)
        nodes_data.append(('head', head, snow_mat, [0, size * 1.6, 0], [0, 0, 0], [1, 1, 1]))

        nose = create_cone(size * 0.04, size * 0.2, 8)
        nodes_data.append(('nose', nose, orange_mat, [0, size * 1.55, size * 0.22], [90, 0, 0], [1, 1, 1]))

        for angle in [-20, 0, 20]:
            eye = create_sphere(size * 0.03, 10, 8)
            ex = math.sin(math.radians(angle)) * size * 0.12
            nodes_data.append(('eye', eye, black_mat,
                            [ex, size * 1.68, size * 0.18], [0, 0, 0], [1, 1, 1]))

        for angle in [-15, 0, 15]:
            btn = create_sphere(size * 0.03, 8, 6)
            by = size * (0.95 + angle * 0.01)
            nodes_data.append(('button', btn, black_mat, [0, by, size * 0.33], [0, 0, 0], [1, 1, 1]))

        return nodes_data

    def generate_mountain(self, size=1.0):
        nodes_data = []
        rock_mat = {'diffuse': np.array([0.4, 0.38, 0.35, 1.0]), 'shininess': 3.0}
        snow_mat = {'diffuse': np.array([0.95, 0.95, 1.0, 1.0]), 'shininess': 5.0}
        grass_mat = {'diffuse': np.array([0.2, 0.5, 0.15, 1.0]), 'shininess': 3.0}

        from ..core.primitives import create_cone
        mountain = create_cone(size * 1.2, size * 2.0, 24)
        nodes_data.append(('mountain', mountain, rock_mat, [0, size * 1.0, 0], [0, 0, 0], [1, 1, 1]))

        peak = create_cone(size * 0.4, size * 0.6, 16)
        nodes_data.append(('peak', peak, snow_mat, [0, size * 2.1, 0], [0, 0, 0], [1, 1, 1]))

        base = create_cone(size * 1.5, size * 0.3, 20)
        nodes_data.append(('base', base, grass_mat, [0, size * 0.15, 0], [0, 0, 0], [1, 1, 1]))

        return nodes_data

    def generate_star(self, size=1.0):
        nodes_data = []
        star_mat = {'diffuse': np.array([1.0, 0.9, 0.0, 1.0]), 'shininess': 100.0, 'emission': np.array([0.3, 0.3, 0, 1])}

        points = 5
        inner_r = size * 0.2
        outer_r = size * 0.5
        vertices = []
        faces = []
        center_top = 0
        center_bottom = 1
        vertices.append(([0, size * 0.05, 0], [0, 1, 0]))
        vertices.append(([0, -size * 0.05, 0], [0, -1, 0]))

        for i in range(points * 2):
            angle = (math.pi * i) / points - math.pi / 2
            r = outer_r if i % 2 == 0 else inner_r
            x = r * math.cos(angle)
            z = r * math.sin(angle)
            vertices.append(([x, 0, z], [0, 1, 0]))

        for i in range(points * 2):
            next_i = (i + 1) % (points * 2) + 2
            faces.append([center_top, i + 2, next_i])
            faces.append([center_bottom, next_i, i + 2])

        star_mesh = Mesh("Star")
        for pos, norm in vertices:
            star_mesh.add_vertex(pos, norm)
        for face in faces:
            star_mesh.add_face(face)
        star_mesh.compute_normals()

        nodes_data.append(('star', star_mesh, star_mat, [0, size * 0.5, 0], [0, 0, 0], [1, 1, 1]))
        return nodes_data

    def generate_heart(self, size=1.0):
        nodes_data = []
        heart_mat = {'diffuse': np.array([0.9, 0.1, 0.2, 1.0]), 'shininess': 30.0}

        vertices = []
        faces = []
        segments = 24
        rings = 16

        for ring in range(rings + 1):
            v = ring / rings
            phi = math.pi * v
            for seg in range(segments + 1):
                u = seg / segments
                theta = 2 * math.pi * u
                x = 16 * math.pow(math.sin(phi), 3) if False else 0
                sin_phi = math.sin(phi)
                cos_phi = math.cos(phi)
                x = sin_phi * math.sin(phi) * math.sin(phi) * 0.03
                y = cos_phi
                z = sin_phi * math.cos(theta)
                x_val = 16 * sin_phi * sin_phi * sin_phi * math.sin(theta) / 100
                y_val = (13 * cos_phi - 5 * math.cos(2 * phi) - 2 * math.cos(3 * phi) - math.cos(4 * phi)) / 100
                z_val = 16 * sin_phi * sin_phi * sin_phi * math.cos(theta) / 100
                vertices.append(([x_val * size, y_val * size + size * 0.3, z_val * size], [0, 0, 1]))

        for ring in range(rings):
            for seg in range(segments):
                i0 = ring * (segments + 1) + seg
                i1 = i0 + 1
                i2 = i0 + (segments + 1)
                i3 = i2 + 1
                faces.append([i0, i2, i3, i1])

        heart_mesh = Mesh("Heart")
        for pos, norm in vertices:
            heart_mesh.add_vertex(pos, norm)
        for face in faces:
            heart_mesh.add_face(face)
        heart_mesh.compute_normals()

        nodes_data.append(('heart', heart_mesh, heart_mat, [0, 0, 0], [0, 0, 0], [1, 1, 1]))
        return nodes_data

    def generate_spiral(self, size=1.0):
        nodes_data = []
        spiral_mat = {'diffuse': np.array([0.5, 0.2, 0.8, 1.0]), 'shininess': 40.0, 'metallic': 0.3}

        from ..core.primitives import create_sphere
        turns = 3
        points_per_turn = 20
        total = turns * points_per_turn
        for i in range(total):
            t = i / total
            angle = t * turns * 2 * math.pi
            r = t * size * 0.5
            x = r * math.cos(angle)
            y = t * size * 2
            z = r * math.sin(angle)
            s = 0.05 + t * 0.15
            ball = create_sphere(s, 10, 8)
            hue = t
            r_c = abs(math.sin(hue * math.pi * 2))
            g_c = abs(math.sin(hue * math.pi * 2 + 2))
            b_c = abs(math.sin(hue * math.pi * 2 + 4))
            mat = {'diffuse': np.array([r_c, g_c, b_c, 1.0]), 'shininess': 40.0}
            nodes_data.append(('spiral_ball', ball, mat, [x, y, z], [0, 0, 0], [1, 1, 1]))

        return nodes_data

    def generate_dna(self, size=1.0):
        nodes_data = []
        strand_mat = {'diffuse': np.array([0.2, 0.4, 0.9, 1.0]), 'shininess': 60.0}
        link_mat = {'diffuse': np.array([0.9, 0.3, 0.3, 1.0]), 'shininess': 30.0}

        from ..core.primitives import create_sphere, create_cylinder
        turns = 2
        pts = 60
        radius = size * 0.3

        for strand in range(2):
            for i in range(pts):
                t = i / pts
                angle = t * turns * 2 * math.pi + strand * math.pi
                x = radius * math.cos(angle)
                y = t * size * 3
                z = radius * math.sin(angle)
                ball = create_sphere(size * 0.05, 8, 6)
                nodes_data.append(('strand_ball', ball, strand_mat, [x, y, z], [0, 0, 0], [1, 1, 1]))

        for i in range(0, pts, 5):
            t = i / pts
            angle1 = t * turns * 2 * math.pi
            angle2 = angle1 + math.pi
            x1 = radius * math.cos(angle1)
            x2 = radius * math.cos(angle2)
            y = t * size * 3
            z1 = radius * math.sin(angle1)
            z2 = radius * math.sin(angle2)
            mid_x = (x1 + x2) / 2
            mid_z = (z1 + z2) / 2
            dx = x2 - x1
            dz = z2 - z1
            length = math.sqrt(dx * dx + dz * dz)
            link = create_cylinder(size * 0.02, length, 6)
            angle = math.degrees(math.atan2(dz, dx))
            nodes_data.append(('link', link, link_mat, [mid_x, y, mid_z], [0, 0, 0], [1, 1, 1]))

        return nodes_data

    def _create_tapered_cylinder(self, radius_top, radius_bottom, height, segments):
        from ..core.mesh import Mesh
        m = Mesh("TaperedCylinder")
        half_h = height / 2.0
        for i in range(segments + 1):
            theta = 2 * math.pi * i / segments
            cos_t = math.cos(theta)
            sin_t = math.sin(theta)
            m.add_vertex([radius_top * cos_t, half_h, radius_top * sin_t], [cos_t, 0, sin_t])
            m.add_vertex([radius_bottom * cos_t, -half_h, radius_bottom * sin_t], [cos_t, 0, sin_t])
        for i in range(segments):
            i0 = i * 2
            i1 = i0 + 1
            i2 = i0 + 2
            i3 = i0 + 3
            m.add_face([i0, i2, i3, i1])
        m.compute_normals()
        return m

    def _create_pyramid(self, base_size, height, sides=4):
        from ..core.mesh import Mesh
        m = Mesh("Pyramid")
        half = base_size / 2.0
        tip = m.add_vertex([0, height, 0], [0, 1, 0], [0.5, 1.0])
        center_bottom = m.add_vertex([0, 0, 0], [0, -1, 0], [0.5, 0.5])

        for i in range(sides):
            angle = (2 * math.pi * i) / sides
            next_angle = (2 * math.pi * (i + 1)) / sides
            x1 = half * math.cos(angle)
            z1 = half * math.sin(angle)
            x2 = half * math.cos(next_angle)
            z2 = half * math.sin(next_angle)
            v1 = m.add_vertex([x1, 0, z1], [0, 0, 0])
            v2 = m.add_vertex([x2, 0, z2], [0, 0, 0])
            m.add_face([tip, v2, v1])
            m.add_face([center_bottom, v1, v2])
        m.compute_normals()
        return m

    def _create_deformed_sphere(self, radius, segments, rings, deform=0.3):
        from ..core.mesh import Mesh
        m = Mesh("DeformedSphere")
        for ring in range(rings + 1):
            phi = math.pi * ring / rings
            for seg in range(segments + 1):
                theta = 2.0 * math.pi * seg / segments
                r = radius * (1.0 + deform * math.sin(3 * theta) * math.sin(2 * phi))
                x = r * math.sin(phi) * math.cos(theta)
                y = r * math.cos(phi)
                z = r * math.sin(phi) * math.sin(theta)
                n = np.array([x, y, z], dtype=np.float32)
                norm = np.linalg.norm(n)
                if norm > 1e-8:
                    n /= norm
                m.add_vertex([x, y, z], n, [seg / segments, ring / rings])
        for ring in range(rings):
            for seg in range(segments):
                i0 = ring * (segments + 1) + seg
                i1 = i0 + 1
                i2 = i0 + (segments + 1)
                i3 = i2 + 1
                m.add_face([i0, i2, i3, i1])
        return m
