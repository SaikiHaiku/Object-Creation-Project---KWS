# OCP - Object Creation Project

Logiciel de creation d'objets 3D et 2D avec edition manuelle et generation par IA.

## Fonctionnalites

- **Mode Manuel**: Editeur 3D complet avec OpenGL, 9 primitives, transformations, materiaux PBR
- **Mode IA**: Generer des scenes 3D complexes a partir de prompts textuels - 60+ generators avec deformation organique, 100% local
- **Editeur Pro**: Multi-selection, Copier/Coller, Grouper/Degrouper, Wireframe, X-Ray
- **Outils QWERTY**: G=Deplacer, E=Tourner, R=Dimensionner (style Blender)
- **Export**: Formats OBJ, STL, PNG
- **Undo/Redo**: Systeme d'annulation avec 50 niveaux
- **Save/Load**: Serialisation de scene complete
- **Gizmos**: Transform Move/Rotate/Scale dans le viewport
- **60+ Scenes IA**: Maison, robot, chateau, vaisseau, arbre, epee, avion, bateau, crane, cerveau, fleur, champignon, donut, bijoux, trone, campement, fountain, crystal, corail, papillon, chat, chien, oiseau, caverne, tente, et plus

## Telechargement

### Recommande — C++ (v2.2)
**[Telecharger OCP v2.2 - Installeur](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v2.2/OCP-v2.2-Installer.exe)** — Installeur Windows avec raccourcis et desinstallation. (~1.3 Mo)

**[Telecharger OCP v2.2 - Portable](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v2.2/OCP-v2.2-CPP.zip)** — Version portable sans installation. Extraire le zip et lancer `OCP.exe`.

### Ancien — C++ (v2.1)
**[Telecharger OCP v2.1 (C++)](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v2.1/OCP-v2.1-CPP.zip)** — Version precedente.

### Python (v1.0)
**[Telecharger OCP.exe (v1.0 Python)](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v1.0/OCP.exe)** — Executable Windows, pas besoin d'installer Python.

## Demarrage Rapide

### Depuis l'executable (v2.2)
1. Telecharger `OCP-v2.2-Installer.exe` ci-dessus
2. Lancer l'installeur et suivre les etapes
3. OCP sera accessible via le menu Demarrer

### Depuis la version portable (v2.2)
1. Telecharger `OCP-v2.2-CPP.zip` ci-dessus
2. Extraire le zip
3. Double-cliquer sur `OCP.exe` (`libwinpthread-1.dll` doit etre dans le meme dossier)

### Depuis la source (Python)
```bash
pip install -r requirements.txt
python main.py
```

### Depuis la source (C++)
```bash
# Necessite MinGW-w64 (MSVCRT) a C:\mingw64_msvcrt\mingw64\bin
python cpp/build.py
```

## Raccourcis Clavier

| Raccourci | Action |
|-----------|--------|
| Ctrl+Z | Annuler (Undo) |
| Ctrl+Y | Refaire (Redo) |
| Ctrl+C | Copier |
| Ctrl+V | Coller |
| Ctrl+D | Dupliquer |
| Ctrl+G | Grouper |
| Ctrl+Shift+G | Degrouper |
| Ctrl+S | Sauvegarder |
| Ctrl+O | Ouvrir scene |
| Ctrl+N | Nouvelle scene |
| Ctrl+A | Selectionner tout |
| G | Outil Deplacer (Move) |
| E | Outil Tourner (Rotate) |
| R | Outil Dimensionner (Scale) |
| W | Mode Wireframe |
| X | Mode X-Ray |
| F | Centrer sur selection |
| H | Console |
| Del | Supprimer selection |
| F1 | A propos |
| Molette milieu | Orbiter |
| Clic droit | Pan |
| Molette | Zoom |
| Ctrl+Clic | Multi-selection |

## Stack Technique

### v2.2 (C++) — Recommande
- C++17 / MinGW-w64
- OpenGL 3.3 + Dear ImGui
- GLFW 3.4
- GLM (math)
- GLAD (OpenGL loader)
- Smart pointers (zero memory leaks)
- 60+ generators avec deformation organique Perlin noise

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
