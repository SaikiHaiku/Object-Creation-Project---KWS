import numpy as np
from dataclasses import dataclass, field
from typing import List, Optional
import struct


@dataclass
class Vertex:
    position: np.ndarray = field(default_factory=lambda: np.zeros(3, dtype=np.float32))
    normal: np.ndarray = field(default_factory=lambda: np.array([0, 1, 0], dtype=np.float32))
    uv: np.ndarray = field(default_factory=lambda: np.zeros(2, dtype=np.float32))
    color: np.ndarray = field(default_factory=lambda: np.array([1, 1, 1, 1], dtype=np.float32))

    def to_array(self):
        return np.concatenate([self.position, self.normal, self.uv, self.color]).astype(np.float32)


@dataclass
class Face:
    vertices: List[int] = field(default_factory=list)
    normal: Optional[np.ndarray] = None


class Mesh:
    def __init__(self, name="Mesh"):
        self.name = name
        self.vertices: List[Vertex] = []
        self.faces: List[Face] = []
        self._vao = None
        self._vbo = None
        self._ebo = None
        self._dirty = True
        self._vertex_data = None
        self._index_data = None

    def add_vertex(self, position, normal=None, uv=None, color=None):
        v = Vertex(
            position=np.asarray(position, dtype=np.float32),
            normal=np.asarray(normal if normal is not None else [0, 1, 0], dtype=np.float32),
            uv=np.asarray(uv if uv is not None else [0, 0], dtype=np.float32),
            color=np.asarray(color if color is not None else [1, 1, 1, 1], dtype=np.float32),
        )
        self.vertices.append(v)
        self._dirty = True
        return len(self.vertices) - 1

    def add_face(self, vertex_indices, normal=None):
        f = Face(vertices=list(vertex_indices), normal=normal)
        self.faces.append(f)
        self._dirty = True
        return len(self.faces) - 1

    def get_vertex_data(self):
        if self._dirty or self._vertex_data is None:
            data = np.array([v.to_array() for v in self.vertices], dtype=np.float32)
            self._vertex_data = data
        return self._vertex_data

    def get_index_data(self):
        if self._dirty or self._index_data is None:
            indices = []
            for face in self.faces:
                indices.extend(face.vertices)
            self._index_data = np.array(indices, dtype=np.uint32)
            self._dirty = False
        return self._index_data

    def compute_normals(self):
        for v in self.vertices:
            v.normal = np.zeros(3, dtype=np.float32)

        for face in self.faces:
            if len(face.vertices) < 3:
                continue
            v0 = self.vertices[face.vertices[0]].position
            v1 = self.vertices[face.vertices[1]].position
            v2 = self.vertices[face.vertices[2]].position
            normal = np.cross(v1 - v0, v2 - v0)
            norm = np.linalg.norm(normal)
            if norm > 1e-8:
                normal /= norm
            for vi in face.vertices:
                self.vertices[vi].normal += normal

        for v in self.vertices:
            norm = np.linalg.norm(v.normal)
            if norm > 1e-8:
                v.normal /= norm
        self._dirty = True

    def get_triangle_count(self):
        count = 0
        for face in self.faces:
            if len(face.vertices) >= 3:
                count += len(face.vertices) - 2
        return count

    def get_vertex_count(self):
        return len(self.vertices)

    def apply_transform(self, matrix):
        for v in self.vertices:
            p = np.append(v.position, 1.0)
            v.position = (matrix @ p)[:3]
            n = np.append(v.normal, 0.0)
            v.normal = (matrix @ n)[:3]
            norm = np.linalg.norm(v.normal)
            if norm > 1e-8:
                v.normal /= norm
        self._dirty = True

    def weld_vertices(self, threshold=1e-4):
        new_verts = []
        remap = {}
        for i, v in enumerate(self.vertices):
            found = False
            for j, nv in enumerate(new_verts):
                if np.linalg.norm(v.position - nv.position) < threshold:
                    remap[i] = j
                    found = True
                    break
            if not found:
                remap[i] = len(new_verts)
                new_verts.append(v)
        self.vertices = new_verts
        for face in self.faces:
            face.vertices = [remap.get(v, v) for v in face.vertices]
        self._dirty = True

    def subdivide(self):
        new_faces = []
        for face in self.faces:
            verts = face.vertices
            if len(verts) == 3:
                mid01 = self._midpoint(verts[0], verts[1])
                mid12 = self._midpoint(verts[1], verts[2])
                mid02 = self._midpoint(verts[0], verts[2])
                new_faces.append(Face([verts[0], mid01, mid02]))
                new_faces.append(Face([mid01, verts[1], mid12]))
                new_faces.append(Face([mid02, mid12, verts[2]]))
                new_faces.append(Face([mid01, mid12, mid02]))
            elif len(verts) == 4:
                mid01 = self._midpoint(verts[0], verts[1])
                mid12 = self._midpoint(verts[1], verts[2])
                mid23 = self._midpoint(verts[2], verts[3])
                mid03 = self._midpoint(verts[0], verts[3])
                center = self._midpoint(mid01, mid23)
                new_faces.append(Face([verts[0], mid01, center, mid03]))
                new_faces.append(Face([mid01, verts[1], mid12, center]))
                new_faces.append(Face([center, mid12, verts[2], mid23]))
                new_faces.append(Face([mid03, center, mid23, verts[3]]))
        self.faces = new_faces
        self.compute_normals()
        self._dirty = True

    def _midpoint(self, i0, i1):
        p = (self.vertices[i0].position + self.vertices[i1].position) / 2.0
        n = (self.vertices[i0].normal + self.vertices[i1].normal) / 2.0
        norm = np.linalg.norm(n)
        if norm > 1e-8:
            n /= norm
        uv = (self.vertices[i0].uv + self.vertices[i1].uv) / 2.0
        c = (self.vertices[i0].color + self.vertices[i1].color) / 2.0
        return self.add_vertex(p, n, uv, c)

    def clone(self):
        m = Mesh(self.name + "_copy")
        m.vertices = [Vertex(v.position.copy(), v.normal.copy(), v.uv.copy(), v.color.copy()) for v in self.vertices]
        m.faces = [Face(list(f.vertices), f.normal.copy() if f.normal is not None else None) for f in self.faces]
        m._dirty = True
        return m

    def invert_faces(self):
        for face in self.faces:
            face.vertices.reverse()
            if face.normal is not None:
                face.normal = -face.normal
        self._dirty = True

    def get_bounding_box(self):
        if not self.vertices:
            return np.zeros(3), np.zeros(3)
        positions = np.array([v.position for v in self.vertices])
        return np.min(positions, axis=0), np.max(positions, axis=0)

    def get_center(self):
        bb_min, bb_max = self.get_bounding_box()
        return (bb_min + bb_max) / 2.0
