import numpy as np
import math
from enum import Enum
from ..utils.math_utils import mat4_perspective, mat4_look_at, mat4_ortho, mat4_inverse


class ProjectionType(Enum):
    PERSPECTIVE = 0
    ORTHOGRAPHIC = 1


class Camera:
    def __init__(self, name="Camera"):
        self.name = name
        self.position = np.array([5.0, 5.0, 5.0], dtype=np.float32)
        self.target = np.array([0.0, 0.0, 0.0], dtype=np.float32)
        self.up = np.array([0.0, 1.0, 0.0], dtype=np.float32)
        self.fov = 60.0
        self.near = 0.1
        self.far = 1000.0
        self.aspect = 16.0 / 9.0
        self.projection_type = ProjectionType.PERSPECTIVE
        self.ortho_size = 10.0
        self._yaw = -45.0
        self._pitch = 30.0
        self._distance = 10.0

    def get_view_matrix(self):
        return mat4_look_at(self.position, self.target, self.up).astype(np.float32)

    def get_projection_matrix(self):
        if self.projection_type == ProjectionType.PERSPECTIVE:
            return mat4_perspective(self.fov, self.aspect, self.near, self.far)
        else:
            h = self.ortho_size
            w = h * self.aspect
            return mat4_ortho(-w, w, -h, h, self.near, self.far)

    def orbit(self, delta_yaw, delta_pitch):
        self._yaw += delta_yaw
        self._pitch = max(-89.0, min(89.0, self._pitch + delta_pitch))
        self._update_position_from_orbit()

    def zoom(self, delta):
        self._distance = max(0.5, self._distance + delta)
        self._update_position_from_orbit()

    def pan(self, dx, dy):
        forward = self.target - self.position
        forward = forward / (np.linalg.norm(forward) + 1e-8)
        right = np.cross(forward, self.up)
        right = right / (np.linalg.norm(right) + 1e-8)
        up = np.cross(right, forward)
        up = up / (np.linalg.norm(up) + 1e-8)
        speed = self._distance * 0.002
        self.target += right * dx * speed + up * dy * speed
        self._update_position_from_orbit()

    def _update_position_from_orbit(self):
        yaw_rad = math.radians(self._yaw)
        pitch_rad = math.radians(self._pitch)
        x = self._distance * math.cos(pitch_rad) * math.cos(yaw_rad)
        y = self._distance * math.sin(pitch_rad)
        z = self._distance * math.cos(pitch_rad) * math.sin(yaw_rad)
        self.position = self.target + np.array([x, y, z], dtype=np.float32)

    def focus_on(self, center, size):
        self.target = np.asarray(center, dtype=np.float32)
        self._distance = size * 2.0
        self._update_position_from_orbit()

    def set_view(self, view_name):
        self._pitch = 0.0
        views = {
            "front": (0.0, 0.0), "back": (180.0, 0.0),
            "left": (90.0, 0.0), "right": (-90.0, 0.0),
            "top": (0.0, 89.0), "bottom": (0.0, -89.0),
            "front_right": (-45.0, 30.0),
        }
        if view_name in views:
            self._yaw, self._pitch = views[view_name]
        self._update_position_from_orbit()

    def screen_to_ray(self, screen_x, screen_y, viewport_width, viewport_height):
        ndc_x = (2.0 * screen_x / viewport_width) - 1.0
        ndc_y = 1.0 - (2.0 * screen_y / viewport_height)
        proj = self.get_projection_matrix()
        view = self.get_view_matrix()
        inv_vp = mat4_inverse(proj @ view)
        near_point = np.array([ndc_x, ndc_y, -1.0, 1.0], dtype=np.float32)
        far_point = np.array([ndc_x, ndc_y, 1.0, 1.0], dtype=np.float32)
        near_w = inv_vp @ near_point
        far_w = inv_vp @ far_point
        near_w /= near_w[3]
        far_w /= far_w[3]
        direction = far_w[:3] - near_w[:3]
        direction = direction / (np.linalg.norm(direction) + 1e-8)
        return near_w[:3], direction

    def clone(self):
        c = Camera(self.name + "_copy")
        c.position = self.position.copy()
        c.target = self.target.copy()
        c.up = self.up.copy()
        c.fov = self.fov
        c.near = self.near
        c.far = self.far
        c.aspect = self.aspect
        c.projection_type = self.projection_type
        c.ortho_size = self.ortho_size
        c._yaw = self._yaw
        c._pitch = self._pitch
        c._distance = self._distance
        return c
