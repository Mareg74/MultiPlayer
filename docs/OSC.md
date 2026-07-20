# Contrôle OSC — MultiPlayer

Port UDP par défaut : **7000** (configurable dans Paramètres → OSC / MIDI).

Préfixe : `/mp`

## Sortie

| Adresse | Arguments | Effet |
|---------|-----------|--------|
| `/mp/output` | `i`/`f`/`T`/`F` (optionnel) | Active/désactive OUTPUT ON AIR. Sans argument : toggle. |
| `/mp/select` | `i` index module (0–13) | Sélectionne un module |
| `/mp/play` | — | Play tous les modules chargés |
| `/mp/pause` | — | Pause tous les modules |
| `/mp/stop` | — | Stop tous les modules |
| `/mp/toggle` | — | Play/Pause global |

## Modules (0–13, max 14)

| Adresse | Arguments | Effet |
|---------|-----------|--------|
| `/mp/module/{n}/play` | — | Lecture |
| `/mp/module/{n}/pause` | — | Pause |
| `/mp/module/{n}/stop` | — | Stop |
| `/mp/module/{n}/toggle` | — | Play/Pause |
| `/mp/module/{n}/load` | `s` chemin fichier | Charge un média |
| `/mp/module/{n}/seek` | `f` 0–1 | Seek normalisé |
| `/mp/module/{n}/opacity` | `f` 0–1 | Opacité |
| `/mp/module/{n}/scale` | `f` | Scale |
| `/mp/module/{n}/pos` | `f f` x y | Position canvas |
| `/mp/module/{n}/crop` | `f f f f` L T R B | Crop normalisé |
| `/mp/module/{n}/visible` | `f`/`i` | Visible si ≥ 0.5 |

## Exemples (oscsend / Max / TouchOSC)

```bash
# OUTPUT ON
oscsend localhost 7000 /mp/output i 1

# Play module 0
oscsend localhost 7000 /mp/module/0/play

# Opacity module 2
oscsend localhost 7000 /mp/module/2/opacity f 0.5
```

## MIDI

Actions learnables (panneau Contrôle) :

- `output_toggle`, `output`
- `play_all`, `pause_all`, `stop_all`, `toggle_all`
- `module.N.toggle`, `module.N.play`, `module.N.stop`
- `module.N.opacity`, `module.N.scale` (valeur CC 0–127 → 0–1)
