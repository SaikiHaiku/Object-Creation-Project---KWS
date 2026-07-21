from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTextEdit, QPushButton,
    QLabel, QProgressBar, QGroupBox, QComboBox, QSpinBox,
    QCheckBox, QSlider, QFrame, QLineEdit
)
from PyQt6.QtCore import Qt, pyqtSignal, QThread, QTimer
from PyQt6.QtGui import QFont, QTextCursor
import time


class AIPromptPanel(QWidget):
    generation_requested = pyqtSignal(str)
    generation_complete = pyqtSignal(object)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumHeight(200)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(5, 5, 5, 5)

        header = QLabel("AI Object Generator")
        header.setFont(QFont("Arial", 12, QFont.Weight.Bold))
        layout.addWidget(header)

        desc_label = QLabel("Describe what you want to create:")
        layout.addWidget(desc_label)

        self.prompt_input = QTextEdit()
        self.prompt_input.setPlaceholderText(
            "Examples:\n"
            "- A red metallic cube\n"
            "- A tall tree with green leaves\n"
            "- A medieval castle with 4 towers\n"
            "- A futuristic robot with glowing blue eyes\n"
            "- 5 small golden spheres arranged in a circle\n"
            "- A sword with a gold guard and silver blade\n"
            "- A snowman with a carrot nose\n"
            "- A DNA double helix strand\n"
            "- A neon spiral of 30 colored balls\n"
            "- A house with brown roof and windows\n"
        )
        self.prompt_input.setMaximumHeight(120)
        layout.addWidget(self.prompt_input)

        options_row = QHBoxLayout()

        options_row.addWidget(QLabel("Quality:"))
        self.quality_combo = QComboBox()
        self.quality_combo.addItems(["Low", "Medium", "High", "Ultra"])
        self.quality_combo.setCurrentIndex(2)
        options_row.addWidget(self.quality_combo)

        options_row.addWidget(QLabel("Style:"))
        self.style_combo = QComboBox()
        self.style_combo.addItems(["Realistic", "Cartoon", "Low Poly", "Sci-Fi", "Fantasy"])
        options_row.addWidget(self.style_combo)

        layout.addLayout(options_row)

        btn_row = QHBoxLayout()
        self.generate_btn = QPushButton("GENERATE")
        self.generate_btn.setMinimumHeight(40)
        self.generate_btn.setStyleSheet(
            "QPushButton { background-color: #2196F3; color: white; font-weight: bold; font-size: 14px; "
            "border-radius: 5px; } QPushButton:hover { background-color: #1976D2; }"
        )
        self.generate_btn.clicked.connect(self._on_generate)
        btn_row.addWidget(self.generate_btn)

        self.clear_btn = QPushButton("Clear Scene")
        self.clear_btn.setMinimumHeight(40)
        self.clear_btn.clicked.connect(self._on_clear)
        btn_row.addWidget(self.clear_btn)

        layout.addLayout(btn_row)

        self.progress_bar = QProgressBar()
        self.progress_bar.setVisible(False)
        layout.addWidget(self.progress_bar)

        self.status_label = QLabel("Ready")
        layout.addWidget(self.status_label)

        examples_group = QGroupBox("Quick Examples")
        examples_layout = QVBoxLayout()
        examples = [
            ("House", "A colorful house with roof, door, and windows"),
            ("Robot", "A silver robot with glowing blue eyes"),
            ("Castle", "A medieval stone castle with towers"),
            ("Spaceship", "A futuristic spaceship with engines"),
            ("Tree", "A tall tree with green leaves"),
            ("Sword", "A magical sword with gold details"),
            ("Snowman", "A snowman with carrot nose and buttons"),
            ("DNA", "A DNA double helix strand"),
        ]
        for name, prompt in examples:
            btn = QPushButton(name)
            btn.setMaximumHeight(28)
            btn.clicked.connect(lambda _, p=prompt: self.prompt_input.setPlainText(p))
            examples_layout.addWidget(btn)
        examples_group.setLayout(examples_layout)
        layout.addWidget(examples_group)

        layout.addStretch()

    def _on_generate(self):
        prompt = self.prompt_input.toPlainText().strip()
        if not prompt:
            self.status_label.setText("Please enter a prompt!")
            return
        self.generate_btn.setEnabled(False)
        self.progress_bar.setVisible(True)
        self.progress_bar.setRange(0, 0)
        self.status_label.setText("Generating...")
        self.generation_requested.emit(prompt)

    def on_generation_done(self, result):
        self.generate_btn.setEnabled(True)
        self.progress_bar.setVisible(False)
        self.status_label.setText(f"Generated successfully! {result}")

    def on_generation_error(self, error):
        self.generate_btn.setEnabled(True)
        self.progress_bar.setVisible(False)
        self.status_label.setText(f"Error: {error}")

    def _on_clear(self):
        self.generation_requested.emit("__CLEAR__")
        self.status_label.setText("Scene cleared")
