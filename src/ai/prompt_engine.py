import re
import numpy as np
import math
from ..core.primitives import (
    create_cube, create_sphere, create_cylinder, create_cone,
    create_torus, create_plane, create_torus_knot, create_ico_sphere,
    create_arrow
)
from ..core.material import Material
from ..core.scene import SceneNode
from ..utils.colors import COLORS, hex_to_rgb


SHAPE_KEYWORDS = {
    'cube': ['cube', 'block', 'box', 'rectangular', 'boite', 'bloc'],
    'sphere': ['sphere', 'ball', 'round', 'circle', 'boule', 'rond', 'orb'],
    'cylinder': ['cylinder', 'tube', 'pipe', 'column', 'cylindre', 'tube', 'colonne'],
    'cone': ['cone', 'pyramid', 'pointed', 'cone', 'pyramide'],
    'torus': ['torus', 'donut', 'ring', 'doughnut', 'anneau', 'beignet'],
    'plane': ['plane', 'flat', 'surface', 'ground', 'plate', 'plan', 'surface'],
    'torus_knot': ['knot', 'twisted', 'complex', 'noeud', 'tordu'],
    'ico_sphere': ['geodesic', 'low_poly', 'faceted', 'gem', ' cristal'],
    'arrow': ['arrow', 'pointer', 'direction', 'fleche'],
}

SIZE_KEYWORDS = {
    'tiny': 0.25, 'small': 0.5, 'medium': 1.0, 'large': 1.5, 'big': 2.0, 'huge': 3.0, 'massive': 5.0,
    'petit': 0.5, 'moyen': 1.0, 'grand': 2.0, 'enorme': 3.0, 'gros': 2.0, 'minuscule': 0.25,
}

COLOR_KEYWORDS = {}
for name, color in COLORS.items():
    COLOR_KEYWORDS[name] = color
COLOR_KEYWORDS['rainbow'] = None
COLOR_KEYWORDS['arc'] = None
COLOR_KEYWORDS['metal'] = np.array([0.7, 0.7, 0.8, 1.0], dtype=np.float32)
COLOR_KEYWORDS['gold'] = np.array([1.0, 0.84, 0.0, 1.0], dtype=np.float32)
COLOR_KEYWORDS['silver'] = np.array([0.75, 0.75, 0.75, 1.0], dtype=np.float32)
COLOR_KEYWORDS['copper'] = np.array([0.72, 0.45, 0.20, 1.0], dtype=np.float32)
COLOR_KEYWORDS['bronze'] = np.array([0.8, 0.5, 0.2, 1.0], dtype=np.float32)
COLOR_KEYWORDS['wood'] = np.array([0.6, 0.4, 0.2, 1.0], dtype=np.float32)
COLOR_KEYWORDS['stone'] = np.array([0.5, 0.5, 0.5, 1.0], dtype=np.float32)
COLOR_KEYWORDS['grass'] = np.array([0.2, 0.6, 0.2, 1.0], dtype=np.float32)
COLOR_KEYWORDS['water'] = np.array([0.1, 0.3, 0.8, 0.8], dtype=np.float32)
COLOR_KEYWORDS['fire'] = np.array([1.0, 0.3, 0.0, 1.0], dtype=np.float32)
COLOR_KEYWORDS['ice'] = np.array([0.7, 0.9, 1.0, 0.9], dtype=np.float32)
COLOR_KEYWORDS['neon'] = np.array([0.0, 1.0, 0.5, 1.0], dtype=np.float32)
COLOR_KEYWORDS['dark'] = np.array([0.1, 0.1, 0.1, 1.0], dtype=np.float32)
COLOR_KEYWORDS['light'] = np.array([0.9, 0.9, 0.9, 1.0], dtype=np.float32)

