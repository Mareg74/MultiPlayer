# MultiPlayer (Windows)

VJ multi-player — Qt 6 / C++17 / FFmpeg — composition Program, sorties **NDI** / **Spout**, contrôle **OSC** / **MIDI**.

| Branche | Plateforme | Rôle |
|---------|------------|------|
| [`mac`](https://github.com/Mareg74/MultiPlayer/tree/mac) | macOS | défaut |
| [`windows`](https://github.com/Mareg74/MultiPlayer/tree/windows) | Windows | **cette branche** |

**App ID :** `com.mareg74.multiplayer` · **Version :** `0.1.0`

Checkout `windows` sur PC, `mac` sur Mac — pas de merge systématique entre les deux.

---

## Français

### Prérequis

- Visual Studio 2022 Build Tools (MSVC) + CMake + Ninja  
- **Qt 6** MSVC (ex. `C:\Qt\6.8.3\msvc2022_64`)  
- **FFmpeg** + **RtMidi** via `scripts\win-fetch-deps.ps1` → `C:\deps\`  
- **NDI Runtime/SDK** (optionnel) — headers sous `third_party/NDI/include`  
- **Spout** — header sous `third_party/Spout/include` ; placer `SpoutLibrary.dll` (voir script de build)

### Build

```powershell
git clone https://github.com/Mareg74/MultiPlayer.git
cd MultiPlayer
git checkout windows

powershell -ExecutionPolicy Bypass -File scripts\win-fetch-deps.ps1

# Adapter QTDIR / SRC / BUILD dans scripts\win-build.bat puis :
scripts\win-build.bat
```

### Utilisation rapide

1. Charger des clips via **Banque** ou **Load**.  
2. Composer sur le **Program**.  
3. **OUTPUT** = ON AIR → NDI / Spout (Paramètres → Sorties).  
4. OSC : [docs/OSC.md](docs/OSC.md) (UDP 7000 par défaut).

### Licence

**Tous droits réservés.** Aucune licence open source n’est publiée pour l’instant.

---

## English

### Prerequisites

VS 2022 Build Tools, CMake, Ninja, Qt 6 MSVC. Run `scripts\win-fetch-deps.ps1` for FFmpeg/RtMidi. Optional NDI runtime + Spout DLL.

### Build

```powershell
git clone https://github.com/Mareg74/MultiPlayer.git && cd MultiPlayer
git checkout windows
powershell -ExecutionPolicy Bypass -File scripts\win-fetch-deps.ps1
# Edit paths in scripts\win-build.bat, then:
scripts\win-build.bat
```

### License

**All rights reserved.** No open-source license at this time.
