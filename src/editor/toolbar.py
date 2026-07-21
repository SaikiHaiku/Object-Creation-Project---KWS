from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QToolBar,
    QComboBox, QLabel, QSpinBox, QDoubleSpinBox, QGroupBox,
    QLineEdit, QTextEdit, QSplitter, QFrame, QScrollArea,
    QCheckBox, QTabWidget, QColorDialog, QMessageBox, QSlider,
    QProgressBar, QSizePolicy
)
from PyQt6.QtCore import Qt, QSize, pyqtSignal
from PyQt6.QtGui import QIcon, QColor, QFont, QAction
import numpy as np


class ToolbarWidget(QWidget):
    tool_changed = pyqtSignal(str)
    primitive_added = pyqtSignal(str)
    view_changed = pyqtSignal(str)
    mode_changed = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedHeight(60)
        layout = QHBoxLayout(self)
        layout.setContentsMargins(5, 5, 5, 5)

        mode_group = self._create_group("Mode")
        mode_layout = QHBoxLayout()
        self.btn_manual = QPushButton("MANUAL")
        self.btn_manual.setCheckable(True)
        self.btn_manual.setChecked(True)
        self.btn_manual.setMinimumWidth(80)
        self.btn_manual.clicked.connect(lambda: self.mode_changed.emit("manual"))
        self.btn_auto = QPushButton("AI AUTO")
        self.btn_auto.setCheckable(True)
        self.btn_auto.setMinimumWidth(80)
        self.btn_auto.clicked.connect(lambda: self.mode_changed.emit("auto"))
        mode_layout.addWidget(self.btn_manual)
        mode_layout.addWidget(self.btn_auto)
        mode_group.setLayout(mode_layout)
        layout.addWidget(mode_group)

        prim_group = self._create_group("Add")
        prim_layout = QHBoxLayout()
        primitives = ["Cube", "Sphere", "Cylinder", "Cone", "Torus", "Plane", "TorusKnot", "IcoSphere"]
        for prim in primitives:
            btn = QPushButton(prim[:3])
            btn.setToolTip(f"Add {prim}")
            btn.setMaximumWidth(35)
            btn.clicked.connect(lambda _, p=prim.lower(): self.primitive_added.emit(p))
            prim_layout.addWidget(btn)
        prim_group.setLayout(prim_layout)
        layout.addWidget(prim_group)

        view_group = self._create_group("View")
        view_layout = QHBoxLayout()
        views = [("Front", "front"), ("Top", "top"), ("Right", "right"), ("3D", "front_right")]
        for label, name in views:
            btn = QPushButton(label)
            btn.setMaximumWidth(45)
            btn.clicked.connect(lambda _, n=name: self.view_changed.emit(n))
            view_layout.addWidget(btn)
        view_group.setLayout(view_layout)
        layout.addWidget(view_group)

        tool_group = self._create_group("Tools")
        tool_layout = QHBoxLayout()
        tools = [("Move", "move"), ("Rotate", "rotate"), ("Scale", "scale")]
        self.tool_buttons = []
        for label, name in tools:
            btn = QPushButton(label)
            btn.setCheckable(True)
            btn.setMaximumWidth(55)
            btn.clicked.connect(lambda _, n=name: self._select_tool(n))
            tool_layout.addWidget(btn)
            self.tool_buttons.append(btn)
        self.tool_buttons[0].setChecked(True)
        tool_group.setLayout(tool_layout)
        layout.addWidget(tool_group)

        layout.addStretch()

    def _create_group(self, title):
        group = QGroupBox(title)
        group.setStyleSheet("QGroupBox { font-weight: bold; font-size: 10px; padding-top: 15px; }")
        return group

    def _select_tool(self, name):
        for btn in self.tool_buttons:
            btn.setChecked(False)
        for btn in self.tool_buttons:
            if btn.text().lower() == name:
                btn.setChecked(True)
        self.tool_changed.emit(name)


