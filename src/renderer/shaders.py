from OpenGL.GL import *
from OpenGL.GL import shaders
import numpy as np


VERTEX_SHADER_3D = """
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec4 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
out vec4 vColor;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = normalize(uNormalMatrix * aNormal);
    vTexCoord = aTexCoord;
    vColor = aColor;
    gl_Position = uProjection * uView * worldPos;
}
"""

FRAGMENT_SHADER_3D = """
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec4 vColor;

uniform vec3 uCameraPos;
uniform vec4 uDiffuse;
uniform vec3 uSpecular;
uniform float uShininess;
uniform float uMetallic;
uniform float uRoughness;
uniform int uLightCount;

struct Light {
    vec3 position;
    vec3 direction;
    vec3 color;
    int type;
    float range;
    float spotAngle;
    float spotExponent;
    vec3 attenuation;
};

uniform Light uLights[8];

out vec4 FragColor;

vec3 computeLight(Light light, vec3 normal, vec3 viewDir) {
    vec3 lightDir;
    float attenuation = 1.0;

    if (light.type == 0) {
        lightDir = normalize(light.position - vWorldPos);
        float dist = length(light.position - vWorldPos);
        attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
    } else if (light.type == 1) {
        lightDir = normalize(-light.direction);
    } else if (light.type == 2) {
        lightDir = normalize(light.position - vWorldPos);
        float dist = length(light.position - vWorldPos);
        attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
        float spotCos = dot(normalize(-lightDir), normalize(light.direction));
        float spotCutoff = cos(radians(light.spotAngle));
        if (spotCos < spotCutoff) attenuation = 0.0;
        else attenuation *= pow(spotCos, light.spotExponent);
    }

    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.color * NdotL * uDiffuse.rgb;

    vec3 halfDir = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfDir), 0.0);
    float spec = pow(NdotH, uShininess);
    vec3 specular = light.color * spec * uSpecular * (1.0 - uRoughness);

    vec3 metallicDiffuse = diffuse * (1.0 - uMetallic);
    vec3 metallicSpecular = mix(specular, light.color * spec, uMetallic);

    return (metallicDiffuse + metallicSpecular) * attenuation;
}

void main() {
    vec3 normal = normalize(vNormal);
    vec3 viewDir = normalize(uCameraPos - vWorldPos);

    vec3 ambient = vec3(0.1) * uDiffuse.rgb;
    vec3 result = ambient;

    for (int i = 0; i < uLightCount && i < 8; i++) {
        result += computeLight(uLights[i], normal, viewDir);
    }

    result += uDiffuse.rgb * 0.02;

    FragColor = vec4(result, uDiffuse.a * vColor.a);
}
"""

VERTEX_SHADER_LINE = """
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 3) in vec4 aColor;

uniform mat4 uMVP;

out vec4 vColor;

void main() {
    vColor = aColor;
    gl_Position = uMVP * vec4(aPosition, 1.0);
}
"""

FRAGMENT_SHADER_LINE = """
#version 330 core
in vec4 vColor;
out vec4 FragColor;

void main() {
    FragColor = vColor;
}
"""

VERTEX_SHADER_GRID = """
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 3) in vec4 aColor;

uniform mat4 uView;
uniform mat4 uProjection;
uniform float uGridSize;

out vec4 vColor;
out vec3 vWorldPos;

void main() {
    vColor = aColor;
    vWorldPos = aPosition;
    gl_Position = uProjection * uView * vec4(aPosition, 1.0);
}
"""

FRAGMENT_SHADER_GRID = """
#version 330 core
in vec4 vColor;
in vec3 vWorldPos;
out vec4 FragColor;

void main() {
    float halfSize = uGridSize * 0.5;
    vec2 coord = vWorldPos.xz;
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
    float line = min(grid.x, grid.y);
    float alpha = 1.0 - min(line, 1.0);
    float dist = max(abs(coord.x), abs(coord.y)) / halfSize;
    alpha *= max(0.0, 1.0 - dist);
    if (alpha < 0.01) discard;
    vec3 color = vec3(0.35);
    if (abs(coord.x) < 0.05 || abs(coord.y) < 0.05) color = vec3(0.5, 0.5, 0.7);
    FragColor = vec4(color, alpha * 0.6);
}
"""


class ShaderProgram:
    def __init__(self):
        self.program = None
        self.uniforms = {}
        self._attribs = {}

    def compile(self, vertex_src, fragment_src):
        vs = shaders.compileShader(vertex_src, GL_VERTEX_SHADER)
        fs = shaders.compileShader(fragment_src, GL_FRAGMENT_SHADER)
        self.program = shaders.compileProgram(vs, fs)
        self._cache_uniforms()

    def _cache_uniforms(self):
        num_uniforms = glGetProgramiv(self.program, GL_ACTIVE_UNIFORMS)
        for i in range(num_uniforms):
            name, size, type_ = glGetActiveUniform(self.program, i)
            if isinstance(name, bytes):
                name = name.decode('utf-8')
            self.uniforms[name] = glGetUniformLocation(self.program, name)

    def use(self):
        glUseProgram(self.program)

    def set_mat4(self, name, matrix):
        loc = self.uniforms.get(name)
        if loc is not None and loc != -1:
            glUniformMatrix4fv(loc, 1, GL_FALSE, np.asarray(matrix, dtype=np.float32))

    def set_mat3(self, name, matrix):
        loc = self.uniforms.get(name)
        if loc is not None and loc != -1:
            glUniformMatrix3fv(loc, 1, GL_FALSE, np.asarray(matrix, dtype=np.float32))

    def set_vec3(self, name, vec):
        loc = self.uniforms.get(name)
        if loc is not None and loc != -1:
            glUniform3fv(loc, 1, np.asarray(vec, dtype=np.float32))

    def set_vec4(self, name, vec):
        loc = self.uniforms.get(name)
        if loc is not None and loc != -1:
            glUniform4fv(loc, 1, np.asarray(vec, dtype=np.float32))

    def set_float(self, name, value):
        loc = self.uniforms.get(name)
        if loc is not None and loc != -1:
            glUniform1f(loc, float(value))

    def set_int(self, name, value):
        loc = self.uniforms.get(name)
        if loc is not None and loc != -1:
            glUniform1i(loc, int(value))

    def destroy(self):
        if self.program:
            glDeleteProgram(self.program)


class ShaderManager:
    def __init__(self):
        self.shaders = {}
        self._default_3d = None
        self._default_line = None
        self._default_grid = None

    def initialize(self):
        self._default_3d = ShaderProgram()
        self._default_3d.compile(VERTEX_SHADER_3D, FRAGMENT_SHADER_3D)
        self._default_line = ShaderProgram()
        self._default_line.compile(VERTEX_SHADER_LINE, FRAGMENT_SHADER_LINE)
        self._default_grid = ShaderProgram()
        self._default_grid.compile(VERTEX_SHADER_GRID, FRAGMENT_SHADER_GRID)

    def get_3d_shader(self):
        return self._default_3d

    def get_line_shader(self):
        return self._default_line

    def get_grid_shader(self):
        return self._default_grid

    def destroy(self):
        for s in self.shaders.values():
            s.destroy()
        if self._default_3d:
            self._default_3d.destroy()
        if self._default_line:
            self._default_line.destroy()
        if self._default_grid:
            self._default_grid.destroy()
