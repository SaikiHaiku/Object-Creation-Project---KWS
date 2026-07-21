import numpy as np
import math

def vec3(x=0.0, y=0.0, z=0.0):
    return np.array([x, y, z], dtype=np.float32)

def vec4(x=0.0, y=0.0, z=0.0, w=1.0):
    return np.array([x, y, z, w], dtype=np.float32)

def mat4_identity():
    return np.eye(4, dtype=np.float32)

def mat4_translate(tx, ty, tz):
    m = mat4_identity()
    m[0, 3] = tx
    m[1, 3] = ty
    m[2, 3] = tz
    return m

def mat4_scale(sx, sy, sz):
    m = mat4_identity()
    m[0, 0] = sx
    m[1, 1] = sy
    m[2, 2] = sz
    return m

def mat4_rotate_x(angle):
    c, s = math.cos(angle), math.sin(angle)
    m = mat4_identity()
    m[1, 1] = c;  m[1, 2] = -s
    m[2, 1] = s;  m[2, 2] = c
    return m

def mat4_rotate_y(angle):
    c, s = math.cos(angle), math.sin(angle)
    m = mat4_identity()
    m[0, 0] = c;  m[0, 2] = s
    m[2, 0] = -s; m[2, 2] = c
    return m

def mat4_rotate_z(angle):
    c, s = math.cos(angle), math.sin(angle)
    m = mat4_identity()
    m[0, 0] = c;  m[0, 1] = -s
    m[1, 0] = s;  m[1, 1] = c
    return m

def mat4_rotate_euler(rx, ry, rz):
    return mat4_rotate_z(rz) @ mat4_rotate_y(ry) @ mat4_rotate_x(rx)

def mat4_perspective(fov, aspect, near, far):
    f = 1.0 / math.tan(math.radians(fov) / 2.0)
    m = mat4_identity()
    m[0, 0] = f / aspect
    m[1, 1] = f
    m[2, 2] = (far + near) / (near - far)
    m[2, 3] = (2 * far * near) / (near - far)
    m[3, 2] = -1.0
    m[3, 3] = 0.0
    return m

def mat4_ortho(left, right, bottom, top, near, far):
    m = mat4_identity()
    m[0, 0] = 2.0 / (right - left)
    m[1, 1] = 2.0 / (top - bottom)
    m[2, 2] = -2.0 / (far - near)
    m[0, 3] = -(right + left) / (right - left)
    m[1, 3] = -(top + bottom) / (top - bottom)
    m[2, 3] = -(far + near) / (far - near)
    return m

def mat4_look_at(eye, target, up):
    eye = np.asarray(eye, dtype=np.float32)
    target = np.asarray(target, dtype=np.float32)
    up = np.asarray(up, dtype=np.float32)
    f = target - eye
    f = f / (np.linalg.norm(f) + 1e-8)
    r = np.cross(f, up)
    r = r / (np.linalg.norm(r) + 1e-8)
    u = np.cross(r, f)
    m = mat4_identity()
    m[0, :3] = r
    m[1, :3] = u
    m[2, :3] = -f
    m[0, 3] = -np.dot(r, eye)
    m[1, 3] = -np.dot(u, eye)
    m[2, 3] = np.dot(f, eye)
    return m

def mat4_inverse(m):
    return np.linalg.inv(m).astype(np.float32)

def mat4_transpose(m):
    return np.transpose(m).astype(np.float32)

def mat3_normal_from_mat4(m):
    inv = np.linalg.inv(m[:3, :3].astype(np.float64))
    return np.transpose(inv).astype(np.float32)

def lerp(a, b, t):
    return a + (b - a) * t

def clamp(v, lo, hi):
    return max(lo, min(hi, v))

def smoothstep(edge0, edge1, x):
    t = clamp((x - edge0) / (edge1 - edge0 + 1e-8), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)

def noise2d(x, y):
    n = int(x * 57 + y * 131) & 0xFFFFFFFF
    n = (n << 13) ^ n
    return (1.0 - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7FFFFFFF) / 1073741824.0)

def fbm(x, y, octaves=6):
    val = 0.0
    amp = 0.5
    freq = 1.0
    for _ in range(octaves):
        val += amp * noise2d(x * freq, y * freq)
        amp *= 0.5
        freq *= 2.0
    return val
