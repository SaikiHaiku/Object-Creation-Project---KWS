# OCP - Object Creation Project

Professional 3D/2D object creation tool with manual editing and AI procedural generation.
**Developed by KitariosWebStudio - KWS**

## Features

### Pro Editor
- **Cook-Torrance PBR** rendering with real-time metallic/roughness workflow
- **16 dynamic lights** with shadow mapping
- **Material Editor** — Albedo, Metallic, Roughness, AO, Normal, Emission
- **Grid** with distance-based fade-out
- **Selection Outlines** with FBO-based glow
- **Performance HUD** (FPS, draw calls, triangles)
- **Modifier Stack** — Subdivision, Mirror, Array, Smooth, Lattice, Decimate, Solidify
- **Wireframe** (W) and **X-Ray** (X) modes

### Edit Mode (Tab)
- **BMesh half-edge topology** — real professional mesh data structure
- **Vertex/Edge/Face selection** with Ctrl+Click for multi-select
- **33 modeling tools**: Extrude, Inset, Bevel, Loop Cut, Knife, Bridge, Fill, Grid Fill, Merge, Split, Weld, Rip, Spin, Screw, Solidify, Mirror, Subdivide, Decimate, Triangulate, Untriangulate, Relax, Smooth, Recalculate/Flip Normals
- **Edit overlays**: Vertex dots, edge highlights, face selection glow

### Sculpt Mode (Shift+Tab)
- **10 brush types**: Draw, Clay, Inflate, Smooth, Flatten, Grab, Crease, Pinch, Mask, SmoothMask
- **Adjustable** radius, strength, focal falloff
- **X/Y/Z symmetry** support
- Real-time sculpt cursor visualization

### AI Generation (Parametric Procedural Engine)
- **Organic deformation** — Perlin noise, ridged multifractal, domain warping
- No symmetry artifacts — every mesh is unique
- Scenes: house, robot, castle, spaceship, tree, sword, airplane, boat, skull, brain, flower, mushroom, donut, jewelry, throne, camp, fountain, crystal, coral, butterfly, cat, dog, bird, cave, tent, bookshelf, dragon, alien, car, tank, guitar, piano, house interior, city, dungeon, factory, library, market, tower, bridge, garden, lighthouse, windmill, volcano, mountain, ocean, space station, submarine, robot army, medieval village, cyberpunk city, steampunk machine, fantasy castle, ancient temple, modern skyscraper, haunted house, underwater base, space colony, and more
- 100% offline — no internet required

### QWERTY Tools (Blender-style)
- G = Move | E = Rotate | R = Scale | Q = Select
- Multi-selection (Ctrl+Click)
- Copy/Paste (Ctrl+C/V)
- Group/Ungroup (Ctrl+G)
- Undo/Redo (Ctrl+Z/Y) — 50 levels
- Save/Load scenes (Ctrl+S/O)

### Export
- OBJ, STL, PNG

### 3D Resource Library (3.4 GB)
- **80+ PBR textures** across 13 categories (wood, metal, stone, fabric, etc.)
- **44 high-res 2048x2048 textures**
- **20 HDRI environment maps** + 24 panoramas (for realistic lighting)
- **78 PBR material presets**
- **148 mesh templates**
- **200 font data packs** for 3D text
- **39 drawing brushes**
- **12 scene templates**

## Download

**[OCP v3.2 Installer (1.2 MB)](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v3.2/OCP-Installer.exe)** — Parametric AI engine, BMesh edit mode, sculpt mode, 33 modeling tools, 10 sculpt brushes. Requires OpenGL 3.3+ GPU.

## Quick Start

1. Download `OCP-Installer.exe`
2. Run it and click Install
3. OCP installs to `%LOCALAPPDATA%\OCP` and creates a Desktop shortcut
4. Requires OpenGL 3.3+ GPU

### Build from Source (C++)
```bash
# Requires MinGW-w64 (MSVCRT) at C:\mingw64_msvcrt\mingw64\bin
python cpp/build.py
```

### Build from Source (Python)
```bash
pip install -r requirements.txt
python main.py
```

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+C / V | Copy / Paste |
| Ctrl+D | Duplicate |
| Ctrl+G | Group |
| Ctrl+Shift+G | Ungroup |
| Ctrl+S / O | Save / Open scene |
| Ctrl+N | New scene |
| Ctrl+A | Select all |
| Tab | Edit Mode toggle |
| Shift+Tab | Sculpt Mode toggle |
| Q | Select tool |
| G | Move tool |
| E | Rotate tool |
| R | Scale tool |
| W | Wireframe mode |
| X | X-Ray mode |
| F | Focus on selection |
| H | Console |
| Del | Delete selection |
| F1 | About |
| Middle Mouse | Orbit |
| Right Click | Pan |
| Scroll | Zoom |
| Ctrl+Click | Multi-select |

## Tech Stack

- C++17 / MinGW-w64 (MSVCRT)
- OpenGL 3.3 Core + Dear ImGui
- GLFW 3.4 (window/input)
- GLM (mathematics)
- GLAD (OpenGL loader)
- Cook-Torrance BRDF (PBR rendering)
- Smart pointers (zero memory leaks)
- 60+ AI generators with Perlin noise organic deformation

## License

MIT License — Copyright 2026 KitariosWebStudio - KWS
