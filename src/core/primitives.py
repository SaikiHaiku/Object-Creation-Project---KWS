import numpy as np
import math
from .mesh import Mesh


def create_cube(size=1.0, segments=1):
    m = Mesh("Cube")
    s = size / 2.0
    faces = [
        ((-s, -s, s), (s, -s, s), (s, s, s), (-s, s, s), (0, 0, 1)),
        ((s, -s, -s), (-s, -s, -s), (-s, s, -s), (s, s, -s), (0, 0, -1)),
        ((-s, -s, -s), (-s, -s, s), (-s, s, s), (-s, s, -s), (-1, 0, 0)),
        ((s, -s, s), (s, -s, -s), (s, s, -s), (s, s, s), (1, 0, 0)),
        ((-s, s, s), (s, s, s), (s, s, -s), (-s, s, -s), (0, 1, 0)),
        ((-s, -s, -s), (s, -s, -s), (s, -s, s), (-s, -s, s), (0, -1, 0)),
    ]
    uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
    for p0, p1, p2, p3, n in faces:
        base = len(m.vertices)
        for i, (p, uv) in enumerate(zip([p0, p1, p2, p3], uvs)):
            m.add_vertex(p, n, uv)
        m.add_face([base, base + 1, base + 2, base + 3])
    m.compute_normals()
    return m


def create_sphere(radius=0.5, segments=32, rings=16):
    m = Mesh("Sphere")
    for ring in range(rings + 1):
        phi = math.pi * ring / rings
        for seg in range(segments + 1):
            theta = 2.0 * math.pi * seg / segments
            x = radius * math.sin(phi) * math.cos(theta)
            y = radius * math.cos(phi)
            z = radius * math.sin(phi) * math.sin(theta)
            n = np.array([x, y, z], dtype=np.float32)
            norm = np.linalg.norm(n)
            if norm > 1e-8:
                n /= norm
            u = seg / segments
            v = ring / rings
            m.add_vertex([x, y, z], n, [u, v])
    for ring in range(rings):
        for seg in range(segments):
            i0 = ring * (segments + 1) + seg
            i1 = i0 + 1
            i2 = i0 + (segments + 1)
            i3 = i2 + 1
            m.add_face([i0, i2, i3, i1])
    return m


def create_cylinder(radius=0.5, height=1.0, segments=32):
    m = Mesh("Cylinder")
    half_h = height / 2.0
    for i in range(segments + 1):
        theta = 2.0 * math.pi * i / segments
        x = radius * math.cos(theta)
        z = radius * math.sin(theta)
        u = i / segments
        nx = math.cos(theta)
        nz = math.sin(theta)
        m.add_vertex([x, half_h, z], [0, 1, 0], [u, 1])
        m.add_vertex([x, -half_h, z], [0, -1, 0], [u, 0])
        side_n = np.array([nx, 0, nz], dtype=np.float32)
        m.add_vertex([x, half_h, z], side_n, [u, 1])
        m.add_vertex([x, -half_h, z], side_n, [u, 0])
    base_center_top = len(m.vertices)
    m.add_vertex([0, half_h, 0], [0, 1, 0], [0.5, 0.5])
    base_center_bottom = len(m.vertices)
    m.add_vertex([0, -half_h, 0], [0, -1, 0], [0.5, 0.5])
    for i in range(segments):
        s = i * 4
        m.add_face([s, s + 4, s + 6, s + 2])
        m.add_face([s + 1, s + 3, s + 5, s + 7])
        m.add_face([base_center_top, s + 2, s])
        m.add_face([base_center_bottom, s + 1, s + 3])
    return m


