import numpy as np
from enum import Enum, auto


class BlendMode(Enum):
    OPAQUE = auto()
    ALPHA_BLEND = auto()
    ADDITIVE = auto()


class Material:
    def __init__(self, name="Material"):
        self.name = name
        self.diffuse = np.array([0.8, 0.8, 0.8, 1.0], dtype=np.float32)
        self.specular = np.array([1.0, 1.0, 1.0, 1.0], dtype=np.float32)
        self.ambient = np.array([0.2, 0.2, 0.2, 1.0], dtype=np.float32)
        self.emission = np.array([0.0, 0.0, 0.0, 1.0], dtype=np.float32)
        self.shininess = 32.0
        self.metallic = 0.0
        self.roughness = 0.5
        self.opacity = 1.0
        self.blend_mode = BlendMode.OPAQUE
        self.wireframe = False
        self.backface_culling = True
        self.texture_diffuse = None
        self.texture_normal = None
        self.texture_specular = None

    def set_color(self, r, g, b, a=1.0):
        self.diffuse = np.array([r, g, b, a], dtype=np.float32)

    def set_from_hex(self, hex_color):
        from ..utils.colors import hex_to_rgb
        self.diffuse = hex_to_rgb(hex_color)

    def clone(self):
        m = Material(self.name + "_copy")
        m.diffuse = self.diffuse.copy()
        m.specular = self.specular.copy()
        m.ambient = self.ambient.copy()
        m.emission = self.emission.copy()
        m.shininess = self.shininess
        m.metallic = self.metallic
        m.roughness = self.roughness
        m.opacity = self.opacity
        m.blend_mode = self.blend_mode
        m.wireframe = self.wireframe
        m.backface_culling = self.backface_culling
        return m


class Texture:
    def __init__(self, name="Texture"):
        self.name = name
        self.image_data = None
        self.width = 0
        self.height = 0
        self._gl_texture = None
        self._dirty = True

    def load_from_file(self, filepath):
        from PIL import Image
        img = Image.open(filepath).convert("RGBA")
        self.width, self.height = img.size
        self.image_data = np.array(img, dtype=np.uint8)
        self._dirty = True

    def generate_checkerboard(self, size=64, tiles=8):
        self.width = size
        self.height = size
        self.image_data = np.zeros((size, size, 4), dtype=np.uint8)
        tile_size = size // tiles
        for y in range(tiles):
            for x in range(tiles):
                c = 200 if (x + y) % 2 == 0 else 100
                y0, y1 = y * tile_size, (y + 1) * tile_size
                x0, x1 = x * tile_size, (x + 1) * tile_size
                self.image_data[y0:y1, x0:x1] = [c, c, c, 255]
        self._dirty = True

    def generate_gradient(self, size=256, color1=None, color2=None):
        if color1 is None:
            color1 = [255, 255, 255, 255]
        if color2 is None:
            color2 = [0, 0, 0, 255]
        self.width = size
        self.height = size
        self.image_data = np.zeros((size, size, 4), dtype=np.uint8)
        for y in range(size):
            t = y / max(size - 1, 1)
            r = int(color1[0] * (1 - t) + color2[0] * t)
            g = int(color1[1] * (1 - t) + color2[1] * t)
            b = int(color1[2] * (1 - t) + color2[2] * t)
            a = int(color1[3] * (1 - t) + color2[3] * t)
            self.image_data[y, :] = [r, g, b, a]
        self._dirty = True

    def generate_noise(self, size=256, scale=10.0):
        from ..utils.math_utils import fbm
        self.width = size
        self.height = size
        self.image_data = np.zeros((size, size, 4), dtype=np.uint8)
        for y in range(size):
            for x in range(size):
                n = fbm(x / scale, y / scale)
                c = int((n + 1) * 0.5 * 255)
                c = max(0, min(255, c))
                self.image_data[y, x] = [c, c, c, 255]
        self._dirty = True