SCENE_KEYWORDS = {
    'house': ['house', 'home', 'building', 'maison', 'batiment'],
    'tree': ['tree', 'plant', 'arbre', 'plante'],
    'car': ['car', 'vehicle', 'auto', 'voiture', 'vehicule'],
    'robot': ['robot', 'android', 'mech', 'robot'],
    'castle': ['castle', 'fortress', 'chateau', 'forteresse'],
    'spaceship': ['spaceship', 'ship', 'rocket', 'vaisseau', 'fusee'],
    'sword': ['sword', 'blade', 'weapon', 'epee', 'lame'],
    'chair': ['chair', 'seat', 'chaise', 'siege'],
    'table': ['table', 'desk', 'table', 'bureau'],
    'skull': ['skull', 'head', 'crane', 'tete'],
    'star': ['star', 'etoile'],
    'heart': ['heart', 'coeur'],
    'mountain': ['mountain', 'hill', 'montagne', 'colline'],
    'city': ['city', 'town', 'building', 'ville', 'immeuble'],
    'sun': ['sun', 'soleil'],
    'moon': ['moon', 'lune'],
    'snowman': ['snowman', 'bonhomme de neige'],
    'diamond': ['diamond', 'gem', 'diamant', 'bijou'],
    'cross': ['cross', 'plus', 'croix'],
    'spiral': ['spiral', 'vortex', 'tourbillon'],
    'dna': ['dna', 'helix', 'helice'],
}


class PromptEngine:
    def __init__(self):
        self.last_parsed = None

    def parse_prompt(self, prompt):
        prompt_lower = prompt.lower().strip()
        parsed = {
            'original': prompt,
            'shapes': [],
            'scene_type': None,
            'size': 1.0,
            'color': None,
            'material_type': 'default',
            'count': 1,
            'arrangement': None,
            'modifiers': [],
        }

        for scene_type, keywords in SCENE_KEYWORDS.items():
            for kw in keywords:
                if kw in prompt_lower:
                    parsed['scene_type'] = scene_type
                    break
            if parsed['scene_type']:
                break

        if not parsed['scene_type']:
            for shape_name, keywords in SHAPE_KEYWORDS.items():
                for kw in keywords:
                    if kw in prompt_lower:
                        parsed['shapes'].append(shape_name)
                        break

        for keyword, size in SIZE_KEYWORDS.items():
            if keyword in prompt_lower:
                parsed['size'] = size
                break

        for keyword, color in COLOR_KEYWORDS.items():
            if keyword in prompt_lower:
                parsed['color'] = color
                break

        if 'metallic' in prompt_lower or 'metal' in prompt_lower:
            parsed['material_type'] = 'metallic'
        elif 'glossy' in prompt_lower or 'shiny' in prompt_lower or 'brillant' in prompt_lower:
            parsed['material_type'] = 'glossy'
        elif 'matte' in prompt_lower or 'flat' in prompt_lower:
            parsed['material_type'] = 'matte'
        elif 'glass' in prompt_lower or 'transparent' in prompt_lower or 'verre' in prompt_lower:
            parsed['material_type'] = 'glass'
        elif 'glow' in prompt_lower or 'emissive' in prompt_lower or 'lumineux' in prompt_lower:
            parsed['material_type'] = 'emissive'
        elif 'wireframe' in prompt_lower or 'fil' in prompt_lower:
            parsed['material_type'] = 'wireframe'

        count_match = re.search(r'(\d+)\s*(x|times|fois|copies)', prompt_lower)
        if count_match:
            parsed['count'] = min(int(count_match.group(1)), 100)

        for mod in ['smooth', 'subdivide', 'mirror', 'array', 'spiral', 'grid', 'circle']:
            if mod in prompt_lower:
                parsed['modifiers'].append(mod)

        if 'arrange' in prompt_lower or 'array' in prompt_lower or 'grid' in prompt_lower:
            parsed['arrangement'] = 'grid'
        elif 'circle' in prompt_lower or 'ring' in prompt_lower or 'round' in prompt_lower:
            parsed['arrangement'] = 'circle'
        elif 'line' in prompt_lower or 'row' in prompt_lower or 'ligne' in prompt_lower:
            parsed['arrangement'] = 'line'
        elif 'spiral' in prompt_lower:
            parsed['arrangement'] = 'spiral'

        if not parsed['shapes'] and not parsed['scene_type']:
            parsed['shapes'] = ['cube']

        self.last_parsed = parsed
        return parsed

    def get_description(self, parsed):
        parts = []
        if parsed['scene_type']:
            parts.append(f"Scene: {parsed['scene_type']}")
        if parsed['shapes']:
            parts.append(f"Shapes: {', '.join(parsed['shapes'])}")
        parts.append(f"Size: {parsed['size']}")
        if parsed['color'] is not None:
            parts.append("Color: custom")
        parts.append(f"Material: {parsed['material_type']}")
        if parsed['count'] > 1:
            parts.append(f"Count: {parsed['count']}")
        if parsed['arrangement']:
            parts.append(f"Arrangement: {parsed['arrangement']}")
        return " | ".join(parts)