def create_cone(radius=0.5, height=1.0, segments=32):
    m = Mesh("Cone")
    half_h = height / 2.0
    tip_idx = m.add_vertex([0, half_h, 0], [0, 1, 0], [0.5, 1.0])
    base_center = m.add_vertex([0, -half_h, 0], [0, -1, 0], [0.5, 0.5])
    for i in range(segments + 1):
        theta = 2.0 * math.pi * i / segments
        x = radius * math.cos(theta)
        z = radius * math.sin(theta)
        nx = math.cos(theta)
        nz = math.sin(theta)
        slope = np.array([nx * height, radius, nz * height], dtype=np.float32)
        norm = np.linalg.norm(slope)
        if norm > 1e-8:
            slope /= norm
        u = i / segments
        m.add_vertex([x, -half_h, z], slope, [u, 0])
    for i in range(segments):
        base = i + 2
        next_base = base + 1
        m.add_face([tip_idx, next_base, base])
        m.add_face([base_center, base, next_base])
    return m


def create_torus(major_radius=0.5, minor_radius=0.2, major_segments=32, minor_segments=16):
    m = Mesh("Torus")
    for i in range(major_segments + 1):
        theta = 2.0 * math.pi * i / major_segments
        cos_theta = math.cos(theta)
        sin_theta = math.sin(theta)
        for j in range(minor_segments + 1):
            phi = 2.0 * math.pi * j / minor_segments
            cos_phi = math.cos(phi)
            sin_phi = math.sin(phi)
            x = (major_radius + minor_radius * cos_phi) * cos_theta
            y = minor_radius * sin_phi
            z = (major_radius + minor_radius * cos_phi) * sin_theta
            nx = cos_phi * cos_theta
            ny = sin_phi
            nz = cos_phi * sin_theta
            u = i / major_segments
            v = j / minor_segments
            m.add_vertex([x, y, z], [nx, ny, nz], [u, v])
    for i in range(major_segments):
        for j in range(minor_segments):
            i0 = i * (minor_segments + 1) + j
            i1 = i0 + 1
            i2 = i0 + (minor_segments + 1)
            i3 = i2 + 1
            m.add_face([i0, i2, i3, i1])
    return m


def create_plane(size=1.0, subdivisions=1):
    m = Mesh("Plane")
    half = size / 2.0
    step = size / subdivisions
    for iz in range(subdivisions + 1):
        for ix in range(subdivisions + 1):
            x = -half + ix * step
            z = -half + iz * step
            u = ix / subdivisions
            v = iz / subdivisions
            m.add_vertex([x, 0, z], [0, 1, 0], [u, v])
    for iz in range(subdivisions):
        for ix in range(subdivisions):
            i0 = iz * (subdivisions + 1) + ix
            i1 = i0 + 1
            i2 = i0 + (subdivisions + 1)
            i3 = i2 + 1
            m.add_face([i0, i2, i3, i1])
    return m


def create_grid(size=20.0, subdivisions=20):
    m = Mesh("Grid")
    half = size / 2.0
    step = size / subdivisions
    for i in range(subdivisions + 1):
        pos = -half + i * step
        idx0 = m.add_vertex([pos, 0, -half], [0, 1, 0], [0, 0], [0.4, 0.4, 0.4, 1.0])
        idx1 = m.add_vertex([pos, 0, half], [0, 1, 0], [0, 1], [0.4, 0.4, 0.4, 1.0])
        m.add_face([idx0, idx1])
        idx0 = m.add_vertex([-half, 0, pos], [0, 1, 0], [0, 0], [0.4, 0.4, 0.4, 1.0])
        idx1 = m.add_vertex([half, 0, pos], [0, 1, 0], [1, 0], [0.4, 0.4, 0.4, 1.0])
        m.add_face([idx0, idx1])
    return m


