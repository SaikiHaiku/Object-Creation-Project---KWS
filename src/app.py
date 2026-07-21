import sys
import os
import numpy as np
from PyQt6.QtWidgets import (
    QMainWindow, QApplication, QWidget, QVBoxLayout, QHBoxLayout,
    QSplitter, QMenuBar, QMenu, QStatusBar, QFileDialog,
    QMessageBox, QLabel, QTabWidget, QDockWidget, QToolBar,
    QComboBox, QPushButton
)
from PyQt6.QtCore import Qt, QSize, QTimer
from PyQt6.QtGui import QAction, QFont, QColor

from .core.scene import Scene, SceneNode
from .core.camera import Camera
from .core.light import Light, LightType
from .core.material import Material
from .core.primitives import create_cube, create_sphere, create_cylinder, create_cone, create_torus, create_plane, create_torus_knot, create_ico_sphere, create_grid
from .editor.viewport import ViewportWidget
from .editor.toolbar import ToolbarWidget, PropertiesPanel
from .editor.scene_tree import SceneTreeWidget
from .editor.properties import AIPromptPanel
from .ai.generator import ObjectGenerator
from .exporters.exporters import OBJExporter, STLExporter, SVGExporter


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("OCP - Object Creation Project")
        self.setMinimumSize(1200, 800)
        self.resize(1600, 900)

        self.scene = Scene("My Scene")
        self.generator = ObjectGenerator()
        self.current_mode = "manual"
        self.current_tool = "move"

        self._setup_default_scene()
        self._create_ui()
        self._create_menu_bar()
        self._create_status_bar()
        self._connect_signals()

        self.viewport.set_scene(self.scene)
        self.scene_tree.set_scene(self.scene)
        self._update_status()

    def _setup_default_scene(self):
        self.scene.add_light(Light("Main Light", LightType.DIRECTIONAL))
        self.scene.lights[0].position = np.array([5, 10, 5], dtype=np.float32)
        self.scene.lights[0].direction = np.array([-0.5, -1, -0.5], dtype=np.float32)
        self.scene.lights[0].intensity = 1.2

        fill = Light("Fill Light", LightType.POINT)
        fill.position = np.array([-5, 3, -5], dtype=np.float32)
        fill.intensity = 0.4
        fill.color = np.array([0.5, 0.7, 1.0, 1.0], dtype=np.float32)
        self.scene.add_light(fill)

        ambient = Light("Ambient", LightType.AMBIENT)
        ambient.color = np.array([0.3, 0.3, 0.4, 1.0], dtype=np.float32)
        ambient.intensity = 0.5
        self.scene.add_light(ambient)

        cube = SceneNode("Demo Cube")
        cube.mesh = create_cube(1.0)
        cube.material = Material("Red Material")
        cube.material.diffuse = np.array([0.8, 0.2, 0.2, 1.0], dtype=np.float32)
        cube.material.shininess = 64.0
        cube.position = np.array([0, 0.5, 0], dtype=np.float32)
        self.scene.add_node(cube)

        sphere = SceneNode("Demo Sphere")
        sphere.mesh = create_sphere(0.5, 32, 16)
        sphere.material = Material("Blue Material")
        sphere.material.diffuse = np.array([0.2, 0.3, 0.9, 1.0], dtype=np.float32)
        sphere.material.metallic = 0.7
        sphere.material.shininess = 80.0
        sphere.position = np.array([2.5, 0.5, 0], dtype=np.float32)
        self.scene.add_node(sphere)

        torus = SceneNode("Demo Torus")
        torus.mesh = create_torus(0.4, 0.15, 32, 16)
        torus.material = Material("Gold Material")
        torus.material.diffuse = np.array([1.0, 0.84, 0.0, 1.0], dtype=np.float32)
        torus.material.metallic = 0.9
        torus.material.shininess = 100.0
        torus.position = np.array([-2.5, 0.8, 0], dtype=np.float32)
        torus.rotation = np.array([30, 45, 0], dtype=np.float32)
        self.scene.add_node(torus)

        plane = SceneNode("Ground Plane")
        plane.mesh = create_plane(10.0)
        plane.material = Material("Ground")
        plane.material.diffuse = np.array([0.35, 0.4, 0.35, 1.0], dtype=np.float32)
        plane.material.roughness = 0.9
        self.scene.add_node(plane)

        self.scene.camera = Camera()
        self.scene.cameras.append(self.scene.camera)

    def _create_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QVBoxLayout(central)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)

        self.toolbar = ToolbarWidget()
        main_layout.addWidget(self.toolbar)

        splitter_h = QSplitter(Qt.Orientation.Horizontal)

        self.scene_tree = SceneTreeWidget()
        splitter_h.addWidget(self.scene_tree)

        center_widget = QWidget()
        center_layout = QVBoxLayout(center_widget)
        center_layout.setContentsMargins(0, 0, 0, 0)
        self.viewport = ViewportWidget()
        center_layout.addWidget(self.viewport)
        splitter_h.addWidget(center_widget)

        right_panel = QSplitter(Qt.Orientation.Vertical)

        self.properties_panel = PropertiesPanel()
        right_panel.addWidget(self.properties_panel)

        self.ai_panel = AIPromptPanel()
        right_panel.addWidget(self.ai_panel)

        right_panel.setSizes([400, 300])
        splitter_h.addWidget(right_panel)

        splitter_h.setSizes([220, 900, 350])

        main_layout.addWidget(splitter_h)

    def _create_menu_bar(self):
        menubar = self.menuBar()

        file_menu = menubar.addMenu("File")

        new_action = QAction("New Scene", self)
        new_action.setShortcut("Ctrl+N")
        new_action.triggered.connect(self._new_scene)
        file_menu.addAction(new_action)

        open_action = QAction("Open OBJ...", self)
        open_action.setShortcut("Ctrl+O")
        file_menu.addAction(open_action)

        file_menu.addSeparator()

        save_obj = QAction("Export OBJ...", self)
        save_obj.setShortcut("Ctrl+Shift+O")
        save_obj.triggered.connect(lambda: self._export("obj"))
        file_menu.addAction(save_obj)

        save_stl = QAction("Export STL...", self)
        save_stl.triggered.connect(lambda: self._export("stl"))
        file_menu.addAction(save_stl)

        save_svg = QAction("Export SVG...", self)
        save_svg.triggered.connect(lambda: self._export("svg"))
        file_menu.addAction(save_svg)

        save_img = QAction("Export Image...", self)
        save_img.setShortcut("Ctrl+Shift+S")
        save_img.triggered.connect(lambda: self._export("png"))
        file_menu.addAction(save_img)

        file_menu.addSeparator()

        exit_action = QAction("Exit", self)
        exit_action.setShortcut("Alt+F4")
        exit_action.triggered.connect(self.close)
        file_menu.addAction(exit_action)

        edit_menu = menubar.addMenu("Edit")

        undo_action = QAction("Undo", self)
        undo_action.setShortcut("Ctrl+Z")
        edit_menu.addAction(undo_action)

        redo_action = QAction("Redo", self)
        redo_action.setShortcut("Ctrl+Y")
        edit_menu.addAction(redo_action)

        edit_menu.addSeparator()

        dup_action = QAction("Duplicate", self)
        dup_action.setShortcut("Ctrl+D")
        dup_action.triggered.connect(self._duplicate_selected)
        edit_menu.addAction(dup_action)

        del_action = QAction("Delete", self)
        del_action.setShortcut("Delete")
        del_action.triggered.connect(self._delete_selected)
        edit_menu.addAction(del_action)

        edit_menu.addSeparator()

        select_all = QAction("Select All", self)
        select_all.setShortcut("Ctrl+A")
        edit_menu.addAction(select_all)

        view_menu = menubar.addMenu("View")

        for view_name in ["Front", "Back", "Left", "Right", "Top", "Bottom", "Perspective"]:
            action = QAction(view_name, self)
            action.triggered.connect(lambda _, v=view_name.lower(): self.viewport.set_view(v))
            view_menu.addAction(action)

        view_menu.addSeparator()

        focus_action = QAction("Focus Selected", self)
        focus_action.setShortcut("F")
        focus_action.triggered.connect(self.viewport.focus_selected)
        view_menu.addAction(focus_action)

        frame_action = QAction("Frame All", self)
        frame_action.setShortcut("Shift+F")
        frame_action.triggered.connect(self.viewport.frame_all)
        view_menu.addAction(frame_action)

        add_menu = menubar.addMenu("Add")
        primitives = [
            ("Cube", "cube"), ("Sphere", "sphere"), ("Cylinder", "cylinder"),
            ("Cone", "cone"), ("Torus", "torus"), ("Plane", "plane"),
            ("Torus Knot", "torus_knot"), ("Ico Sphere", "ico_sphere"),
        ]
        for label, shape in primitives:
            action = QAction(label, self)
            action.triggered.connect(lambda _, s=shape: self._add_primitive(s))
            add_menu.addAction(action)

        help_menu = menubar.addMenu("Help")
        about_action = QAction("About OCP", self)
        about_action.triggered.connect(self._show_about)
        help_menu.addAction(about_action)

    def _create_status_bar(self):
        self.statusBar().showMessage("Ready | OCP v1.0 by KitariosWebStudio - KWS")

    def _connect_signals(self):
        self.toolbar.primitive_added.connect(self._add_primitive)
        self.toolbar.view_changed.connect(lambda v: self.viewport.set_view(v))
        self.toolbar.mode_changed.connect(self._switch_mode)
        self.toolbar.tool_changed.connect(self._switch_tool)

        self.scene_tree.node_selected.connect(self._on_node_selected)
        self.scene_tree.node_deleted.connect(self._on_node_deleted)
        self.scene_tree.node_visibility_changed.connect(self._on_visibility_changed)

        self.properties_panel.property_changed.connect(self._on_property_changed)

        self.ai_panel.generation_requested.connect(self._on_ai_generation)

        self._update_timer = QTimer()
        self._update_timer.timeout.connect(self._update_viewport)
        self._update_timer.start(16)

    def _update_viewport(self):
        if self.scene:
            self.scene.update()
        self.viewport.update()

    def _add_primitive(self, shape_name):
        creators = {
            'cube': create_cube, 'sphere': create_sphere,
            'cylinder': create_cylinder, 'cone': create_cone,
            'torus': create_torus, 'plane': create_plane,
            'torus_knot': create_torus_knot, 'ico_sphere': create_ico_sphere,
        }
        if shape_name not in creators:
            return

        node = SceneNode(shape_name.capitalize())
        node.mesh = creators[shape_name]()
        node.material = Material(f"{shape_name}_mat")
        node.material.diffuse = np.array([0.7, 0.3, 0.2, 1.0], dtype=np.float32)

        count = self.scene.get_node_count()
        node.position = np.array([count * 2.0, 0.5, 0], dtype=np.float32)
        if shape_name == 'plane':
            node.position[1] = 0.0

        self.scene.add_node(node)
        self.scene.select(node)
        self.scene_tree.refresh()
        self.properties_panel.set_node(node)
        self._update_status()

    def _switch_mode(self, mode):
        self.current_mode = mode
        if mode == "manual":
            self.ai_panel.setVisible(False)
            self.properties_panel.setVisible(True)
        else:
            self.ai_panel.setVisible(True)
            self.properties_panel.setVisible(True)
        self._update_status()

    def _switch_tool(self, tool):
        self.current_tool = tool
        self._update_status()

    def _on_node_selected(self, node):
        self.scene.select(node)
        self.properties_panel.set_node(node)
        self.scene_tree.highlight_node(node)
        self.viewport.update()

    def _on_node_deselected(self):
        self.scene.deselect()
        self.properties_panel.set_node(None)
        self.viewport.update()

    def _on_node_deleted(self, node):
        self.scene.remove_node(node)
        self.scene.deselect()
        self.properties_panel.set_node(None)
        self.scene_tree.refresh()
        self.viewport.update()

    def _on_visibility_changed(self, node, visible):
        self.viewport.update()

    def _on_property_changed(self):
        self.scene_tree.refresh()
        self.viewport.update()

    def _on_ai_generation(self, prompt):
        if prompt == "__CLEAR__":
            self.scene.clear()
            self._setup_default_scene()
            self.scene_tree.set_scene(self.scene)
            self.viewport.set_scene(self.scene)
            self.ai_panel.on_generation_done("Scene cleared")
            return

        try:
            new_scene, nodes = self.generator.generate_from_prompt(prompt)
            self.scene = new_scene
            self._setup_default_scene()
            for node in self.scene.root.children[:]:
                pass

            generated_scene, nodes = self.generator.generate_from_prompt(prompt)
            for light in self.scene.lights:
                generated_scene.add_light(light)
            self.scene = generated_scene
            self.scene.cameras = [Camera()]

            self.viewport.set_scene(self.scene)
            self.scene_tree.set_scene(self.scene)

            desc = self.generator.get_last_description()
            self.ai_panel.on_generation_done(f"Created {len(nodes)} objects | {desc}")
            self._update_status()
        except Exception as e:
            self.ai_panel.on_generation_error(str(e))

    def _duplicate_selected(self):
        dup = self.scene.duplicate_selected()
        if dup:
            self.scene_tree.refresh()

    def _delete_selected(self):
        self.scene.delete_selected()
        self.properties_panel.set_node(None)
        self.scene_tree.refresh()
        self.viewport.update()

    def _new_scene(self):
        reply = QMessageBox.question(
            self, "New Scene",
            "Create a new scene? Current changes will be lost.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if reply == QMessageBox.StandardButton.Yes:
            self.scene.clear()
            self._setup_default_scene()
            self.scene_tree.set_scene(self.scene)
            self.viewport.set_scene(self.scene)

    def _export(self, fmt):
        if fmt == "obj":
            path, _ = QFileDialog.getSaveFileName(self, "Export OBJ", "scene.obj", "OBJ Files (*.obj)")
            if path:
                count = OBJExporter.export(self.scene, path)
                self.statusBar().showMessage(f"Exported {count} objects to {path}")

        elif fmt == "stl":
            path, _ = QFileDialog.getSaveFileName(self, "Export STL", "scene.stl", "STL Files (*.stl)")
            if path:
                count = STLExporter.export(self.scene, path)
                self.statusBar().showMessage(f"Exported {count} triangles to {path}")

        elif fmt == "svg":
            path, _ = QFileDialog.getSaveFileName(self, "Export SVG", "scene.svg", "SVG Files (*.svg)")
            if path:
                count = SVGExporter.export_2d(self.scene, path)
                self.statusBar().showMessage(f"Exported {count} objects to {path}")

        elif fmt == "png":
            path, _ = QFileDialog.getSaveFileName(self, "Export Image", "render.png", "PNG Files (*.png)")
            if path:
                try:
                    from .exporters.exporters import ImageExporter
                    ImageExporter.export(self.viewport.renderer, path, self.viewport.width(), self.viewport.height())
                    self.statusBar().showMessage(f"Exported image to {path}")
                except Exception as e:
                    self.statusBar().showMessage(f"Export failed: {e}")

    def _show_about(self):
        QMessageBox.about(
            self,
            "About OCP",
            "<h2>OCP v1.0</h2>"
            "<p>Object Creation Project</p>"
            "<p>Developpe par <b>KitariosWebStudio - KWS</b></p>"
            "<p>Creation d'objets 3D et 2D avec edition manuelle et generation par IA.</p>"
            "<p><b>Fonctionnalites:</b></p>"
            "<ul>"
            "<li>Editeur 3D complet avec rendu OpenGL</li>"
            "<li>Mode manuel: primitives, transformations, materiaux</li>"
            "<li>Mode IA: generer des scenes complexes depuis du texte</li>"
            "<li>Export OBJ, STL, SVG, PNG</li>"
            "<li>100% local - aucune connexion requise</li>"
            "</ul>"
        )

    def _update_status(self):
        count = self.scene.get_node_count()
        mode_str = "MANUAL" if self.current_mode == "manual" else "AI AUTO"
        tool_str = self.current_tool.capitalize()
        selected = self.scene.selected_node.name if self.scene.selected_node else "None"
        self.statusBar().showMessage(
            f"Mode: {mode_str} | Tool: {tool_str} | "
            f"Objects: {count} | Selected: {selected}"
        )
