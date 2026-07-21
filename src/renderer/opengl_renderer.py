from OpenGL.GL import *
from OpenGL.GL import shaders
import numpy as np
from ..utils.math_utils import mat4_identity, mat3_normal_from_mat4
from .shaders import ShaderManager


class OpenGLRenderer:
    def __init__(self):
        self.shader_manager = None
        self._mesh_cache = {}
        self._gizmo_meshes = {}
        self._initialized = False

    def initialize(self):
        if self._initialized:
            return
        glEnable(GL_DEPTH_TEST)
        glEnable(GL_MULTISAMPLE)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
        glClearColor(0.15, 0.15, 0.2, 1.0)

        self.shader_manager = ShaderManager()
        self.shader_manager.initialize()
        self._create_gizmo_meshes()
        self._initialized = True

    def _create_gizmo_meshes(self):
        self._gizmo_meshes['grid'] = self._create_grid_mesh(20.0, 20)

    def _create_grid_mesh(self, size, subdivs):
        vao = glGenVertexArrays(1)
        glBindVertexArray(vao)
        vertices = []
        half = size / 2.0
        step = size / subdivs
        for i in range(subdivs + 1):
            pos = -half + i * step
            vertices.extend([pos, 0, -half, 0.5, 0.5, 0.5, 1.0])
            vertices.extend([pos, 0, half, 0.5, 0.5, 0.5, 1.0])
            vertices.extend([-half, 0, pos, 0.5, 0.5, 0.5, 1.0])
            vertices.extend([half, 0, pos, 0.5, 0.5, 0.5, 1.0])
        data = np.array(vertices, dtype=np.float32)
        vbo = glGenBuffers(1)
        glBindBuffer(GL_ARRAY_BUFFER, vbo)
        glBufferData(GL_ARRAY_BUFFER, data.nbytes, data, GL_STATIC_DRAW)
        stride = 7 * 4
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(0))
        glEnableVertexAttribArray(0)
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(12))
        glEnableVertexAttribArray(3)
        glBindVertexArray(0)
        return {'vao': vao, 'count': len(vertices) // 7}

    def _get_or_create_mesh_buffers(self, mesh):
        mesh_id = id(mesh)
        if mesh_id in self._mesh_cache and not mesh._dirty:
            return self._mesh_cache[mesh_id]

        vao = glGenVertexArrays(1)
        glBindVertexArray(vao)

        vertex_data = mesh.get_vertex_data()
        vbo = glGenBuffers(1)
        glBindBuffer(GL_ARRAY_BUFFER, vbo)
        glBufferData(GL_ARRAY_BUFFER, vertex_data.nbytes, vertex_data, GL_DYNAMIC_DRAW)

        stride = 12 * 4
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(0))
        glEnableVertexAttribArray(0)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(12))
        glEnableVertexAttribArray(1)
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(24))
        glEnableVertexAttribArray(2)
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(32))
        glEnableVertexAttribArray(3)

        index_data = mesh.get_index_data()
        ebo = glGenBuffers(1)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_data.nbytes, index_data, GL_DYNAMIC_DRAW)

        glBindVertexArray(0)
        count = len(index_data)

        result = {'vao': vao, 'vbo': vbo, 'ebo': ebo, 'count': count}
        self._mesh_cache[mesh_id] = result
        mesh._dirty = False
        return result

    def clear(self, color=None):
        if color is not None:
            glClearColor(color[0], color[1], color[2], color[3])
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

    def render_scene(self, scene, camera):
        if not self._initialized:
            return
        self.clear(scene.background_color)
        glViewport(0, 0, int(camera.aspect * 800), 800)

        view = camera.get_view_matrix()
        proj = camera.get_projection_matrix()
        camera_pos = camera.position

        if scene.grid_enabled:
            self._render_grid(scene, view, proj)

        shader = self.shader_manager.get_3d_shader()
        shader.use()
        shader.set_mat4("uView", view)
        shader.set_mat4("uProjection", proj)
        shader.set_vec3("uCameraPos", camera_pos)

        lights = scene.lights[:8]
        shader.set_int("uLightCount", len(lights))
        for i, light in enumerate(lights):
            prefix = f"uLights[{i}]"
            data = light.get_light_data()
            shader.set_vec3(f"{prefix}.position", data['position'])
            shader.set_vec3(f"{prefix}.direction", data['direction'])
            shader.set_vec3(f"{prefix}.color", data['color'])
            shader.set_int(f"{prefix}.type", data['type'])
            shader.set_float(f"{prefix}.range", data['range'])
            shader.set_float(f"{prefix}.spotAngle", data['spot_angle'])
            shader.set_float(f"{prefix}.spotExponent", data['spot_exponent'])
            shader.set_vec3(f"{prefix}.attenuation", data['attenuation'])

        self._render_node(scene.root, shader)

        if scene.selected_node is not None:
            self._render_selection_box(scene.selected_node, view, proj)

    def _render_node(self, node, shader):
        if not node.visible:
            return
        node.update_matrices()
        world = node.get_world_matrix()
        normal_mat = mat3_normal_from_mat4(world)

        if node.mesh is not None and node.material is not None:
            shader.set_mat4("uModel", world)
            shader.set_mat3("uNormalMatrix", normal_mat)
            mat = node.material
            shader.set_vec4("uDiffuse", mat.diffuse)
            shader.set_vec3("uSpecular", mat.specular[:3])
            shader.set_float("uShininess", mat.shininess)
            shader.set_float("uMetallic", mat.metallic)
            shader.set_float("uRoughness", mat.roughness)

            if mat.backface_culling:
                glEnable(GL_CULL_FACE)
            else:
                glDisable(GL_CULL_FACE)

            if mat.wireframe:
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)
            else:
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)

            buffers = self._get_or_create_mesh_buffers(node.mesh)
            glBindVertexArray(buffers['vao'])
            glDrawElements(GL_TRIANGLES, buffers['count'], GL_UNSIGNED_INT, None)
            glBindVertexArray(0)

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)
        glEnable(GL_CULL_FACE)

        for child in node.children:
            self._render_node(child, shader)

    def _render_grid(self, scene, view, proj):
        shader = self.shader_manager.get_grid_shader()
        shader.use()
        shader.set_mat4("uView", view)
        shader.set_mat4("uProjection", proj)
        shader.set_float("uGridSize", scene.grid_size)

        grid_data = self._gizmo_meshes['grid']
        glBindVertexArray(grid_data['vao'])
        glDrawArrays(GL_LINES, 0, grid_data['count'])
        glBindVertexArray(0)

    def _render_selection_box(self, node, view, proj):
        bb_min, bb_max = node.get_bounding_box()
        shader = self.shader_manager.get_line_shader()
        shader.use()
        mvp = proj @ view @ mat4_identity()
        shader.set_mat4("uMVP", mvp)

        corners = np.array([
            [bb_min[0], bb_min[1], bb_min[2]],
            [bb_max[0], bb_min[1], bb_min[2]],
            [bb_max[0], bb_max[1], bb_min[2]],
            [bb_min[0], bb_max[1], bb_min[2]],
            [bb_min[0], bb_min[1], bb_max[2]],
            [bb_max[0], bb_min[1], bb_max[2]],
            [bb_max[0], bb_max[1], bb_max[2]],
            [bb_min[0], bb_max[1], bb_max[2]],
        ], dtype=np.float32)

        edges = [
            0, 1, 1, 2, 2, 3, 3, 0,
            4, 5, 5, 6, 6, 7, 7, 4,
            0, 4, 1, 5, 2, 6, 3, 7,
        ]

        color = [1.0, 1.0, 0.0, 1.0]
        vertices = []
        for idx in edges:
            p = corners[idx]
            vertices.extend([p[0], p[1], p[2], *color])

        data = np.array(vertices, dtype=np.float32)

        vao = glGenVertexArrays(1)
        vbo = glGenBuffers(1)
        glBindVertexArray(vao)
        glBindBuffer(GL_ARRAY_BUFFER, vbo)
        glBufferData(GL_ARRAY_BUFFER, data.nbytes, data, GL_DYNAMIC_DRAW)
        stride = 7 * 4
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(0))
        glEnableVertexAttribArray(0)
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(12))
        glEnableVertexAttribArray(3)
        glDrawArrays(GL_LINES, 0, 24)
        glBindVertexArray(0)
        glDeleteBuffers(1, [vbo])
        glDeleteVertexArrays(1, [vao])

    def render_2d(self, canvas_2d, camera):
        if not self._initialized:
            return
        self.clear([0.2, 0.2, 0.25, 1.0])

        shader = self.shader_manager.get_3d_shader()
        shader.use()
        shader.set_mat4("uView", camera.get_view_matrix())
        shader.set_mat4("uProjection", camera.get_projection_matrix())
        shader.set_vec3("uCameraPos", camera.position)
        shader.set_int("uLightCount", 1)
        shader.set_vec3("uLights[0].position", np.array([0, 10, 5]))
        shader.set_vec3("uLights[0].direction", np.array([0, -1, 0]))
        shader.set_vec3("uLights[0].color", np.array([1, 1, 1]))
        shader.set_int("uLights[0].type", 1)

        for node in canvas_2d.get_all_nodes():
            if node.mesh and node.material:
                node.update_matrices()
                world = node.get_world_matrix()
                normal_mat = mat3_normal_from_mat4(world)
                shader.set_mat4("uModel", world)
                shader.set_mat3("uNormalMatrix", normal_mat)
                shader.set_vec4("uDiffuse", node.material.diffuse)
                shader.set_vec3("uSpecular", node.material.specular[:3])
                shader.set_float("uShininess", node.material.shininess)
                shader.set_float("uMetallic", 0.0)
                shader.set_float("uRoughness", 0.5)
                buffers = self._get_or_create_mesh_buffers(node.mesh)
                glBindVertexArray(buffers['vao'])
                glDrawElements(GL_TRIANGLES, buffers['count'], GL_UNSIGNED_INT, None)
                glBindVertexArray(0)

    def cleanup(self):
        for cached in self._mesh_cache.values():
            glDeleteVertexArrays(1, [cached['vao']])
            glDeleteBuffers(1, [cached['vbo']])
            glDeleteBuffers(1, [cached['ebo']])
        self._mesh_cache.clear()
        if self.shader_manager:
            self.shader_manager.destroy()
        self._initialized = False

    def invalidate_mesh(self, mesh):
        mesh_id = id(mesh)
        if mesh_id in self._mesh_cache:
            cached = self._mesh_cache.pop(mesh_id)
            glDeleteVertexArrays(1, [cached['vao']])
            glDeleteBuffers(1, [cached['vbo']])
            glDeleteBuffers(1, [cached['ebo']])
        mesh._dirty = True
