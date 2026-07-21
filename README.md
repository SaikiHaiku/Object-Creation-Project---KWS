# OCP - Object Creation Project

Logiciel de creation d'objets 3D et 2D avec edition manuelle et generation par IA.

## Fonctionnalites

- **Mode Manuel**: Editeur 3D complet avec OpenGL, primitives, transformations, materiaux
- **Mode IA**: Generer des scenes 3D complexes a partir de prompts textuels - 100% local
- **Export**: Formats OBJ, STL, SVG, PNG
- **Materiaux**: Materiaux PBR avec metallic, roughness, shininess
- **16 Scenes IA**: Maison, robot, chateau, vaisseau, arbre, epee, et plus

## Telechargement

**[Télécharger OCP.exe (v1.0)](https://github.com/SaikiHaiku/Object-Creation-Project---KWS/releases/download/v1.0/OCP.exe)** — Exécutable Windows, pas besoin d'installer Python.

## Demarrage Rapide

### Depuis l'exécutable
Télécharger `OCP.exe` ci-dessus et double-cliquer pour lancer.

### Depuis la source
```bash
pip install -r requirements.txt
python main.py
```

## Installation

Voir `website/index.html` pour le guide d'installation detaille.

## Build Executable

```bash
python build.py
```

## Stack Technique

- Python 3.10+
- PyQt6 (Interface)
- PyOpenGL (Rendu 3D)
- NumPy (Mathematiques)
- Pillow (Export Image)

## Auteur

Developpe par **KitariosWebStudio - KWS**

## Licence

MIT
