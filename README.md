# OCP - Object Creation Project

Logiciel de creation d'objets 3D et 2D avec edition manuelle et generation par IA.

## Fonctionnalites

- **Mode Manuel**: Editeur 3D complet avec OpenGL, 9 primitives, transformations, materiaux PBR
- **Mode IA**: Generer des scenes 3D complexes a partir de prompts textuels - 35+ generators, 100% local
- **Export**: Formats OBJ, STL, PNG
- **Undo/Redo**: Systeme d'annulation avec 50 niveaux
- **Save/Load**: Serialisation de scene complete
- **Gizmos**: Transform Move/Rotate/Scale dans le viewport
- **Raccourcis clavier**: Ctrl+Z/Y, F, G, H, Delete, Ctrl+S/O/N/D/A
- **35+ Scenes IA**: Maison, robot, chateau, vaisseau, arbre, epee, avion, bateau, crâne, cerveau, fleur, champignon, donut, bijoux, et plus

## Telechargement

### Recommande — C++ (v2.1)
**[Télécharger OCP v2.1 (C++)](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v2.1/OCP-v2.1-CPP.zip)** — Exécutable Windows natif, 4 Mo. Extraire le zip et lancer `OCP.exe`.

### Ancien — C++ (v2.0)
**[Télécharger OCP v2.0 (C++)](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v2.0/OCP-v2.0-CPP.zip)** — Version precedente.

### Python (v1.0)
**[Télécharger OCP.exe (v1.0 Python)](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v1.0/OCP.exe)** — Exécutable Windows, pas besoin d'installer Python.

## Demarrage Rapide

### Depuis l'exécutable (v2.1)
1. Télécharger `OCP-v2.1-CPP.zip` ci-dessus
2. Extraire le zip
3. Double-cliquer sur `OCP.exe` (`libwinpthread-1.dll` doit être dans le même dossier)

### Depuis la source (Python)
```bash
pip install -r requirements.txt
python main.py
```

### Depuis la source (C++)
```bash
# Nécessite MinGW-w64 (MSVCRT) a C:\mingw64_msvcrt\mingw64\bin
python cpp/build.py
```

## Raccourcis Clavier

| Raccourci | Action |
|-----------|--------|
| Ctrl+Z | Annuler (Undo) |
| Ctrl+Y | Refaire (Redo) |
| Ctrl+D | Dupliquer |
| Ctrl+S | Sauvegarder |
| Ctrl+O | Ouvrir scene |
| Ctrl+N | Nouvelle scene |
| Ctrl+A | Selectionner tout |
| Ctrl+1/2/3 | Outil Move/Rotate/Scale |
| F | Centrer sur selection |
| G | Afficher/Masquer grille |
| H | Console |
| Del | Supprimer selection |
| F1 | A propos |
| Molette milieu | Orbiter |
| Clic droit | Pan |
| Molette | Zoom |

## Stack Technique

### v2.1 (C++) — Recommande
- C++17 / MinGW-w64
- OpenGL 3.3 + Dear ImGui
- GLFW 3.4
- GLM (math)
- GLAD (OpenGL loader)
- Smart pointers (zero memory leaks)

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
