import numpy as np
from enum import Enum, auto


class LightType(Enum):
    POINT = auto()
    DIRECTIONAL = auto()
    SPOT = auto()
    AMBIENT = auto()


class Light:
    def __init__(self, name="Light", light_type=LightType.POINT):
        self.name = name
        self.light_type = light_type
        self.position = np.array([0.0, 5.0, 5.0], dtype=np.float32)
        self.direction = np.array([0.0, -1.0, 0.0], dtype=np.float32)
        self.color = np.array([1.0, 1.0, 1.0, 1.0], dtype=np.float32)
        self.intensity = 1.0
        self.range = 50.0
        self.spot_angle = 45.0
        self.spot_exponent = 32.0
        self.cast_shadows = True
        self.enabled = True
        self.attenuation = np.array([1.0, 0.09, 0.032], dtype=np.float32)

    def get_direction_to(self, target):
        target = np.asarray(target, dtype=np.float32)
        if self.light_type == LightType.DIRECTIONAL:
            return -self.direction / (np.linalg.norm(self.direction) + 1e-8)
        d = target - self.position
        return d / (np.linalg.norm(d) + 1e-8)

    def get_light_data(self):
        return {
            'position': self.position,
            'direction': self.direction,
            'color': self.color[:3] * self.intensity,
            'type': int(self.light_type),
            'range': self.range,
            'spot_angle': self.spot_angle,
            'spot_exponent': self.spot_exponent,
            'attenuation': self.attenuation,
        }

    def clone(self):
        l = Light(self.name + "_copy", self.light_type)
        l.position = self.position.copy()
        l.direction = self.direction.copy()
        l.color = self.color.copy()
        l.intensity = self.intensity
        l.range = self.range
        l.spot_angle = self.spot_angle
        l.spot_exponent = self.spot_exponent
        l.cast_shadows = self.cast_shadows
        l.attenuation = self.attenuation.copy()
        return l
