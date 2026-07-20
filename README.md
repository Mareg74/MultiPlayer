# MultiPlayer

VJ multi-player desktop app — Qt 6 / C++17 / FFmpeg — composition Program, sorties **NDI** / **Spout** (Windows), contrôle **OSC** / **MIDI**.

**App ID:** `com.mareg74.multiplayer` · **Version:** `0.1.0`

| Branche | Plateforme |
|---------|------------|
| [`mac`](https://github.com/Mareg74/MultiPlayer/tree/mac) | macOS (branche par défaut) |
| [`windows`](https://github.com/Mareg74/MultiPlayer/tree/windows) | Windows |

Développement **séparé** par plateforme : pas de merge systématique entre `mac` et `windows`. Sur Mac → `git checkout mac`. Sur PC → `git checkout windows`.

---

## Français

### Prérequis

- **CMake** ≥ 3.21  
- **Qt 6** (Widgets, Network)  
- **FFmpeg** (libavformat, avcodec, avutil, swscale, swresample)  
- **NDI SDK** (optionnel, recommandé) — headers aussi fournis sous `third_party/NDI/include`  
- **RtMidi** (optionnel, pour MIDI)  
- **Spout** : Windows uniquement (`third_party/Spout`)

### Build — macOS (`mac`)

```bash
brew install cmake qt ffmpeg rtmidi
# NDI SDK Apple : https://ndi.video/tools/ → installer, ou exporter NDI_SDK_DIR

git clone https://github.com/Mareg74/MultiPlayer.git
cd MultiPlayer
git checkout mac

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
open build/MultiPlayer.app
```

### Build — Windows (`windows`)

```powershell
git clone https://github.com/Mareg74/MultiPlayer.git
cd MultiPlayer
git checkout windows

# Dépendances locales (FFmpeg + RtMidi sous C:\deps)
powershell -ExecutionPolicy Bypass -File scripts\win-fetch-deps.ps1

# Adapter les chemins dans scripts\win-build.bat (Qt, Visual Studio, SRC)
scripts\win-build.bat
```

Installer aussi le **runtime NDI** et placer `SpoutLibrary.dll` si besoin (voir script de build).

### Utilisation rapide

1. Charger des clips via **Banque** ou **Load** sur chaque module.  
2. Composer sur le **Program** (déplacer / scale ; Alt = sans snap).  
3. **OUTPUT** = ON AIR → envoi NDI / Spout selon Paramètres → Sorties.  
4. **Paramètres** : composition, sorties, OSC, MIDI.  

Contrôle OSC : voir [docs/OSC.md](docs/OSC.md) (port UDP 7000 par défaut).

### Licence

**Tous droits réservés.** Aucune licence open source n’est publiée pour l’instant.

---

## English

### Prerequisites

- **CMake** ≥ 3.21  
- **Qt 6** (Widgets, Network)  
- **FFmpeg** libraries  
- **NDI SDK** (optional; headers under `third_party/NDI/include`)  
- **RtMidi** (optional)  
- **Spout** on Windows only

### Build — macOS (`mac`)

```bash
brew install cmake qt ffmpeg rtmidi
git clone https://github.com/Mareg74/MultiPlayer.git && cd MultiPlayer
git checkout mac
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
open build/MultiPlayer.app
```

### Build — Windows (`windows`)

```powershell
git clone https://github.com/Mareg74/MultiPlayer.git && cd MultiPlayer
git checkout windows
powershell -ExecutionPolicy Bypass -File scripts\win-fetch-deps.ps1
# Edit paths in scripts\win-build.bat, then:
scripts\win-build.bat
```

### Quick start

Load media into modules, compose on **Program**, toggle **OUTPUT** for NDI/Spout. See [docs/OSC.md](docs/OSC.md) for OSC.

### License

**All rights reserved.** No open-source license is published at this time.
