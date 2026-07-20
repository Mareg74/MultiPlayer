# MultiPlayer (macOS)

VJ multi-player — Qt 6 / C++17 / FFmpeg — composition Program, sortie **NDI**, contrôle **OSC** / **MIDI**.

**Branche :** [`mac`](https://github.com/Mareg74/MultiPlayer/tree/mac) (défaut)  
**App ID :** `com.mareg74.multiplayer` · **Version :** `0.1.0`

> Développement Windows → branche [`windows`](https://github.com/Mareg74/MultiPlayer/tree/windows) (code et deps séparés).

---

## Français

### Prérequis

- **CMake** ≥ 3.21  
- **Qt 6** (Widgets, Network) — Homebrew  
- **FFmpeg** — Homebrew  
- **RtMidi** — Homebrew  
- **NDI SDK for Apple** (optionnel) — headers aussi sous `third_party/NDI/include`

### Build

```bash
brew install cmake qt ffmpeg rtmidi

git clone https://github.com/Mareg74/MultiPlayer.git
cd MultiPlayer
git checkout mac

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
open build/MultiPlayer.app
```

### Utilisation rapide

1. Charger des clips via **Banque** ou **Load**.  
2. Composer sur le **Program**.  
3. **OUTPUT** = ON AIR → envoi NDI (Paramètres → Sorties).  
4. OSC : [docs/OSC.md](docs/OSC.md) (UDP 7000 par défaut).

### Licence

**Tous droits réservés.** Aucune licence open source n’est publiée pour l’instant.

---

## English

### Prerequisites

CMake ≥ 3.21, Qt 6, FFmpeg, RtMidi (Homebrew). Optional NDI SDK for Apple.

### Build

```bash
brew install cmake qt ffmpeg rtmidi
git clone https://github.com/Mareg74/MultiPlayer.git && cd MultiPlayer
git checkout mac
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
open build/MultiPlayer.app
```

### License

**All rights reserved.** No open-source license at this time.
