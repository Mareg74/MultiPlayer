# MultiPlayer

VJ multi-player — Qt 6 / C++17 / FFmpeg — composition Program, sortie **NDI**, contrôle **OSC** / **MIDI**. Sur Windows : aussi **Spout**.

| Branche | Plateforme | Rôle |
|---------|------------|------|
| [`mac`](https://github.com/Mareg74/MultiPlayer/tree/mac) | macOS | **défaut** |
| [`windows`](https://github.com/Mareg74/MultiPlayer/tree/windows) | Windows | deps / scripts séparés |

**App ID :** `com.mareg74.multiplayer` · **Version :** `0.1.0`

Checkout `mac` sur Mac, `windows` sur PC — pas de merge systématique entre les deux.

---

## Français

### Prérequis (macOS)

- **CMake** ≥ 3.21  
- **Qt 6** (Widgets, Network) — Homebrew  
- **FFmpeg** — Homebrew  
- **RtMidi** — Homebrew  
- **NDI SDK for Apple** (optionnel) — headers aussi sous `third_party/NDI/include`

### Build (macOS)

```bash
brew install cmake qt ffmpeg rtmidi

git clone https://github.com/Mareg74/MultiPlayer.git
cd MultiPlayer
git checkout mac

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
open build/MultiPlayer.app
```

### Build (Windows)

Sur la branche [`windows`](https://github.com/Mareg74/MultiPlayer/tree/windows) :

```powershell
git checkout windows
powershell -ExecutionPolicy Bypass -File scripts\win-fetch-deps.ps1
scripts\win-build.bat
```

Voir le README de cette branche pour Qt MSVC, Spout et `C:\deps\`.

### Utilisation rapide

1. Charger des clips via **Banque** ou **Load**.  
2. Composer sur le **Program**.  
3. **OUTPUT** = ON AIR → envoi NDI (Paramètres → Sorties).  
4. OSC : [docs/OSC.md](docs/OSC.md) (UDP 7000 par défaut).

### Licence

**Tous droits réservés / All rights reserved.** Aucune licence open source n’est publiée pour l’instant.

---

## English

### Prerequisites (macOS)

CMake ≥ 3.21, Qt 6, FFmpeg, RtMidi (Homebrew). Optional NDI SDK for Apple.

### Build (macOS)

```bash
brew install cmake qt ffmpeg rtmidi
git clone https://github.com/Mareg74/MultiPlayer.git && cd MultiPlayer
git checkout mac
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
open build/MultiPlayer.app
```

### Build (Windows)

On the [`windows`](https://github.com/Mareg74/MultiPlayer/tree/windows) branch: `scripts\win-fetch-deps.ps1` then `scripts\win-build.bat`.

### Quick start

Load clips → compose on Program → OUTPUT on-air for NDI. OSC: [docs/OSC.md](docs/OSC.md) (UDP 7000).

### License

**All rights reserved.** No open-source license at this time.
