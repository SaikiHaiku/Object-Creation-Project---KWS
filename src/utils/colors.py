import numpy as np

COLORS = {
    'white':      np.array([1.0, 1.0, 1.0, 1.0], dtype=np.float32),
    'black':      np.array([0.0, 0.0, 0.0, 1.0], dtype=np.float32),
    'red':        np.array([1.0, 0.0, 0.0, 1.0], dtype=np.float32),
    'green':      np.array([0.0, 1.0, 0.0, 1.0], dtype=np.float32),
    'blue':       np.array([0.0, 0.0, 1.0, 1.0], dtype=np.float32),
    'yellow':     np.array([1.0, 1.0, 0.0, 1.0], dtype=np.float32),
    'cyan':       np.array([0.0, 1.0, 1.0, 1.0], dtype=np.float32),
    'magenta':    np.array([1.0, 0.0, 1.0, 1.0], dtype=np.float32),
    'orange':     np.array([1.0, 0.65, 0.0, 1.0], dtype=np.float32),
    'purple':     np.array([0.5, 0.0, 0.5, 1.0], dtype=np.float32),
    'gray':       np.array([0.5, 0.5, 0.5, 1.0], dtype=np.float32),
    'dark_gray':  np.array([0.25, 0.25, 0.25, 1.0], dtype=np.float32),
    'light_gray': np.array([0.75, 0.75, 0.75, 1.0], dtype=np.float32),
    'brown':      np.array([0.6, 0.4, 0.2, 1.0], dtype=np.float32),
    'pink':       np.array([1.0, 0.4, 0.7, 1.0], dtype=np.float32),
    'sky_blue':   np.array([0.4, 0.6, 0.9, 1.0], dtype=np.float32),
    'gold':       np.array([1.0, 0.84, 0.0, 1.0], dtype=np.float32),
    'silver':     np.array([0.75, 0.75, 0.75, 1.0], dtype=np.float32),
}

PALETTES = {
    'default': ['white', 'red', 'green', 'blue', 'yellow', 'cyan', 'orange', 'purple'],
    'pastel': [
        np.array([1.0, 0.8, 0.8, 1.0]),
        np.array([0.8, 1.0, 0.8, 1.0]),
        np.array([0.8, 0.8, 1.0, 1.0]),
        np.array([1.0, 1.0, 0.8, 1.0]),
        np.array([0.8, 1.0, 1.0, 1.0]),
        np.array([1.0, 0.8, 1.0, 1.0]),
    ],
    'neon': [
        np.array([1.0, 0.0, 0.5, 1.0]),
        np.array([0.0, 1.0, 0.5, 1.0]),
        np.array([0.0, 0.5, 1.0, 1.0]),
        np.array([1.0, 1.0, 0.0, 1.0]),
        np.array([0.0, 1.0, 1.0, 1.0]),
        np.array([1.0, 0.0, 1.0, 1.0]),
    ],
    'earth': [
        np.array([0.4, 0.3, 0.2, 1.0]),
        np.array([0.2, 0.5, 0.2, 1.0]),
        np.array([0.3, 0.5, 0.7, 1.0]),
        np.array([0.8, 0.7, 0.5, 1.0]),
        np.array([0.5, 0.3, 0.1, 1.0]),
        np.array([0.6, 0.6, 0.6, 1.0]),
    ],
}

def hex_to_rgb(hex_str):
    hex_str = hex_str.lstrip('#')
    return np.array([int(hex_str[i:i+2], 16) / 255.0 for i in (0, 2, 4)] + [1.0], dtype=np.float32)

def rgb_to_hex(color):
    r, g, b = (int(c * 255) for c in color[:3])
    return f"#{r:02x}{g:02x}{b:02x}"

def color_lerp(c1, c2, t):
    return c1 + (c2 - c1) * t
