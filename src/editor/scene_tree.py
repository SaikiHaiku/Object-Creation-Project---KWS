from PyQt6.QtWidgets import (
    QTreeWidget, QTreeWidgetItem, QMenu, QInputDialog,
    QMessageBox, QAbstractItemView
)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QAction


class SceneTreeWidget(QTreeWidget):
    node_selected = pyqtSignal(object)
    node_deleted = pyqtSignal(object)
    node_renamed = pyqtSignal(object, str)
    node_visibility_changed = pyqtSignal(object, bool)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setHeaderLabel("Scene Hierarchy")
        self.setMinimumWidth(200)
        self.setMaximumWidth(300)
        self.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.customContextMenuRequested.connect(self._show_context_menu)
        self.itemClicked.connect(self._on_item_clicked)
        self.itemDoubleClicked.connect(self._on_item_double_clicked)
        self.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.scene = None
        self._node_items = {}

    def set_scene(self, scene):
        self.scene = scene
        self.refresh()

    def refresh(self):
        self.clear()
        self._node_items.clear()
        if self.scene is None:
            return
        root_item = QTreeWidgetItem(self)
        root_item.setText(0, "Scene")
        root_item.setExpanded(True)
        self._add_children(self.scene.root, root_item)

    def _add_children(self, node, parent_item):
        for child in node.children:
            item = QTreeWidgetItem(parent_item)
            item.setText(0, child.name)
            item.setExpanded(True)
            self._node_items[id(child)] = item
            item.setData(0, Qt.ItemDataRole.UserRole, child)

            if child.mesh:
                icon_text = "[M]"
            else:
                icon_text = "[N]"
            item.setText(0, f"{icon_text} {child.name}")

            if not child.visible:
                item.setForeground(0, self.palette().color(self.palette().ColorGroup.Disabled, self.palette().ColorRole.Text))

            self._add_children(child, item)

    def _on_item_clicked(self, item, column):
        node = item.data(0, Qt.ItemDataRole.UserRole)
        if node is not None:
            self.node_selected.emit(node)

    def _on_item_double_clicked(self, item, column):
        node = item.data(0, Qt.ItemDataRole.UserRole)
        if node is not None:
            new_name, ok = QInputDialog.getText(self, "Rename Node", "Name:", text=node.name)
            if ok and new_name:
                node.name = new_name
                item.setText(0, f"{'[M]' if node.mesh else '[N]'} {new_name}")
                self.node_renamed.emit(node, new_name)

    def _show_context_menu(self, pos):
        item = self.itemAt(pos)
        if item is None:
            return
        node = item.data(0, Qt.ItemDataRole.UserRole)
        if node is None:
            return

        menu = QMenu(self)

        rename_action = QAction("Rename", self)
        rename_action.triggered.connect(lambda: self._rename_node(item, node))
        menu.addAction(rename_action)

        visible_action = QAction("Toggle Visibility", self)
        visible_action.triggered.connect(lambda: self._toggle_visibility(item, node))
        menu.addAction(visible_action)

        delete_action = QAction("Delete", self)
        delete_action.triggered.connect(lambda: self._delete_node(item, node))
        menu.addAction(delete_action)

        duplicate_action = QAction("Duplicate", self)
        duplicate_action.triggered.connect(lambda: self._duplicate_node(node))
        menu.addAction(duplicate_action)

        menu.addSeparator()

        focus_action = QAction("Focus", self)
        focus_action.triggered.connect(lambda: self.node_selected.emit(node))
        menu.addAction(focus_action)

        menu.exec(self.viewport().mapToGlobal(pos))

    def _rename_node(self, item, node):
        new_name, ok = QInputDialog.getText(self, "Rename", "Name:", text=node.name)
        if ok and new_name:
            node.name = new_name
            item.setText(0, f"{'[M]' if node.mesh else '[N]'} {new_name}")

    def _toggle_visibility(self, item, node):
        node.visible = not node.visible
        self.node_visibility_changed.emit(node, node.visible)
        self.refresh()

    def _delete_node(self, item, node):
        reply = QMessageBox.question(
            self, "Delete Node",
            f"Delete '{node.name}'?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if reply == QMessageBox.StandardButton.Yes:
            self.node_deleted.emit(node)

    def _duplicate_node(self, node):
        if self.scene:
            dup = node.clone()
            if node.parent:
                node.parent.add_child(dup)
            else:
                self.scene.root.add_child(dup)
            self.refresh()
            self.node_selected.emit(dup)

    def highlight_node(self, node):
        self.clearSelection()
        if node in self._node_items:
            item = self._node_items[id(node)]
            self.setCurrentItem(item)
