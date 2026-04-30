# Creation Theory Morph

> Professional 3D Mesh Viewer & Editor by [Creation Theory](https://creationtheory.co)

![Version](https://img.shields.io/badge/version-1.0.0-blue)
![License](https://img.shields.io/badge/license-Commercial-red)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey)
![Python](https://img.shields.io/badge/python-3.9%2B-yellow)

---

**Creation Theory Morph** is a professional 3D mesh viewing and editing application
built for designers, fabricators, and makers. Inspired by the LuBan workflow,
Morph gives you a powerful yet intuitive viewport for working with 3D geometry
destined for 3D printing, laser cutting, and CNC milling.

---

## Features

- **3D Viewport** — Real-time OpenGL rendering with rotate, pan, and zoom
- **Display Modes** — Wireframe, Solid, Solid + Wireframe overlay
- **File Import/Export** — STL, OBJ, PLY, OFF, GLB/GLTF
- **Batch Import** — Load an entire folder of mesh files at once
- **Mesh Tools** — Flip normals, merge vertices, mesh statistics
- **Object Browser** — Per-object visibility, naming, and management
- **Demo Objects** — Built-in cube, sphere, torus for quick testing
- **Dark Theme** — Professional dark UI with deep blue viewport

---

## Requirements

- Python 3.9 or higher
- pip packages (see below)

---

## Installation

### 1. Clone the repository

```bash
git clone https://github.com/creationtheory/morph.git
cd morph
```

### 2. Install dependencies

```bash
pip install -r requirements.txt
```

### 3. Run

```bash
python morph.py
```

---

## Build a Standalone Executable

```bash
pip install pyinstaller
pyinstaller --onefile --windowed --name "Creation Theory Morph" morph.py
```

The executable will be in the `dist/` folder.

---

## Controls

| Input | Action |
|---|---|
| Left drag | Rotate |
| Middle drag | Pan |
| Right drag / Scroll | Zoom |
| `R` | Reset view |
| `G` | Toggle grid |
| `A` | Toggle axes |

---

## License & Subscription

Creation Theory Morph is **commercial software**. A subscription of **$5/month**
is required to use this software. See [LICENSE.txt](LICENSE.txt) for full terms.

Purchase a subscription at **[creationtheory.co](https://creationtheory.co)**.

Unauthorized copying, distribution, or use of this software is strictly prohibited.

---

## Support

- Website: [creationtheory.co](https://creationtheory.co)
- Email: support@creationtheory.co

---

© 2026 Creation Theory. All Rights Reserved.