def create_arrow(length=2.0, head_length=0.6, head_radius=0.2, shaft_radius=0.05, segments=16):
    m = Mesh("Arrow")
    half_shaft = (length - head_length) / 2.0
    shaft = create_cylinder(shaft_radius, length - head_length, segments)
    for v in shaft.vertices:
        v.position[1] += half_shaft
    for f in shaft.faces:
        m.add_face([f_v + len(m.vertices) for f_v in f.vertices])
    m.vertices.extend(shaft.vertices)
    head = create_cone(head_radius, head_length, segments)
    for v in head.vertices:
        v.position[1] += half_shaft + (length - head_length) / 2.0 + head_length / 2.0
    offset = len(m.vertices)
    for f in head.faces:
        m.add_face([f_v + offset for f_v in f.vertices])
    m.vertices.extend(head.vertices)
    m.compute_normals()
    return m


def create_torus_knot(p=2, q=3, major_radius=1.0, tube_radius=0.15, segments=128, tube_segments=16):
    m = Mesh("TorusKnot")
    points = []
    for i in range(segments + 1):
        t = 2.0 * math.pi * i / segments
        r = major_radius * (2.0 + math.cos(q * t)) / 3.0
        x = r * math.cos(p * t)
        y = r * math.sin(p * t)
        z = major_radius * math.sin(q * t) / 3.0
        points.append(np.array([x, y, z], dtype=np.float32))
    for i in range(segments):
        t0 = 2.0 * math.pi * i / segments
        t1 = 2.0 * math.pi * (i + 1) / segments
        p0 = points[i]
        p1 = points[i + 1]
        tangent = p1 - p0
        norm = np.linalg.norm(tangent)
        if norm > 1e-8:
            tangent /= norm
        up = np.array([0, 1, 0], dtype=np.float32)
        if abs(np.dot(tangent, up)) > 0.99:
            up = np.array([1, 0, 0], dtype=np.float32)
        side = np.cross(tangent, up)
        side = side / (np.linalg.norm(side) + 1e-8)
        bi = np.cross(tangent, side)
        bi = bi / (np.linalg.norm(bi) + 1e-8)
        for j in range(tube_segments + 1):
            angle = 2.0 * math.pi * j / tube_segments
            cx = tube_radius * math.cos(angle)
            cy = tube_radius * math.sin(angle)
            point = p0 + cx * side + cy * bi
            n = (point - p0)
            n = n / (np.linalg.norm(n) + 1e-8)
            u = i / segments
            v = j / tube_segments
            m.add_vertex(point, n, [u, v])
    for i in range(segments):
        for j in range(tube_segments):
            i0 = i * (tube_segments + 1) + j
            i1 = i0 + 1
            i2 = i0 + (tube_segments + 1)
            i3 = i2 + 1
            m.add_face([i0, i2, i3, i1])
    m.compute_normals()
    return m


def create_ico_sphere(radius=0.5, subdivisions=2):
    m = Mesh("IcoSphere")
    t = (1.0 + math.sqrt(5.0)) / 2.0
    verts = [
        [-1, t, 0], [1, t, 0], [-1, -t, 0], [1, -t, 0],
        [0, -1, t], [0, 1, t], [0, -1, -t], [0, 1, -t],
        [t, 0, -1], [t, 0, 1], [-t, 0, -1], [-t, 0, 1],
    ]
    for v in verts:
        n = np.array(v, dtype=np.float32)
        n = n / (np.linalg.norm(n) + 1e-8)
        p = n * radius
        m.add_vertex(p, n, [0, 0])
    tris = [
        (0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11),
        (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
        (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9),
        (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1),
    ]
    for tri in tris:
        m.add_face(list(tri))
    for _ in range(subdivisions):
        m.subdivide()
        for v in m.vertices:
            n = v.position / (np.linalg.norm(v.position) + 1e-8)
            v.position = n * radius
    m.compute_normals()
    return m


PRIMITIVE_CREATORS = {
    "cube": create_cube,
    "sphere": create_sphere,
    "cylinder": create_cylinder,
    "cone": create_cone,
    "torus": create_torus,
    "plane": create_plane,
    "torus_knot": create_torus_knot,
    "ico_sphere": create_ico_sphere,
}