class PropertiesPanel(QWidget):
    property_changed = pyqtSignal()
    node_renamed = pyqtSignal(str, str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumWidth(280)
        self.setMaximumWidth(350)
        self.current_node = None

        layout = QVBoxLayout(self)
        layout.setContentsMargins(5, 5, 5, 5)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        content = QWidget()
        self.content_layout = QVBoxLayout(content)
        self.content_layout.setAlignment(Qt.AlignmentFlag.AlignTop)

        self._create_transform_section()
        self._create_material_section()
        self._create_mesh_section()

        self.content_layout.addStretch()
        scroll.setWidget(content)
        layout.addWidget(scroll)

        self._set_enabled_all(False)

    def _create_transform_section(self):
        group = QGroupBox("Transform")
        layout = QVBoxLayout()

        name_row = QHBoxLayout()
        name_row.addWidget(QLabel("Name:"))
        self.name_input = QLineEdit()
        self.name_input.returnPressed.connect(self._on_name_changed)
        name_row.addWidget(self.name_input)
        layout.addLayout(name_row)

        pos_row = QHBoxLayout()
        pos_row.addWidget(QLabel("Pos:"))
        self.pos_x = self._create_spin(-100, 100, 0.1)
        self.pos_y = self._create_spin(-100, 100, 0.1)
        self.pos_z = self._create_spin(-100, 100, 0.1)
        for sp in [self.pos_x, self.pos_y, self.pos_z]:
            sp.valueChanged.connect(self._on_transform_changed)
            pos_row.addWidget(sp)
        layout.addLayout(pos_row)

        rot_row = QHBoxLayout()
        rot_row.addWidget(QLabel("Rot:"))
        self.rot_x = self._create_spin(-360, 360, 1.0)
        self.rot_y = self._create_spin(-360, 360, 1.0)
        self.rot_z = self._create_spin(-360, 360, 1.0)
        for sp in [self.rot_x, self.rot_y, self.rot_z]:
            sp.valueChanged.connect(self._on_transform_changed)
            rot_row.addWidget(sp)
        layout.addLayout(rot_row)

        scl_row = QHBoxLayout()
        scl_row.addWidget(QLabel("Scl:"))
        self.scl_x = self._create_spin(0.01, 100, 0.1)
        self.scl_y = self._create_spin(0.01, 100, 0.1)
        self.scl_z = self._create_spin(0.01, 100, 0.1)
        for sp in [self.scl_x, self.scl_y, self.scl_z]:
            sp.setValue(1.0)
            sp.valueChanged.connect(self._on_transform_changed)
            scl_row.addWidget(sp)
        layout.addLayout(scl_row)

        group.setLayout(layout)
        self.content_layout.addWidget(group)

    def _create_material_section(self):
        group = QGroupBox("Material")
        layout = QVBoxLayout()

        color_row = QHBoxLayout()
        color_row.addWidget(QLabel("Color:"))
        self.color_btn = QPushButton()
        self.color_btn.setFixedSize(40, 25)
        self.color_btn.setStyleSheet("background-color: rgb(204, 204, 204);")
        self.color_btn.clicked.connect(self._pick_color)
        color_row.addWidget(self.color_btn)

        self.metallic_slider = self._create_slider("Metallic", 0, 100, 0)
        self.roughness_slider = self._create_slider("Roughness", 0, 100, 50)
        self.shininess_slider = self._create_slider("Shininess", 1, 200, 32)

        self.wireframe_cb = QCheckBox("Wireframe")
        self.wireframe_cb.toggled.connect(self._on_material_changed)
        layout.addWidget(self.wireframe_cb)

        self.backface_cb = QCheckBox("Backface Culling")
        self.backface_cb.setChecked(True)
        self.backface_cb.toggled.connect(self._on_material_changed)
        layout.addWidget(self.backface_cb)

        layout.addLayout(color_row)
        layout.addWidget(self.metallic_slider[0])
        layout.addWidget(self.roughness_slider[0])
        layout.addWidget(self.shininess_slider[0])

        group.setLayout(layout)
        self.content_layout.addWidget(group)

    def _create_mesh_section(self):
        group = QGroupBox("Mesh Info")
        layout = QVBoxLayout()
        self.mesh_info_label = QLabel("No mesh selected")
        self.mesh_info_label.setWordWrap(True)
        layout.addWidget(self.mesh_info_label)

        btn_row = QHBoxLayout()
        self.btn_subdivide = QPushButton("Subdivide")
        self.btn_subdivide.clicked.connect(self._on_subdivide)
        btn_row.addWidget(self.btn_subdivide)
        self.btn_invert = QPushButton("Invert Faces")
        self.btn_invert.clicked.connect(self._on_invert)
        btn_row.addWidget(self.btn_invert)
        layout.addLayout(btn_row)

        group.setLayout(layout)
        self.content_layout.addWidget(group)

    def _create_spin(self, min_val, max_val, step):
        spin = QDoubleSpinBox()
        spin.setRange(min_val, max_val)
        spin.setSingleStep(step)
        spin.setDecimals(2)
        spin.setMinimumWidth(60)
        return spin

    def _create_slider(self, label, min_val, max_val, default):
        widget = QWidget()
        layout = QHBoxLayout(widget)
        layout.setContentsMargins(0, 0, 0, 0)
        lbl = QLabel(label + ":")
        lbl.setFixedWidth(60)
        layout.addWidget(lbl)
        slider = QSlider(Qt.Orientation.Horizontal)
        slider.setRange(min_val, max_val)
        slider.setValue(default)
        slider.valueChanged.connect(self._on_material_changed)
        layout.addWidget(slider)
        val_label = QLabel(str(default))
        val_label.setFixedWidth(30)
        slider.valueChanged.connect(lambda v, l=val_label: l.setText(str(v)))
        layout.addWidget(val_label)
        return widget, slider

    def set_node(self, node):
        self.current_node = node
        if node is None:
            self._set_enabled_all(False)
            return
        self._set_enabled_all(True)
        self.name_input.setText(node.name)
        self.pos_x.setValue(float(node.position[0]))
        self.pos_y.setValue(float(node.position[1]))
        self.pos_z.setValue(float(node.position[2]))
        self.rot_x.setValue(float(node.rotation[0]))
        self.rot_y.setValue(float(node.rotation[1]))
        self.rot_z.setValue(float(node.rotation[2]))
        self.scl_x.setValue(float(node.scale[0]))
        self.scl_y.setValue(float(node.scale[1]))
        self.scl_z.setValue(float(node.scale[2]))
        if node.material:
            c = node.material.diffuse
            self.color_btn.setStyleSheet(f"background-color: rgb({int(c[0]*255)},{int(c[1]*255)},{int(c[2]*255)});")
            self.wireframe_cb.setChecked(node.material.wireframe)
            self.backface_cb.setChecked(node.material.backface_culling)
        if node.mesh:
            self.mesh_info_label.setText(
                f"Vertices: {node.mesh.get_vertex_count()}\n"
                f"Triangles: {node.mesh.get_triangle_count()}\n"
                f"Faces: {len(node.mesh.faces)}"
            )

    def _on_transform_changed(self):
        if self.current_node is None:
            return
        self.current_node.position = np.array([
            self.pos_x.value(), self.pos_y.value(), self.pos_z.value()
        ], dtype=np.float32)
        self.current_node.rotation = np.array([
            self.rot_x.value(), self.rot_y.value(), self.rot_z.value()
        ], dtype=np.float32)
        self.current_node.scale = np.array([
            self.scl_x.value(), self.scl_y.value(), self.scl_z.value()
        ], dtype=np.float32)
        self.current_node.set_dirty()
        self.property_changed.emit()

    def _on_material_changed(self):
        if self.current_node is None or self.current_node.material is None:
            return
        mat = self.current_node.material
        mat.wireframe = self.wireframe_cb.isChecked()
        mat.backface_culling = self.backface_cb.isChecked()
        self.property_changed.emit()

    def _on_name_changed(self):
        if self.current_node is None:
            return
        old_name = self.current_node.name
        new_name = self.name_input.text()
        self.current_node.name = new_name
        self.node_renamed.emit(old_name, new_name)

    def _pick_color(self):
        if self.current_node is None or self.current_node.material is None:
            return
        c = self.current_node.material.diffuse
        color = QColorDialog.getColor(QColor(int(c[0]*255), int(c[1]*255), int(c[2]*255)), self)
        if color.isValid():
            self.current_node.material.diffuse = np.array([
                color.red() / 255.0, color.green() / 255.0, color.blue() / 255.0, 1.0
            ], dtype=np.float32)
            self.color_btn.setStyleSheet(f"background-color: rgb({color.red()},{color.green()},{color.blue()});")
            self.property_changed.emit()

    def _on_subdivide(self):
        if self.current_node and self.current_node.mesh:
            self.current_node.mesh.subdivide()
            self.current_node.set_dirty()
            self.set_node(self.current_node)
            self.property_changed.emit()

    def _on_invert(self):
        if self.current_node and self.current_node.mesh:
            self.current_node.mesh.invert_faces()
            self.current_node.set_dirty()
            self.property_changed.emit()

    def _set_enabled_all(self, enabled):
        for child in self.findChildren(QDoubleSpinBox):
            child.setEnabled(enabled)
        for child in self.findChildren(QLineEdit):
            child.setEnabled(enabled)
        for child in self.findChildren(QCheckBox):
            child.setEnabled(enabled)
        self.btn_subdivide.setEnabled(enabled)
        self.btn_invert.setEnabled(enabled)
        self.color_btn.setEnabled(enabled)
