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

### AI Generation (60+ Generators)
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

### v2.3 — Full Installer (Recommended)
**[OCP v2.3 Setup (667 MB)](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v2.3/OCP-v2.3-Setup.exe)** — Professional installer with all resources embedded (textures, models, fonts, brushes, scenes, materials). Just run the installer!

**[OCP HDRI Pack (1.7 GB)](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v2.3/OCP-HDRI-Pack.7z)** — HDRI environment maps for realistic lighting. Place next to `OCP-v2.3-Setup.exe` before installing — the installer will extract it automatically.

### v2.3 — Portable
**[OCP v2.3 Portable (Part 1: Core)](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v2.3/OCP-v2.3-part1-core.zip)** — exe + textures + models + fonts + brushes + scenes + materials (~660 MB)

**[OCP v2.3 Portable (Part 2: HDRI)](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v2.3/OCP-v2.3-part2-hdri.zip)** — HDRI environment maps (~1.7 GB)

### Previous Versions
- **[v2.2 Installer](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v2.2/OCP-v2.2-Installer.exe)** — Previous version (~1.3 MB, exe only)
- **[v2.1](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v2.1/OCP-v2.1-CPP.zip)** — C++ version
- **[v1.0 Python](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v1.0/OCP.exe)** — Original Python version (117 MB)

## Quick Start

### Installer (v2.3)
1. Download `OCP-v2.3-Setup.exe` (and optionally `OCP-HDRI-Pack.7z` for HDRI lighting)
2. Run the installer and follow the steps
3. Select which components to install (textures, models, fonts, HDRI, etc.)
4. OCP will appear in your Start Menu

### Portable (v2.3)
1. Download `OCP-v2.3-part1-core.zip` and `OCP-v2.3-part2-hdri.zip`
2. Extract both into the same folder
3. Run `OCP.exe` (`libwinpthread-1.dll` must be in the same folder)

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
