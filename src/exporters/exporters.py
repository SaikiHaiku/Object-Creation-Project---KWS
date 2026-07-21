import numpy as np


class OBJExporter:
    @staticmethod
    def export(scene, filepath):
        lines = ["# BloxBot OBJ Export", f"# Scene: {scene.name}", ""]
        vertex_offset = 0
        nodes = [n for n in scene.get_all_nodes() if n.mesh is not None]
        for node in nodes:
            mesh = node.mesh
            world = node.get_world_matrix()
            lines.append(f"o {node.name}")
            for v in mesh.vertices:
                p = np.append(v.position, 1.0)
                wp = world @ p
                lines.append(f"v {wp[0]:.6f} {wp[1]:.6f} {wp[2]:.6f}")
            for v in mesh.vertices:
                lines.append(f"vn {v.normal[0]:.6f} {v.normal[1]:.6f} {v.normal[2]:.6f}")
            for v in mesh.vertices:
                lines.append(f"vt {v.uv[0]:.6f} {v.uv[1]:.6f}")
            for face in mesh.faces:
                indices = [f"{i + vertex_offset + 1}/{i + vertex_offset + 1}/{i + vertex_offset + 1}" for i in face.vertices]
                lines.append(f"f {' '.join(indices)}")
            vertex_offset += len(mesh.vertices)
            lines.append("")
        with open(filepath, 'w') as f:
            f.write('\n'.join(lines))
        return len(nodes)


class STLExporter:
    @staticmethod
    def export(scene, filepath):
        triangles = []
        nodes = [n for n in scene.get_all_nodes() if n.mesh is not None]
        for node in nodes:
            mesh = node.mesh
            world = node.get_world_matrix()
            for face in mesh.faces:
                verts = face.vertices
                for i in range(len(verts) - 2):
                    p0 = mesh.vertices[verts[0]].position
                    p1 = mesh.vertices[verts[i + 1]].position
                    p2 = mesh.vertices[verts[i + 2]].position
                    wp0 = (world @ np.append(p0, 1.0))[:3]
                    wp1 = (world @ np.append(p1, 1.0))[:3]
                    wp2 = (world @ np.append(p2, 1.0))[:3]
                    n = np.cross(wp1 - wp0, wp2 - wp0)
                    norm = np.linalg.norm(n)
                    if norm > 1e-8:
                        n /= norm
                    triangles.append((n, wp0, wp1, wp2))

        with open(filepath, 'w') as f:
            f.write("solid BloxBot\n")
            for n, p0, p1, p2 in triangles:
                f.write(f"  facet normal {n[0]:.6f} {n[1]:.6f} {n[2]:.6f}\n")
                f.write("    outer loop\n")
                f.write(f"      vertex {p0[0]:.6f} {p0[1]:.6f} {p0[2]:.6f}\n")
                f.write(f"      vertex {p1[0]:.6f} {p1[1]:.6f} {p1[2]:.6f}\n")
                f.write(f"      vertex {p2[0]:.6f} {p2[1]:.6f} {p2[2]:.6f}\n")
                f.write("    endloop\n")
                f.write("  endfacet\n")
            f.write("endsolid BloxBot\n")
        return len(triangles)


class ImageExporter:
    @staticmethod
    def export(renderer, filepath, width=1920, height=1080):
        from PIL import Image
        from OpenGL.GL import glReadPixels, GL_RGB, GL_UNSIGNED_BYTE
        pixels = glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE)
        img = Image.frombytes('RGB', (width, height), pixels)
        img = img.transpose(Image.FLIP_TOP_BOTTOM)
        img.save(filepath)
        return True


class SVGExporter:
    @staticmethod
    def export_2d(scene, filepath, width=800, height=600):
        lines = [
            '<?xml version="1.0" encoding="UTF-8"?>',
            f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">',
            f'  <title>BloxBot 2D Export</title>',
        ]

        nodes = [n for n in scene.get_all_nodes() if n.mesh is not None]
        for node in nodes:
            mesh = node.mesh
            world = node.get_world_matrix()
            color = node.material.diffuse if node.material else np.array([0.5, 0.5, 0.5, 1.0])
            hex_color = f'#{int(color[0]*255):02x}{int(color[1]*255):02x}{int(color[2]*255):02x}'

            projected_points = []
            for v in mesh.vertices:
                wp = world @ np.append(v.position, 1.0)
                sx = (wp[0] + 1) * 0.5 * width
                sy = (1 - (wp[1] + 1) * 0.5) * height
                projected_points.append((sx, sy))

            for face in mesh.faces:
                if len(face.vertices) >= 3:
                    points = [projected_points[i] for i in face.vertices]
                    point_str = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
                    lines.append(f'  <polygon points="{point_str}" fill="{hex_color}" stroke="#333" stroke-width="0.5"/>')

        lines.append('</svg>')
        with open(filepath, 'w') as f:
            f.write('\n'.join(lines))
        return len(nodes)
