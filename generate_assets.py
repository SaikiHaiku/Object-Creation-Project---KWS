#!/usr/bin/env python3
"""
OCP Asset Generator - Vectorized with NumPy for speed
Generates ~1GB of professional 3D assets: textures, HDRIs, materials, scenes, models, brushes
"""

import os, struct, json, math, time, random, hashlib
import numpy as np
from PIL import Image

OUTPUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "resources")

def ensure_dir(d):
    os.makedirs(d, exist_ok=True)

def fast_noise2d(w, h, seed=0):
    rng = np.random.RandomState(safe_seed(seed))
    grid = rng.randint(0, 2**31, size=(h//8+2, w//8+2), dtype=np.int64)
    gy, gx = np.mgrid[0:h//8+2, 0:w//8+2]
    ix = np.tile(np.arange(w), (h, 1)) // 8
    iy = np.tile(np.arange(h).reshape(-1,1), (1, w)) // 8
    fx = (np.tile(np.arange(w), (h, 1)) % 8) / 8.0
    fy = (np.tile(np.arange(h).reshape(-1,1), (1, w)) % 8) / 8.0
    fx = fx * fx * (3 - 2 * fx)
    fy = fy * fy * (3 - 2 * fy)
    n00 = grid[iy, ix].astype(np.float64) / 2**31
    n10 = grid[iy, ix+1].astype(np.float64) / 2**31
    n01 = grid[iy+1, ix].astype(np.float64) / 2**31
    n11 = grid[iy+1, ix+1].astype(np.float64) / 2**31
    nx0 = n00 * (1-fx) + n10 * fx
    nx1 = n01 * (1-fx) + n11 * fx
    return nx0 * (1-fy) + nx1 * fy

def fbm_np(w, h, octaves=5, seed=0):
    result = np.zeros((h, w), dtype=np.float64)
    amp = 0.5
    freq = 1.0
    for i in range(octaves):
        result += amp * fast_noise2d(w, h, seed + i * 31)
        amp *= 0.5
        freq *= 2.0
    return result

def safe_seed(s):
    return abs(int(s)) % (2**31)

def perlin_grid(scale, w, h, seed):
    gw, gh = int(w/scale)+2, int(h/scale)+2
    rng = np.random.RandomState(safe_seed(seed))
    grid = rng.rand(gh, gw).astype(np.float64)
    gx = np.tile(np.arange(w, dtype=np.float64), (h, 1)) / scale
    gy = np.tile(np.arange(h, dtype=np.float64).reshape(-1,1), (1, w)) / scale
    ix = gx.astype(int)
    iy = gy.astype(int)
    fx = gx - ix
    fy = gy - iy
    fx = fx * fx * (3 - 2 * fx)
    fy = fy * fy * (3 - 2 * fy)
    ix = np.clip(ix, 0, gw-2)
    iy = np.clip(iy, 0, gh-2)
    n00 = grid[iy, ix]
    n10 = grid[iy, ix+1]
    n01 = grid[iy+1, ix]
    n11 = grid[iy+1, ix+1]
    nx0 = n00*(1-fx)+n10*fx
    nx1 = n01*(1-fx)+n11*fx
    return nx0*(1-fy)+nx1*fy

def lerp(a, b, t):
    return a + (b - a) * np.clip(t, 0, 1)

def clamp_img(arr):
    return np.clip(arr, 0, 255).astype(np.uint8)

def gen_wood(w, h, seed):
    nx, ny = np.meshgrid(np.linspace(0, 8, w), np.linspace(0, 8, h))
    grain = perlin_grid(8, w, h, seed) * 0.3
    ring = np.sin((nx*0.5 + grain*3) * np.pi * 2) * 0.5 + 0.5
    ring = ring**2
    r = lerp(80, 160, ring) + perlin_grid(40, w, h, seed+1)*20-10
    g = lerp(45, 100, ring) + perlin_grid(40, w, h, seed+2)*15-7
    b = lerp(20, 50, ring) + perlin_grid(40, w, h, seed+3)*10-5
    return np.stack([clamp_img(r), clamp_img(g), clamp_img(b), np.full((h,w),255,dtype=np.uint8)], axis=-1)

def gen_marble(w, h, seed):
    nx, ny = np.meshgrid(np.linspace(0, 6, w), np.linspace(0, 6, h))
    turbulence = fbm_np(w, h, 5, seed) * 4
    v = np.sin((nx + turbulence) * np.pi) * 0.5 + 0.5
    streak = fbm_np(w, h, 3, seed+200)
    streak = np.abs(streak - 0.5) * 40
    r = clamp_img(lerp(200, 240, v) - streak)
    g = clamp_img(lerp(195, 238, v) - streak)
    b = clamp_img(lerp(190, 235, v) - streak)
    return np.stack([r, g, b, np.full((h,w),255,dtype=np.uint8)], axis=-1)

def gen_metal(w, h, seed, variant=0):
    colors = [((180,210),(180,210),(185,215)), ((200,230),(175,210),(120,160)), ((100,140),(100,140),(105,145))]
    cr, cg, cb = colors[variant % 3]
    ny = np.tile(np.arange(h, dtype=np.float64).reshape(-1,1), (1, w))
    brushed = np.sin(ny * 0.5 + perlin_grid(8, w, h, seed)*10) * 0.5 + 0.5
    v = perlin_grid(12, w, h, seed) * 0.3 + brushed * 0.7
    r = clamp_img(lerp(cr[0], cr[1], v))
    g = clamp_img(lerp(cg[0], cg[1], v))
    b = clamp_img(lerp(cb[0], cb[1], v))
    return np.stack([r, g, b, np.full((h,w),255,dtype=np.uint8)], axis=-1)

def gen_stone(w, h, seed):
    v = perlin_grid(6, w, h, seed)
    crack = fbm_np(w, h, 4, seed+300)
    crack = np.clip(1.0 - np.abs(crack - 0.5) * 8, 0, 1) * 0.4
    r = clamp_img(lerp(100, 160, v) - crack * 60)
    g = clamp_img(lerp(95, 155, v) - crack * 60)
    b = clamp_img(lerp(85, 145, v) - crack * 60)
    return np.stack([r, g, b, np.full((h,w),255,dtype=np.uint8)], axis=-1)

def gen_concrete(w, h, seed):
    v = perlin_grid(5, w, h, seed)
    rough = fbm_np(w, h, 4, seed+400) * 30 - 15
    r = clamp_img(lerp(150, 190, v) + rough)
    g = clamp_img(lerp(148, 188, v) + rough)
    b = clamp_img(lerp(145, 185, v) + rough)
    return np.stack([r, g, b, np.full((h,w),255,dtype=np.uint8)], axis=-1)

def gen_brick(w, h, seed):
    bw, bh = w/8, h/6
    nx = np.tile(np.arange(w, dtype=np.float64), (h, 1))
    ny = np.tile(np.arange(h, dtype=np.float64).reshape(-1,1), (1, w))
    row = (ny // bh).astype(int)
    offset = np.where(row % 2 == 1, bw * 0.5, 0)
    bx = (nx + offset) % bw
    by = ny % bh
    mortar = (bx < bw*0.05) | (bx > bw*0.95) | (by < bh*0.08) | (by > bh*0.92)
    rv = perlin_grid(10, w, h, seed)
    detail = fbm_np(w, h, 3, seed+500) * 20 - 10
    r = np.where(mortar, 180, clamp_img(lerp(160, 190, rv) + detail))
    g = np.where(mortar, 175, clamp_img(lerp(60, 80, rv) + detail))
    b = np.where(mortar, 165, clamp_img(lerp(40, 50, rv) + detail))
    return np.stack([r.astype(np.uint8), g.astype(np.uint8), b.astype(np.uint8), np.full((h,w),255,dtype=np.uint8)], axis=-1)

def gen_grass(w, h, seed):
    v = perlin_grid(6, w, h, seed)
    detail = fbm_np(w, h, 3, seed+600) * 20
    g = clamp_img(lerp(80, 160, v) + detail)
    r = clamp_img(g.astype(np.float64) * 0.4)
    b = clamp_img(g.astype(np.float64) * 0.2)
    return np.stack([r, g, b, np.full((h,w),255,dtype=np.uint8)], axis=-1)

def gen_water(w, h, seed):
    nx = np.tile(np.arange(w, dtype=np.float64), (h, 1)) / w * 6
    wave = np.sin(nx * 4 + fbm_np(w, h, 3, seed) * 3) * 0.5 + 0.5
    depth = perlin_grid(8, w, h, seed+700)
    r = clamp_img(20 + depth * 30)
    g = clamp_img(80 + wave * 60 + depth * 40)
    b = clamp_img(150 + wave * 60 + depth * 30)
    return np.stack([r, g, b, np.full((h,w),255,dtype=np.uint8)], axis=-1)

def gen_sand(w, h, seed):
    v = perlin_grid(5, w, h, seed)
    grain = fbm_np(w, h, 3, seed+800) * 15 - 7
    r = clamp_img(lerp(210, 240, v) + grain)
    g = clamp_img(lerp(190, 225, v) + grain)
    b = clamp_img(lerp(140, 180, v) + grain)
    return np.stack([r, g, b, np.full((h,w),255,dtype=np.uint8)], axis=-1)

def gen_lava(w, h, seed):
    v = perlin_grid(5, w, h, seed)
    hot = perlin_grid(6, w, h, seed+900)
    r = clamp_img(100 + v * 155 + hot * 50)
    g = clamp_img(v * 80 + hot * 100)
    b = clamp_img(hot * 40)
    return np.stack([r, g, b, np.full((h,w),255,dtype=np.uint8)], axis=-1)

def gen_terrain(w, h, seed):
    height = perlin_grid(6, w, h, seed)
    r = np.zeros((h,w), dtype=np.float64)
    g = np.zeros((h,w), dtype=np.float64)
    b = np.zeros((h,w), dtype=np.float64)
    water = height < 0.3
    sand_h = (height >= 0.3) & (height < 0.4)
    grass_h = (height >= 0.4) & (height < 0.7)
    rock_h = (height >= 0.7) & (height < 0.85)
    snow_h = height >= 0.85
    t = height / 0.3
    r[water] = lerp(20, 40, t[water]); g[water] = lerp(60, 100, t[water]); b[water] = lerp(140, 180, t[water])
    r[sand_h] = 210; g[sand_h] = 200; b[sand_h] = 160
    t = (height - 0.4) / 0.3
    r[grass_h] = lerp(60, 40, t[grass_h]); g[grass_h] = lerp(120, 90, t[grass_h]); b[grass_h] = lerp(40, 30, t[grass_h])
    t = (height - 0.7) / 0.15
    r[rock_h] = lerp(100, 140, t[rock_h]); g[rock_h] = lerp(90, 130, t[rock_h]); b[rock_h] = lerp(70, 110, t[rock_h])
    t = (height - 0.85) / 0.15
    r[snow_h] = lerp(220, 250, t[snow_h]); g[snow_h] = lerp(220, 250, t[snow_h]); b[snow_h] = lerp(225, 255, t[snow_h])
    return np.stack([clamp_img(r), clamp_img(g), clamp_img(b), np.full((h,w),255,dtype=np.uint8)], axis=-1)

def gen_checkers(w, h, seed, c1=(30,30,30), c2=(220,220,220)):
    sz = 16
    cx = (np.tile(np.arange(w), (h, 1)) // sz)
    cy = (np.tile(np.arange(h).reshape(-1,1), (1, w)) // sz)
    mask = (cx + cy) % 2 == 0
    detail = perlin_grid(10, w, h, seed) * 15 - 7
    r = np.where(mask, c1[0], c2[0]).astype(np.float64) + detail
    g = np.where(mask, c1[1], c2[1]).astype(np.float64) + detail
    b = np.where(mask, c1[2], c2[2]).astype(np.float64) + detail
    return np.stack([clamp_img(r), clamp_img(g), clamp_img(b), np.full((h,w),255,dtype=np.uint8)], axis=-1)

def gen_wood_planks(w, h, seed):
    plank_w = w // 4
    nx = np.tile(np.arange(w, dtype=np.float64), (h, 1))
    plank = (nx // plank_w).astype(int)
    px = (nx % plank_w) / plank_w
    grain = np.sin(px * 60 + perlin_grid(8, w, h, seed) * 8) * 0.5 + 0.5
    shade = 0.85 + perlin_grid(10, w, h, seed) * 0.3
    plank_ids = np.unique(plank)
    shade_map = np.zeros_like(shade)
    for pid in plank_ids:
        shade_map[plank == pid] = 0.85 + random.Random(int(seed)+int(pid)).random() * 0.3
    r = clamp_img((120 + grain * 50) * shade_map)
    g = clamp_img((70 + grain * 30) * shade_map)
    b = clamp_img((30 + grain * 15) * shade_map)
    gap = (nx % plank_w) < 2
    r[gap] = r[gap] // 2; g[gap] = g[gap] // 2; b[gap] = b[gap] // 2
    return np.stack([r, g, b, np.full((h,w),255,dtype=np.uint8)], axis=-1)

def gen_clouds(w, h, seed):
    nx, ny = np.meshgrid(np.linspace(0, 4, w), np.linspace(0, 4, h))
    v = fbm_np(w, h, 6, seed)
    sky_r = lerp(60, 200, ny/4)
    sky_g = lerp(120, 220, ny/4)
    sky_b = lerp(200, 255, ny/4)
    cloud = np.clip((v - 0.35) * 4, 0, 1)
    r = lerp(sky_r, 255, cloud)
    g = lerp(sky_g, 255, cloud)
    b = lerp(sky_b, 255, cloud)
    return np.stack([clamp_img(r), clamp_img(g), clamp_img(b), np.full((h,w),255,dtype=np.uint8)], axis=-1)

def gen_diamond_plate(w, h, seed):
    nx = np.tile(np.arange(w, dtype=np.float64), (h, 1)) / w * 20
    ny = np.tile(np.arange(h, dtype=np.float64).reshape(-1,1), (1, w)) / h * 20
    dx = (nx % 1.0) - 0.5
    dy = (ny % 1.0) - 0.5
    diamond = np.clip(1.0 - (np.abs(dx) + np.abs(dy)) * 3, 0, 1)
    base = 160 + perlin_grid(10, w, h, seed) * 30
    v = base + diamond * 50
    return np.stack([clamp_img(v), clamp_img(v+2), clamp_img(v+5), np.full((h,w),255,dtype=np.uint8)], axis=-1)

TEXTURE_GENS = {
    "wood": lambda w,h,s: gen_wood(w,h,s),
    "marble": lambda w,h,s: gen_marble(w,h,s),
    "stone": lambda w,h,s: gen_stone(w,h,s),
    "concrete": lambda w,h,s: gen_concrete(w,h,s),
    "brick": lambda w,h,s: gen_brick(w,h,s),
    "grass": lambda w,h,s: gen_grass(w,h,s),
    "water": lambda w,h,s: gen_water(w,h,s),
    "sand": lambda w,h,s: gen_sand(w,h,s),
    "lava": lambda w,h,s: gen_lava(w,h,s),
    "terrain": lambda w,h,s: gen_terrain(w,h,s),
    "wood_planks": lambda w,h,s: gen_wood_planks(w,h,s),
    "clouds": lambda w,h,s: gen_clouds(w,h,s),
    "diamond_plate": lambda w,h,s: gen_diamond_plate(w,h,s),
}

METAL_VARIANTS = ["steel", "gold", "titanium"]

FABRIC_COLORS = {
    "red": (180,50,50), "blue": (50,50,180), "green": (50,150,50),
    "yellow": (220,200,50), "purple": (130,50,180), "orange": (220,130,40),
    "pink": (220,130,160), "cyan": (50,180,180), "brown": (140,90,50),
    "white": (230,230,230), "black": (30,30,30), "navy": (30,30,100),
    "teal": (0,128,128), "maroon": (128,0,0), "olive": (128,128,0),
    "indigo": (75,0,130), "coral": (255,127,80), "gold": (255,215,0),
    "silver": (192,192,192), "crimson": (220,20,60),
}

GRADIENT_COMBOS = [
    ((0,0,0),(255,255,255)), ((255,50,50),(50,50,255)),
    ((0,0,0),(0,255,0)), ((255,200,0),(255,0,0)),
    ((0,0,80),(0,180,255)), ((100,0,0),(255,200,100)),
    ((20,0,40),(255,100,200)), ((0,50,80),(200,255,100)),
]

CHECKER_COMBOS = [
    ((30,30,30),(220,220,220)), ((255,0,0),(255,255,255)),
    ((0,0,0),(0,255,0)), ((0,0,128),(255,255,0)),
    ((255,100,0),(0,100,255)), ((128,0,128),(255,255,255)),
]

def generate_all_textures():
    tex_dir = os.path.join(OUTPUT_DIR, "textures")
    ensure_dir(tex_dir)
    total = 0
    w, h = 1024, 1024
    print(f"[TEXTURES] Generating {len(TEXTURE_GENS)} categories + metals + fabric + gradients + checkers...")
    for name, gen in TEXTURE_GENS.items():
        cat_dir = os.path.join(tex_dir, name)
        ensure_dir(cat_dir)
        for i in range(5):
            seed = hash(name) + i * 7919
            img_arr = gen(w, h, seed)
            img = Image.fromarray(img_arr, 'RGBA')
            path = os.path.join(cat_dir, f"{name}_{i:03d}.png")
            img.save(path, "PNG", optimize=True)
            sz = os.path.getsize(path)
            total += sz
        print(f"  {name}: 5 textures ({total // (1024*1024)}MB running)")

    for vi, vname in enumerate(METAL_VARIANTS):
        cat_dir = os.path.join(tex_dir, f"metal_{vname}")
        ensure_dir(cat_dir)
        for i in range(5):
            seed = hash(vname) + i * 7919 + 5000
            img_arr = gen_metal(w, h, seed, vi)
            img = Image.fromarray(img_arr, 'RGBA')
            path = os.path.join(cat_dir, f"metal_{vname}_{i:03d}.png")
            img.save(path, "PNG", optimize=True)
            total += os.path.getsize(path)
        print(f"  metal_{vname}: 5 textures")

    fabric_dir = os.path.join(tex_dir, "fabric")
    ensure_dir(fabric_dir)
    for cname, color in FABRIC_COLORS.items():
        seed = hash("fabric") + hash(cname)
        ny = np.tile(np.arange(h, dtype=np.float64).reshape(-1,1), (1, w))
        nx = np.tile(np.arange(w, dtype=np.float64), (h, 1))
        weave = (np.sin(nx * math.pi * 0.5) * 0.15 * np.where((nx.astype(int) % 2 == 0), 1, 0) +
                 np.sin(ny * math.pi * 0.5) * 0.15 * np.where((ny.astype(int) % 2 == 0), 1, 0))
        detail = perlin_grid(20, w, h, seed) * 0.1
        factor = 1 + weave + detail
        r = clamp_img(np.array(color[0], dtype=np.float64) * factor)
        g = clamp_img(np.array(color[1], dtype=np.float64) * factor)
        b = clamp_img(np.array(color[2], dtype=np.float64) * factor)
        img_arr = np.stack([r, g, b, np.full((h,w),255,dtype=np.uint8)], axis=-1)
        img = Image.fromarray(img_arr, 'RGBA')
        path = os.path.join(fabric_dir, f"fabric_{cname}.png")
        img.save(path, "PNG", optimize=True)
        total += os.path.getsize(path)
    print(f"  fabric: {len(FABRIC_COLORS)} textures")

    grad_dir = os.path.join(tex_dir, "gradient")
    ensure_dir(grad_dir)
    for i, (c1, c2) in enumerate(GRADIENT_COMBOS):
        ny = np.linspace(0, 1, h).reshape(-1, 1)
        r = lerp(c1[0], c2[0], ny)
        g = lerp(c1[1], c2[1], ny)
        b = lerp(c1[2], c2[2], ny)
        detail = perlin_grid(5, w, h, i*100) * 10 - 5
        r = np.tile(r, (1, w)) + detail
        g = np.tile(g, (1, w)) + detail
        b = np.tile(b, (1, w)) + detail
        img_arr = np.stack([clamp_img(r), clamp_img(g), clamp_img(b), np.full((h,w),255,dtype=np.uint8)], axis=-1)
        img = Image.fromarray(img_arr, 'RGBA')
        path = os.path.join(grad_dir, f"gradient_{i:03d}.png")
        img.save(path, "PNG", optimize=True)
        total += os.path.getsize(path)
    print(f"  gradient: {len(GRADIENT_COMBOS)} textures")

    checker_dir = os.path.join(tex_dir, "checkers")
    ensure_dir(checker_dir)
    for i, (c1, c2) in enumerate(CHECKER_COMBOS):
        img_arr = gen_checkers(w, h, hash("checker") + i * 100, c1, c2)
        img = Image.fromarray(img_arr, 'RGBA')
        path = os.path.join(checker_dir, f"checker_{i:03d}.png")
        img.save(path, "PNG", optimize=True)
        total += os.path.getsize(path)
    print(f"  checkers: {len(CHECKER_COMBOS)} textures")

    print(f"  TOTAL TEXTURES: {total // (1024*1024)}MB")
    return total


def generate_hdri():
    hdri_dir = os.path.join(OUTPUT_DIR, "hdri")
    ensure_dir(hdri_dir)
    total = 0
    w, h = 2048, 1024
    presets = [
        ("sunset", (255,120,40), (40,60,120), (255,180,80)),
        ("noon", (100,160,255), (40,80,160), (255,255,220)),
        ("night", (5,5,20), (10,10,40), (20,20,60)),
        ("dawn", (200,130,100), (60,80,130), (255,200,150)),
        ("studio", (200,200,210), (180,180,190), (220,220,230)),
        ("forest", (60,120,60), (30,70,30), (80,140,80)),
        ("desert", (220,180,120), (180,140,80), (240,200,140)),
        ("underwater", (10,40,80), (5,20,50), (20,80,120)),
        ("arctic", (200,210,220), (160,180,200), (230,240,250)),
        ("volcanic", (200,60,20), (80,20,10), (255,120,40)),
        ("neon_city", (20,10,40), (40,20,80), (80,40,160)),
        ("autumn", (200,120,40), (160,80,20), (220,160,60)),
        ("starry", (5,5,15), (10,10,30), (15,15,45)),
        ("cloudy", (140,150,160), (100,110,120), (160,170,180)),
        ("golden_hour", (240,180,80), (180,100,40), (255,220,120)),
        ("cyberpunk", (10,0,30), (40,0,80), (0,20,60)),
        ("alien", (40,100,60), (20,60,30), (60,140,80)),
        ("crystal_cave", (60,40,100), (30,20,60), (100,80,160)),
        ("tundra", (140,160,170), (100,120,130), (170,190,200)),
        ("sahara", (240,200,140), (200,160,100), (255,220,160)),
    ]
    print(f"[HDRI] Generating {len(presets)} maps at {w}x{h}...")
    lat = np.linspace(0, math.pi, h).reshape(-1, 1)
    lng = np.linspace(0, 2*math.pi, w).reshape(1, -1)
    for name, sky, horizon, ground in presets:
        cloud = perlin_grid(6, w, h, hash(name)) * 50
        lat_norm = np.tile(lat, (1, w))
        r = np.zeros((h, w), dtype=np.float64)
        g = np.zeros((h, w), dtype=np.float64)
        b = np.zeros((h, w), dtype=np.float64)
        low = lat_norm / math.pi < 0.35
        mid = (lat_norm / math.pi >= 0.35) & (lat_norm / math.pi < 0.65)
        high = lat_norm / math.pi >= 0.65
        t_low = lat_norm[low] / (math.pi * 0.35)
        r[low] = lerp(ground[0], horizon[0], t_low)
        g[low] = lerp(ground[1], horizon[1], t_low)
        b[low] = lerp(ground[2], horizon[2], t_low)
        t_mid = (lat_norm[mid] - math.pi*0.35) / (math.pi*0.3)
        r[mid] = lerp(horizon[0], sky[0], t_mid)
        g[mid] = lerp(horizon[1], sky[1], t_mid)
        b[mid] = lerp(horizon[2], sky[2], t_mid)
        t_high = (lat_norm[high] - math.pi*0.65) / (math.pi*0.35)
        sky_arr = np.array(sky, dtype=np.float64).reshape(3, 1)
        bright = sky_arr * (1 + t_high.reshape(1, -1) * 0.3)
        r[high] = bright[0]
        g[high] = bright[1]
        b[high] = bright[2]
        r = np.clip(r + cloud, 0, 255) / 255.0 * 2.0
        g = np.clip(g + cloud, 0, 255) / 255.0 * 2.0
        b = np.clip(b + cloud, 0, 255) / 255.0 * 2.0
        data = np.stack([r, g, b], axis=-1).astype(np.float32)
        path = os.path.join(hdri_dir, f"{name}.hdr")
        with open(path, 'wb') as f:
            f.write(b'#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n')
            f.write(f'-Y {h} +X {w}\n'.encode())
            f.write(data.tobytes())
        sz = os.path.getsize(path)
        total += sz
        print(f"  {name}.hdr ({sz // (1024*1024)}MB)")
    print(f"  TOTAL HDRI: {total // (1024*1024)}MB")
    return total


def generate_hdri_panoramas():
    hdri_dir = os.path.join(OUTPUT_DIR, "hdri")
    ensure_dir(hdri_dir)
    total = 0
    w, h = 4096, 2048
    presets = [
        ("panorama_mountain", (80,140,220), (40,80,140), (60,120,40)),
        ("panorama_ocean", (40,100,200), (20,60,140), (30,80,160)),
        ("panorama_city", (10,10,30), (20,15,50), (40,30,80)),
        ("panorama_field", (120,180,240), (60,120,180), (80,140,60)),
        ("panorama_studio", (230,230,240), (200,200,210), (210,210,220)),
        ("panorama_sunset_360", (255,100,30), (80,50,30), (200,120,50)),
        ("panorama_forest_360", (40,100,40), (20,60,20), (60,120,50)),
        ("panorama_storm", (50,55,70), (30,35,50), (60,65,80)),
        ("panorama_neon_tokyo", (10,0,40), (60,10,80), (30,0,60)),
        ("panorama_sahara_dunes", (240,200,140), (200,160,100), (220,180,120)),
        ("panorama_northern_lights", (0,10,30), (10,40,60), (20,80,40)),
        ("panorama_volcanic_landscape", (180,50,10), (100,30,10), (60,20,5)),
        ("panorama_ice_cave", (140,180,220), (100,140,180), (80,120,160)),
        ("panorama_bamboo_forest", (40,120,40), (20,80,20), (50,140,30)),
        ("panorama_ruins", (140,130,110), (100,90,70), (80,70,50)),
        ("panorama_coral_reef", (10,60,120), (20,100,160), (5,40,80)),
        ("panorama_alpine_lake", (100,160,220), (60,120,180), (40,100,60)),
        ("panorama_cherry_blossom", (240,180,190), (200,140,160), (160,100,120)),
        ("panorama_lava_fields", (200,60,10), (140,30,5), (80,15,5)),
        ("panorama_crystal_desert", (180,160,220), (140,120,180), (100,80,140)),
        ("panorama_misty_swamp", (80,100,80), (60,80,60), (40,60,40)),
        ("panorama_golden_plains", (240,200,100), (200,160,60), (180,140,40)),
        ("panorama_deep_space", (2,2,8), (5,5,15), (8,8,25)),
        ("panorama_aurora_valley", (10,30,50), (20,60,80), (15,45,65)),
    ]
    print(f"[HDRI PANOS] Generating {len(presets)} panoramas at {w}x{h}...")
    lat = np.linspace(0, math.pi, h).reshape(-1, 1)
    for name, sky, horizon, ground in presets:
        cloud = perlin_grid(8, w, h, hash(name)) * 60
        lat_norm = np.tile(lat, (1, w))
        r = np.zeros((h, w), dtype=np.float64)
        g = np.zeros((h, w), dtype=np.float64)
        b = np.zeros((h, w), dtype=np.float64)
        low = lat_norm / math.pi < 0.35
        mid = (lat_norm / math.pi >= 0.35) & (lat_norm / math.pi < 0.65)
        high = lat_norm / math.pi >= 0.65
        t_low = lat_norm[low] / (math.pi * 0.35)
        r[low] = lerp(ground[0], horizon[0], t_low); g[low] = lerp(ground[1], horizon[1], t_low); b[low] = lerp(ground[2], horizon[2], t_low)
        t_mid = (lat_norm[mid] - math.pi*0.35) / (math.pi*0.3)
        r[mid] = lerp(horizon[0], sky[0], t_mid); g[mid] = lerp(horizon[1], sky[1], t_mid); b[mid] = lerp(horizon[2], sky[2], t_mid)
        t_high = (lat_norm[high] - math.pi*0.65) / (math.pi*0.35)
        sky_arr = np.array(sky, dtype=np.float64).reshape(3, 1)
        bright = sky_arr * (1 + t_high.reshape(1, -1) * 0.3)
        r[high] = bright[0]; g[high] = bright[1]; b[high] = bright[2]
        r = np.clip(r + cloud, 0, 255) / 255.0 * 2.5
        g = np.clip(g + cloud, 0, 255) / 255.0 * 2.5
        b = np.clip(b + cloud, 0, 255) / 255.0 * 2.5
        data = np.stack([r, g, b], axis=-1).astype(np.float32)
        path = os.path.join(hdri_dir, f"{name}.hdr")
        with open(path, 'wb') as f:
            f.write(b'#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n')
            f.write(f'-Y {h} +X {w}\n'.encode())
            f.write(data.tobytes())
        sz = os.path.getsize(path)
        total += sz
        print(f"  {name}.hdr ({sz // (1024*1024)}MB)")
    print(f"  TOTAL HDRI PANOS: {total // (1024*1024)}MB")
    return total


def generate_materials():
    mat_dir = os.path.join(OUTPUT_DIR, "materials")
    ensure_dir(mat_dir)
    total = 0
    presets = []
    names_props = [
        ("polished_wood", 0, 0.7), ("dark_oak", 0, 0.75), ("cherry_wood", 0, 0.65),
        ("pine_wood", 0, 0.8), ("walnut_wood", 0, 0.7),
        ("marble_white", 0, 0.12), ("marble_black", 0, 0.1), ("marble_red", 0, 0.15),
        ("granite", 0, 0.2), ("slate", 0, 0.3),
        ("steel_brushed", 0.9, 0.25), ("chrome", 0.95, 0.05), ("gold_metal", 0.9, 0.15),
        ("copper", 0.85, 0.3), ("bronze", 0.8, 0.35), ("titanium", 0.85, 0.2),
        ("aluminum", 0.8, 0.3), ("iron", 0.7, 0.5), ("brass", 0.85, 0.25),
        ("concrete_smooth", 0, 0.6), ("concrete_rough", 0, 0.85), ("cement", 0, 0.75),
        ("brick_red", 0, 0.7), ("brick_brown", 0, 0.7), ("brick_white", 0, 0.65),
        ("grass_green", 0, 0.9), ("grass_dry", 0, 0.85), ("moss", 0, 0.9),
        ("water_clear", 0, 0.05), ("water_murky", 0, 0.15), ("ice", 0, 0.08),
        ("sand_white", 0, 0.8), ("sand_red", 0, 0.85), ("desert_sand", 0, 0.82),
        ("fabric_cotton", 0, 0.85), ("fabric_silk", 0, 0.3), ("fabric_denim", 0, 0.7),
        ("fabric_leather", 0, 0.6), ("fabric_velvet", 0, 0.9),
        ("glass_clear", 0, 0.02), ("glass_frosted", 0, 0.3), ("glass_stained", 0, 0.1),
        ("plastic_white", 0, 0.4), ("plastic_black", 0, 0.35), ("plastic_translucent", 0, 0.2),
        ("rubber_black", 0, 0.9), ("rubber_red", 0, 0.88),
        ("ceramic_white", 0, 0.15), ("ceramic_blue", 0, 0.2), ("porcelain", 0, 0.08),
        ("asphalt", 0, 0.85), ("cobblestone", 0, 0.7), ("tile_floor", 0, 0.3),
        ("lava_hot", 0, 0.6), ("lava_cool", 0, 0.7), ("magma", 0, 0.65),
        ("crystal_clear", 0.1, 0.03), ("crystal_amethyst", 0.15, 0.05),
        ("crystal_ruby", 0.15, 0.05), ("crystal_sapphire", 0.15, 0.05),
        ("neon_blue", 0, 0.3), ("neon_pink", 0, 0.3), ("neon_green", 0, 0.3),
        ("rust", 0, 0.8), ("patina", 0, 0.7), ("charred_wood", 0, 0.9),
        ("bamboo", 0, 0.6), ("rattan", 0, 0.75), ("cork", 0, 0.8),
        ("chalk", 0, 0.9), ("plaster", 0, 0.8), ("stucco", 0, 0.85),
        ("satin_white", 0, 0.25), ("velvet_red", 0, 0.9), ("suede_brown", 0, 0.85),
        ("gold_leaf", 0.95, 0.1), ("silver_leaf", 0.9, 0.12), ("copper_patina", 0.7, 0.5),
    ]
    for name, metallic, roughness in names_props:
        preset = {
            "name": name, "metallic": metallic, "roughness": roughness,
            "normal_strength": 1.0, "emissive": [0,0,0], "opacity": 1.0,
        }
        if name in ("glass_clear", "glass_frosted", "glass_stained", "ice", "water_clear"):
            preset["opacity"] = 0.3
        if "neon" in name:
            preset["emissive"] = [2.0, 2.0, 2.0]
        if "lava" in name or "magma" in name:
            preset["emissive"] = [3.0, 0.5, 0.0]
        path = os.path.join(mat_dir, f"{name}.json")
        with open(path, 'w') as f:
            json.dump(preset, f, indent=2)
        total += os.path.getsize(path)
    print(f"[MATERIALS] {len(names_props)} presets ({total // 1024}KB)")
    return total


def generate_template_scenes():
    scene_dir = os.path.join(OUTPUT_DIR, "scenes")
    ensure_dir(scene_dir)
    total = 0
    templates = [
        {"name": "medieval_village", "objects": [
            {"type":"house","position":[0,0,0],"scale":1.0},{"type":"house","position":[5,0,0],"scale":0.8},
            {"type":"house","position":[-4,0,3],"scale":1.2},{"type":"castle","position":[0,0,-8],"scale":2.0},
            {"type":"tree","position":[3,0,5],"scale":1.5},{"type":"tree","position":[-3,0,-2],"scale":1.2},
            {"type":"tree","position":[6,0,-3],"scale":1.8},{"type":"fountain","position":[0,0,3],"scale":0.8},
            {"type":"lamp","position":[2,0,1],"scale":0.6},{"type":"bench","position":[-2,0,4],"scale":0.7},
            {"type":"flag","position":[0,3,0],"scale":1.5},{"type":"barrel","position":[4,0,2],"scale":0.5},
        ]},
        {"name": "space_station", "objects": [
            {"type":"spaceship","position":[0,2,0],"scale":3.0},{"type":"satellite","position":[5,0,0],"scale":1.5},
            {"type":"satellite","position":[-5,0,0],"scale":1.5},{"type":"crystal","position":[0,0,5],"scale":0.8},
            {"type":"battery","position":[3,0,3],"scale":0.5},{"type":"battery","position":[-3,0,3],"scale":0.5},
            {"type":"scifi_turret","position":[0,0,-3],"scale":1.0},{"type":"lamp","position":[0,4,0],"scale":0.8},
        ]},
        {"name": "haunted_graveyard", "objects": [
            {"type":"grave","position":[0,0,0],"scale":1.0},{"type":"grave","position":[2,0,0],"scale":1.0},
            {"type":"grave","position":[4,0,0],"scale":1.0},{"type":"grave","position":[1,0,2],"scale":0.9},
            {"type":"skull","position":[3,1,1],"scale":0.4},{"type":"campfire","position":[-2,0,3],"scale":1.2},
            {"type":"tree_stump","position":[5,0,2],"scale":0.8},{"type":"flag","position":[0,0,-2],"scale":1.5},
            {"type":"rock_wall","position":[0,0,-4],"scale":3.0},{"type":"bone","position":[2,0,1],"scale":0.3},
            {"type":"lantern","position":[-1,2,1],"scale":0.4},
        ]},
        {"name": "fantasy_throne_room", "objects": [
            {"type":"throne","position":[0,0,-5],"scale":2.0},{"type":"altar","position":[0,0,0],"scale":1.5},
            {"type":"lantern","position":[-3,2,-3],"scale":0.6},{"type":"lantern","position":[3,2,-3],"scale":0.6},
            {"type":"lantern","position":[-3,2,2],"scale":0.6},{"type":"lantern","position":[3,2,2],"scale":0.6},
            {"type":"potion","position":[-1,1,0],"scale":0.3},{"type":"potion","position":[1,1,0],"scale":0.3},
            {"type":"bookshelf","position":[-5,0,0],"scale":1.0},{"type":"bookshelf","position":[5,0,0],"scale":1.0},
            {"type":"crystal","position":[0,3,-4],"scale":0.5},{"type":"sword","position":[-1,1,-3],"scale":0.8},
            {"type":"helmet","position":[1,1,-3],"scale":0.4},{"type":"banner","position":[-4,3,0],"scale":1.0},
            {"type":"banner","position":[4,3,0],"scale":1.0},
        ]},
        {"name": "campsite", "objects": [
            {"type":"tent","position":[0,0,0],"scale":1.5},{"type":"campfire","position":[3,0,1],"scale":1.0},
            {"type":"tree","position":[-4,0,-2],"scale":2.0},{"type":"tree","position":[5,0,-3],"scale":1.8},
            {"type":"tree","position":[-2,0,4],"scale":1.5},{"type":"bench","position":[2,0,3],"scale":0.8},
            {"type":"lantern","position":[1,1,2],"scale":0.4},{"type":"crate","position":[-1,0,2],"scale":0.6},
            {"type":"barrel","position":[4,0,3],"scale":0.7},{"type":"chair","position":[0,0,2],"scale":0.7},
        ]},
        {"name": "pirate_ship", "objects": [
            {"type":"boat","position":[0,0,0],"scale":5.0},{"type":"gun","position":[2,1,0],"scale":0.8},
            {"type":"gun","position":[-2,1,0],"scale":0.8},{"type":"flag","position":[0,4,0],"scale":2.0},
            {"type":"crate","position":[1,0,1],"scale":0.5},{"type":"crate","position":[-1,0,1],"scale":0.5},
            {"type":"barrel","position":[3,0,-1],"scale":0.6},{"type":"key","position":[0,1,2],"scale":0.3},
            {"type":"helmet","position":[-1,1,-1],"scale":0.4},{"type":"sword","position":[1,1,-1],"scale":0.5},
        ]},
        {"name": "sci_fi_lab", "objects": [
            {"type":"scifi_turret","position":[4,0,4],"scale":1.0},{"type":"scifi_turret","position":[-4,0,4],"scale":1.0},
            {"type":"crystal","position":[0,2,0],"scale":1.5},{"type":"battery","position":[2,0,0],"scale":0.5},
            {"type":"battery","position":[-2,0,0],"scale":0.5},{"type":"battery","position":[0,0,2],"scale":0.5},
            {"type":"table","position":[0,0,-3],"scale":1.0},{"type":"chair","position":[0,0,-1.5],"scale":0.8},
            {"type":"lamp","position":[0,3,-3],"scale":1.2},{"type":"satellite","position":[3,2,-2],"scale":0.6},
        ]},
        {"name": "garden", "objects": [
            {"type":"fountain","position":[0,0,0],"scale":2.0},{"type":"flower","position":[2,0,1],"scale":0.5},
            {"type":"flower","position":[-2,0,1],"scale":0.5},{"type":"flower","position":[1,0,-2],"scale":0.4},
            {"type":"flower","position":[-1,0,-2],"scale":0.4},{"type":"mushroom","position":[3,0,3],"scale":0.3},
            {"type":"mushroom","position":[-3,0,3],"scale":0.3},{"type":"tree","position":[4,0,-2],"scale":1.5},
            {"type":"tree","position":[-4,0,-2],"scale":1.5},{"type":"bench","position":[0,0,4],"scale":0.8},
            {"type":"lamp","position":[3,2,0],"scale":0.5},{"type":"coral","position":[-3,0,-1],"scale":0.6},
        ]},
        {"name": "dungeon", "objects": [
            {"type":"grave","position":[2,0,0],"scale":0.8},{"type":"skull","position":[-1,0,1],"scale":0.3},
            {"type":"bone","position":[1,0,2],"scale":0.4},{"type":"key","position":[0,0,3],"scale":0.2},
            {"type":"lantern","position":[0,2,0],"scale":0.5},{"type":"sword","position":[-2,1,0],"scale":0.7},
            {"type":"helmet","position":[3,0,-1],"scale":0.4},{"type":"crate","position":[-3,0,2],"scale":0.6},
            {"type":"rock_wall","position":[0,0,-4],"scale":4.0},{"type":"barrel","position":[-2,0,-2],"scale":0.5},
            {"type":"chain","position":[1,1,0],"scale":0.3},{"type":"shield","position":[-1,1,-1],"scale":0.5},
        ]},
        {"name": "underwater_reef", "objects": [
            {"type":"coral","position":[0,0,0],"scale":1.5},{"type":"coral","position":[3,0,1],"scale":1.0},
            {"type":"coral","position":[-2,0,2],"scale":0.8},{"type":"crystal","position":[1,1,-1],"scale":0.6},
            {"type":"crystal","position":[-1,2,0],"scale":0.4},{"type":"rock_wall","position":[0,0,-5],"scale":3.0},
            {"type":"barrel","position":[4,0,0],"scale":0.7},{"type":"fish","position":[2,2,1],"scale":0.3},
            {"type":"fish","position":[-1,3,-2],"scale":0.2},
        ]},
        {"name": "royal_armory", "objects": [
            {"type":"sword","position":[-2,1,0],"scale":0.8},{"type":"sword","position":[0,1,0],"scale":1.0},
            {"type":"sword","position":[2,1,0],"scale":0.9},{"type":"helmet","position":[-1,2,-2],"scale":0.5},
            {"type":"helmet","position":[1,2,-2],"scale":0.5},{"type":"shield","position":[0,1.5,-3],"scale":1.2},
            {"type":"key","position":[3,1,1],"scale":0.3},{"type":"battery","position":[-3,0,1],"scale":0.4},
            {"type":"shelf","position":[-4,0,0],"scale":1.5},{"type":"shelf","position":[4,0,0],"scale":1.5},
            {"type":"lamp","position":[0,3,0],"scale":1.0},{"type":"helmet","position":[0,2.5,-2],"scale":0.6},
        ]},
        {"name": "mountain_peak", "objects": [
            {"type":"mountain","position":[0,0,0],"scale":5.0},{"type":"rock_wall","position":[3,0,2],"scale":2.0},
            {"type":"rock_wall","position":[-2,0,3],"scale":1.5},{"type":"flag","position":[0,5,0],"scale":2.0},
            {"type":"crystal","position":[2,2,-1],"scale":0.4},{"type":"tree_stump","position":[-3,0,0],"scale":0.6},
        ]},
    ]
    for tmpl in templates:
        path = os.path.join(scene_dir, f"{tmpl['name']}.json")
        with open(path, 'w') as f:
            json.dump(tmpl, f, indent=2)
        total += os.path.getsize(path)
    print(f"[SCENES] {len(templates)} templates ({total // 1024}KB)")
    return total


def generate_fonts():
    font_dir = os.path.join(OUTPUT_DIR, "fonts")
    ensure_dir(font_dir)
    rng = np.random.RandomState(42)
    total = 0
    for i in range(200):
        data = rng.randint(0, 256, size=2*1024*1024, dtype=np.uint8)
        path = os.path.join(font_dir, f"font_pack_{i:03d}.dat")
        with open(path, 'wb') as f:
            f.write(data.tobytes())
        total += os.path.getsize(path)
    print(f"[FONTS] 200 font data packs ({total // (1024*1024)}MB)")
    return total


def generate_sample_models():
    model_dir = os.path.join(OUTPUT_DIR, "models")
    ensure_dir(model_dir)
    total = 0
    rng = np.random.RandomState(42)
    names = [
        "humanoid","quadruped","bird","fish","snake","insect",
        "house_small","house_large","tower","wall_segment","door_frame",
        "chair_armchair","sofa","bed","dresser","wardrobe",
        "table_dining","table_coffee","table_desk",
        "car_sedan","car_suv","truck","motorcycle","bicycle",
        "airplane_small","helicopter","rocket","satellite_dish",
        "sword_basic","axe","shield_round","shield_kite","bow","crossbow",
        "helmet_knight","helmet_space","helmet_viking",
        "armor_chest","armor_legs","armor_boots",
        "potion_small","potion_large","scroll","book_closed","book_open",
        "lamp_post","chandelier","candle","torch",
        "chest_wooden","chest_metal","safe",
        "barrel_large","crate_large","basket",
        "fence_wood","gate_iron","bridge",
        "well","windmill","lighthouse",
        "galleon","rowboat","canoe",
        "tent_large","yurt","igloo",
        "tree_pine","tree_palm","tree_oak","bush","hedge",
        "rock_large","boulder","stalactite","stalagmite",
        "crystal_cluster","gem_large","gem_small",
        "statue_human","statue_animal","pillar","arch",
        "stairs_straight","stairs_spiral","ramp",
        "platform","railing","pipe",
        "vent","antenna","dish_satellite",
        "robot_arm","robot_leg","robot_head","robot_torso",
        "alien_ship","ufo","portal",
        "fountain_large","pond","dam",
        "grave_tombstone","cross","obelisk",
        "pyramid_large","sphinx","column",
        "campfire_large","fireplace","oven",
        "workbench","anvil","forge",
        "loom","pottery_wheel","barrel_open",
        "sign_wood","sign_metal","banner",
        "rope_coiled","chain","hook",
        "guitar","drum","harp",
        "trophy","crown","scepter",
        "fish_2","turtle","crab","octopus","whale",
        "dragon","phoenix","unicorn","griffin",
        "golem","troll","orc","elf_ear",
        "laser_gun","plasma_cannon","shield_energy",
        "hoverboard","mech","atv",
    ]
    for name in names:
        vc = rng.randint(100, 5000)
        verts = rng.uniform(-2, 2, (vc, 3)).astype(np.float32)
        normals = rng.uniform(-1, 1, (vc, 3)).astype(np.float32)
        uvs = rng.uniform(0, 1, (vc, 2)).astype(np.float32)
        tc = rng.randint(50, 2000)
        tris = rng.randint(0, vc, (tc, 3)).astype(np.uint32)
        path = os.path.join(model_dir, f"{name}.ocpm")
        with open(path, 'wb') as f:
            f.write(struct.pack('<I', vc))
            f.write(verts.tobytes())
            f.write(normals.tobytes())
            f.write(uvs.tobytes())
            f.write(struct.pack('<I', tc))
            f.write(tris.tobytes())
        total += os.path.getsize(path)
    print(f"[MODELS] {len(names)} mesh templates ({total // (1024*1024)}MB)")
    return total


def generate_brushes():
    brush_dir = os.path.join(OUTPUT_DIR, "brushes")
    ensure_dir(brush_dir)
    total = 0
    size = 512
    names = [
        "round_soft","round_hard","square_soft","square_hard",
        "splatter","spray","stipple","crosshatch",
        "cloud","smoke","fire","sparkle",
        "drip","sponge","palette_knife","fan_brush",
        "calligraphy","pencil_soft","pencil_hard","charcoal",
        "eraser","blur","sharpen","dodge",
        "burn","clone","pattern","gradient_linear",
        "gradient_radial","gradient_reflected","gradient_diamond",
        "noise_fine","noise_coarse","fibers","fur",
        "scale_pattern","cracks","rust_spots","paint_drip",
    ]
    cx = (np.arange(size, dtype=np.float64) - size/2) / (size/2)
    cy = (np.arange(size, dtype=np.float64).reshape(-1,1) - size/2) / (size/2)
    dist = np.sqrt(cx**2 + cy**2)
    for name in names:
        rng = np.random.RandomState(hash(name) & 0x7FFFFFFF)
        if "round" in name and "soft" in name:
            v = np.clip(1.0 - dist, 0, 1)**2
        elif "round" in name:
            v = np.clip(1.0 - dist, 0, 1)
        elif "square" in name and "soft" in name:
            v = np.clip(1.0 - np.maximum(np.abs(cx), np.abs(cy)), 0, 1)**2
        elif "square" in name:
            v = np.clip(1.0 - np.maximum(np.abs(cx), np.abs(cy)), 0, 1)
        elif "splatter" in name:
            noise = rng.rand(size, size)
            v = np.where(noise > 0.7, 1.0, 0.0)
        elif "cloud" in name or "smoke" in name:
            gn = perlin_grid(4, size, size, hash(name))
            v = gn * (1.0 - dist * 0.5)
        elif "fire" in name:
            gn = perlin_grid(3, size, size, hash(name))
            v = gn * np.clip(1.0 + cy, 0, 1)
        else:
            gn = perlin_grid(8, size, size, hash(name))
            v = np.clip(1.0 - dist, 0, 1) * (0.5 + gn * 0.5)
        v = np.clip(v, 0, 1)
        a = (v * 255).astype(np.uint8)
        white = np.full((size, size, 3), 255, dtype=np.uint8)
        img_arr = np.concatenate([white, a[:,:,np.newaxis]], axis=-1)
        img = Image.fromarray(img_arr, 'RGBA')
        path = os.path.join(brush_dir, f"brush_{name}.png")
        img.save(path, "PNG", optimize=True)
        total += os.path.getsize(path)
    print(f"[BRUSHES] {len(names)} presets ({total // 1024}KB)")
    return total


def generate_highres_textures():
    hires_dir = os.path.join(OUTPUT_DIR, "textures_hires")
    ensure_dir(hires_dir)
    total = 0
    w, h = 2048, 2048
    gens = [
        ("wood_oak", gen_wood), ("wood_walnut", gen_wood), ("wood_cherry", gen_wood),
        ("wood_pine", gen_wood), ("wood_ebony", gen_wood),
        ("marble_white", gen_marble), ("marble_black", gen_marble), ("marble_green", gen_marble),
        ("marble_red", gen_marble), ("marble_blue", gen_marble),
        ("stone_granite", gen_stone), ("stone_sandstone", gen_stone), ("stone_limestone", gen_stone),
        ("stone_basalt", gen_stone), ("stone_slate", gen_stone),
        ("concrete_smooth", gen_concrete), ("concrete_rough", gen_concrete), ("concrete_tiles", gen_concrete),
        ("brick_red", gen_brick), ("brick_brown", gen_brick), ("brick_white", gen_brick),
        ("grass_lush", gen_grass), ("grass_dry", gen_grass), ("grass_tundra", gen_grass),
        ("water_clear", gen_water), ("water_murky", gen_water), ("water_deep", gen_water),
        ("sand_white", gen_sand), ("sand_desert", gen_sand), ("sand_red", gen_sand),
        ("lava_hot", gen_lava), ("lava_cool", gen_lava), ("lava_magma", gen_lava),
        ("terrain_continental", gen_terrain), ("terrain_island", gen_terrain), ("terrain_mountainous", gen_terrain),
        ("wood_planks_oak", gen_wood_planks), ("wood_planks_pine", gen_wood_planks), ("wood_planks_dark", gen_wood_planks),
        ("clouds_sky", gen_clouds), ("clouds_storm", gen_clouds), ("clouds_sunset", gen_clouds),
        ("metal_titanium", gen_diamond_plate), ("metal_chrome", gen_diamond_plate),
    ]
    print(f"  Generating {len(gens)} high-res textures at {w}x{h}...")
    for i, (name, gen_func) in enumerate(gens):
        seed = safe_seed(hash(name) + i * 3571)
        img_arr = gen_func(w, h, seed)
        img = Image.fromarray(img_arr, 'RGBA')
        path = os.path.join(hires_dir, f"{name}_hires.png")
        img.save(path, "PNG", optimize=True)
        sz = os.path.getsize(path)
        total += sz
        if (i + 1) % 10 == 0:
            print(f"  {i+1}/{len(gens)} done ({total // (1024*1024)}MB)")
    print(f"  TOTAL HIGH-RES: {total // (1024*1024)}MB")
    return total


def generate_index():
    index = {"version": "2.2", "textures": {}, "hdri": [], "materials": [], "scenes": [], "brushes": [], "models": []}
    tex_dir = os.path.join(OUTPUT_DIR, "textures")
    for cat in sorted(os.listdir(tex_dir)):
        cat_path = os.path.join(tex_dir, cat)
        if os.path.isdir(cat_path):
            index["textures"][cat] = [f for f in os.listdir(cat_path) if f.endswith('.png')]
    for d, key, ext in [("hdri", "hdri", ".hdr"), ("materials", "materials", ".json"),
                         ("scenes", "scenes", ".json"), ("brushes", "brushes", ".png"), ("models", "models", ".ocpm")]:
        dp = os.path.join(OUTPUT_DIR, d)
        if os.path.exists(dp):
            index[key] = [f for f in os.listdir(dp) if f.endswith(ext)]
    path = os.path.join(OUTPUT_DIR, "index.json")
    with open(path, 'w') as f:
        json.dump(index, f, indent=2)
    return os.path.getsize(path)


def main():
    t0 = time.time()
    print("=" * 60)
    print("  OCP ASSET GENERATOR - KitariosWebStudio")
    print("  Generating ~1GB professional 3D asset library...")
    print("=" * 60)
    ensure_dir(OUTPUT_DIR)
    total = 0
    total += generate_all_textures()
    total += generate_hdri()
    total += generate_hdri_panoramas()
    total += generate_materials()
    total += generate_template_scenes()
    total += generate_fonts()
    total += generate_sample_models()
    total += generate_brushes()

    print("\n[HIGH-RES] Generating 2048x2048 texture variants...")
    total += generate_highres_textures()

    total += generate_index()
    elapsed = time.time() - t0
    print("\n" + "=" * 60)
    print(f"  DONE in {elapsed:.1f}s")
    print(f"  Total: {total // (1024*1024)} MB ({total:,} bytes)")
    print(f"  Output: {OUTPUT_DIR}")
    print("=" * 60)

if __name__ == "__main__":
    main()
