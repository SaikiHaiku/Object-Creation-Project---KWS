from PyQt6.QtWidgets import QOpenGLWidget
from PyQt6.QtCore import Qt, QTimer, QPoint
from PyQt6.QtGui import QMouseEvent, QWheelEvent
from OpenGL.GL import *
import numpy as np
from ..renderer.opengl_renderer import OpenGLRenderer
from ..core.camera import Camera, ProjectionType


class ViewportWidget(QOpenGLWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.renderer = OpenGLRenderer()
        self.camera = Camera()
        self.scene = None
        self.mode = "3d"

        self._last_mouse_pos = QPoint()
        self._mouse_button = Qt.MouseButton.NoButton
        self._orbiting = False
        self._panning = False
        self._zooming = False

        self.setMinimumSize(400, 300)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

    def initializeGL(self):
        self.renderer.initialize()

    def resizeGL(self, w, h):
        self.camera.aspect = w / max(h, 1)
        glViewport(0, 0, w, h)

    def paintGL(self):
        if self.scene:
            self.camera.aspect = self.width() / max(self.height(), 1)
            if self.mode == "3d":
                self.renderer.render_scene(self.scene, self.camera)
            else:
                self.renderer.clear(self.scene.background_color)

    def set_scene(self, scene):
        self.scene = scene
        self.update()

    def set_mode(self, mode):
        self.mode = mode
        self.update()

    def mousePressEvent(self, event):
        self._last_mouse_pos = event.position().toPoint()
        self._mouse_button = event.button()

        if event.button() == Qt.MouseButton.MiddleButton:
            if event.modifiers() & Qt.KeyboardModifier.ShiftModifier:
                self._panning = True
            else:
                self._orbiting = True
        elif event.button() == Qt.MouseButton.RightButton:
            self._panning = True
        elif event.button() == Qt.MouseButton.LeftButton:
            if self.scene and self.mode == "3d":
                self._pick_object(event.position().toPoint())

    def mouseMoveEvent(self, event):
        pos = event.position().toPoint()
        dx = pos.x() - self._last_mouse_pos.x()
        dy = pos.y() - self._last_mouse_pos.y()

        if self._orbiting:
            self.camera.orbit(dx * 0.5, dy * 0.5)
            self.update()
        elif self._panning:
            self.camera.pan(dx, -dy)
            self.update()
        elif self._zooming:
            self.camera.zoom(-dy * 0.05)
            self.update()

        self._last_mouse_pos = pos

    def mouseReleaseEvent(self, event):
        self._orbiting = False
        self._panning = False
        self._zooming = False
        self._mouse_button = Qt.MouseButton.NoButton

    def wheelEvent(self, event):
        delta = event.angleDelta().y() / 120.0
        self.camera.zoom(-delta * 0.5)
        self.update()

    def _pick_object(self, pos):
        origin, direction = self.camera.screen_to_ray(
            pos.x(), pos.y(), self.width(), self.height()
        )
        hit, dist = self.scene.raycast(origin, direction)
        if hit is not None:
            self.scene.select(hit)
            if hasattr(self.parent(), 'on_node_selected'):
                self.parent().on_node_selected(hit)
        else:
            self.scene.deselect()
            if hasattr(self.parent(), 'on_node_deselected'):
                self.parent().on_node_deselected()
        self.update()

    def set_perspective(self):
        self.camera.projection_type = ProjectionType.PERSPECTIVE
        self.update()

    def set_orthographic(self):
        self.camera.projection_type = ProjectionType.ORTHOGRAPHIC
        self.update()

    def set_view(self, name):
        self.camera.set_view(name)
        self.update()

    def focus_selected(self):
        if self.scene and self.scene.selected_node:
            bb_min, bb_max = self.scene.selected_node.get_bounding_box()
            center = (bb_min + bb_max) / 2.0
            size = np.linalg.norm(bb_max - bb_min)
            self.camera.focus_on(center, size)
            self.update()

    def frame_all(self):
        if self.scene:
            nodes = self.scene.get_all_nodes()
            if nodes:
                all_min = np.array([float('inf')] * 3)
                all_max = np.array([float('-inf')] * 3)
                for n in nodes:
                    try:
                        bb_min, bb_max = n.get_bounding_box()
                        all_min = np.minimum(all_min, bb_min)
                        all_max = np.maximum(all_max, bb_max)
                    except:
                        pass
                center = (all_min + all_max) / 2.0
                size = np.linalg.norm(all_max - all_min)
                if size < 0.01:
                    size = 5.0
                self.camera.focus_on(center, size)
                self.update()
