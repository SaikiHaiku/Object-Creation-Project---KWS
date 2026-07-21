# OCP - Object Creation Project

Logiciel de creation d'objets 3D et 2D avec edition manuelle et generation par IA.

## Fonctionnalites

- **Mode Manuel**: Editeur 3D complet avec OpenGL, primitives, transformations, materiaux
- **Mode IA**: Generer des scenes 3D complexes a partir de prompts textuels - 100% local
- **Export**: Formats OBJ, STL, SVG, PNG
- **Materiaux**: Materiaux PBR avec metallic, roughness, shininess
- **16 Scenes IA**: Maison, robot, chateau, vaisseau, arbre, epee, et plus

## Telechargement

### Recommande — C++ (v2.0)
**[Télécharger OCP v2.0 (C++)](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v2.0/OCP-v2.0-CPP.zip)** — Exécutable Windows natif, 3 Mo, rapide. Extraire le zip et lancer `OCP.exe`.

### Python (v1.0)
**[Télécharger OCP.exe (v1.0 Python)](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v1.0/OCP.exe)** — Exécutable Windows, pas besoin d'installer Python.

## Demarrage Rapide

### Depuis l'exécutable (v2.0)
1. Télécharger `OCP-v2.0-CPP.zip` ci-dessus
2. Extraire le zip
3. Copier `libwinpthread-1.dll` dans le même dossier que `OCP.exe` (déjà inclus)
4. Double-cliquer sur `OCP.exe`

### Depuis la source (Python)
```bash
pip install -r requirements.txt
python main.py
```

### Depuis la source (C++)
```bash
# Nécessite MinGW-w64 (MSVCRT)
python cpp/build.py
```

## Stack Technique

### v2.0 (C++) — Recommande
- C++17 / MinGW-w64
- OpenGL 3.3 + Dear ImGui
- GLFW 3.4
- GLM (math)
- GLAD (OpenGL loader)

### v1.0 (Python)
- Python 3.10+
- PyQt6 (Interface)
- PyOpenGL (Rendu 3D)
- NumPy (Mathematiques)
- Pillow (Export Image)

## Auteur

Developpe par **KitariosWebStudio - KWS**

## Licence

MIT
