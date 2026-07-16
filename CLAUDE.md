# AURA — Contexte Projet pour Claude Code

## Objectif du projet

AURA (Abstraction de Contrôle par Vision Artificielle) est un **framework C++** qui transforme le flux vidéo d'une webcam en commandes système en temps réel. But : remplacer clavier/souris par la **reconnaissance gestuelle des mains** — utilisable dans des jeux, applis, scripts, etc.

Pipeline cible : `Capture vidéo → Traitement image → Analyse géométrique → Simulation d'input système`

## Architecture cible (CDC §2) — Producteur-Consommateur découplé

```
[AURA Core]  → génère des GestureEvent abstraits (GESTURE_PINCH, GESTURE_CLICK, etc.)
     ↓
[AURA Mapper] → lit un fichier JSON/config et lie geste → action (sans recompiler)
     ↓
[AURA Client] → consomme les événements via une EventQueue thread-safe
```

**Principe clé :** le Core ne connaît pas la souris/clavier. Le Mapper fait le pont. Le Client (jeu, script) consomme. Cette séparation rend l'outil réutilisable pour n'importe quel usage.

## Stack technique (CDC §3)

| Composant       | Technologie                                  |
|-----------------|----------------------------------------------|
| Langage         | **C++20** (CMakeLists a C++17 — à corriger)  |
| Vision          | **OpenCV 4.x** + **MediaPipe** (non intégré) |
| Input système   | **X11/Xtst** (Linux), **CGEvent** (macOS)    |
| Demo app        | **SFML** (non commencé)                      |
| Build           | **CMake** modulaire                          |

## État actuel du code (juin 2026)

### CE QUI EXISTE ET FONCTIONNE

| Fichier | Rôle | État |
|---------|------|------|
| `src/Vision/Camera.cpp` | Capture, trackbars HSV, calibration guidée/auto | Fonctionne, trop monolithique |
| `src/Vision/HandTracker.cpp` | Détection contour + hull convexe + Kalman | Fonctionne, basique |
| `src/Core/KalmanFilter.cpp` | Lissage 2D position (predict/update) | Propre, réutilisable |
| `src/Input/Controller.cpp` | moveMouse + click (Linux XTest + macOS CGEvent) | Fonctionne sur les 2 plateformes |
| `src/main.cpp` | Boucle principale, flags CLI | Trop monolithique |

**Calibration :** guidée (3 poses) + auto (YCrCb + fallback motion) + save/load preset. **Problème :** sauve dans `build/` → perdu au `make clean`. À déplacer vers `~/.aura/` ou `config/`.

**Seul geste détecté :** open palm → left click. Détection via `convexityDefects` (≥3 défauts > 20px).

### CE QUI MANQUE (vs CDC)

#### Architecture (bloquant pour la réutilisabilité)
- [ ] `GestureEvent` — enum/struct abstrait (GESTURE_PINCH, GESTURE_CLICK, GESTURE_SCROLL_UP, etc.)
- [ ] `EventQueue<GestureEvent>` — file thread-safe (std::queue + mutex + cv)
- [ ] **AURA Mapper** — fichier JSON qui lie un geste à une action (ex: `"GESTURE_PINCH": "LEFT_CLICK"`)
- [ ] Interface `IAuraClient` — pour découpler le consommateur

#### Vision
- [ ] **MediaPipe** non intégré — on est en HSV couleur peau seulement (fragile selon lumière)
- [ ] `GestureDetector` — couche d'interprétation entre HandTracker (géométrie brute) et GestureEvent
- [ ] Comptage des doigts levés (finger count → gesture mapping)
- [ ] Pinch (pouce + index proches)
- [ ] Swipe (vecteur de déplacement rapide)
- [ ] Circle (trajectoire circulaire)

#### Input
- [ ] Simulation clavier (XTest KeySym / CGKeyCode)
- [ ] Simulation scroll (molette — X11 button 4/5, macOS CGScrollWheelEvent)
- [ ] Drag & release
- [ ] Double-click

#### Calibration (ton idée : premier lancement)
- [ ] Check au démarrage si `~/.aura/calib_default.txt` existe → si non, lancer wizard automatiquement
- [ ] Stocker les calibrations dans `~/.aura/` (pas dans `build/`)
- [ ] `CalibConfig` class — gère les chemins, lecture/écriture indépendamment de Camera

#### Demo / Tests
- [ ] App SFML de démo (DemoGame)
- [ ] Tests unitaires (au minimum KalmanFilter et GestureDetector)

## Problèmes structurels à corriger dans le refactor

1. **`Camera` fait trop de choses** : capture + UI (trackbars/windows) + calibration → à séparer
2. **`HSVRange` dans `Camera.hpp`** mais utilisé par `HandTracker` → créer `include/Aura/Vision/Types.hpp`
3. **Calibration path hardcodé `build/`** → `~/.aura/calib_<name>.txt`
4. **Pas de `GestureEvent`** → tout le raisonnement gestuel est dans main.cpp avec des `bool`
5. **C++17 dans CMakeLists** au lieu de C++20 (CDC §3)
6. **Pas de séparation Vision / Gesture** : HandTracker fait détection ET interprétation

## Structure cible pour le refactor

```
include/Aura/
├── Core/
│   ├── KalmanFilter.hpp     ✓ OK
│   ├── GestureEvent.hpp     ← à créer (enum GestureType + struct GestureEvent)
│   └── EventQueue.hpp       ← à créer (template thread-safe queue)
├── Vision/
│   ├── Types.hpp            ← à créer (HSVRange, CalibData, DetectionResult)
│   ├── Camera.hpp           ← à simplifier (capture seule, sans UI)
│   ├── Calibrator.hpp       ← à extraire de Camera (guidée + auto + save/load)
│   ├── HandTracker.hpp      ✓ à garder, + intégration MediaPipe future
│   └── GestureDetector.hpp  ← à créer (contour → GestureEvent)
├── Input/
│   ├── Controller.hpp       ✓ à étendre (keyboard + scroll)
│   └── Mapper.hpp           ← à créer (JSON config → gesture/action binding)
└── Config/
    └── CalibConfig.hpp      ← à créer (gestion ~/.aura/)

src/ — mirrors include/
config/
└── default_mapping.json     ← à créer (mapping par défaut)
demo/                        ← à créer (SFML DemoGame)
tests/                       ← à créer
```

## Calibration au premier lancement

Logique à implémenter dans `main.cpp` ou un `AppRunner` :

```cpp
CalibConfig cfg;
if (!cfg.defaultCalibrationExists()) {
    // Premier lancement : wizard obligatoire
    Calibrator calib(camera);
    calib.runGuidedWizard("default");  // sauve dans ~/.aura/calib_default.txt
}
camera.loadCalibration("default");  // toujours charger depuis ~/.aura/
```

## Fichier de mapping (AURA Mapper) — concept cible

```json
// ~/.aura/mapping.json ou config/default_mapping.json
{
  "GESTURE_OPEN_PALM": { "action": "MOUSE_CLICK", "button": "LEFT" },
  "GESTURE_PINCH":     { "action": "MOUSE_CLICK", "button": "LEFT" },
  "GESTURE_FIST":      { "action": "KEY_PRESS", "key": "SPACE" },
  "GESTURE_TWO_FINGERS_UP": { "action": "MOUSE_SCROLL", "direction": "UP", "amount": 3 },
  "GESTURE_SWIPE_LEFT": { "action": "KEY_PRESS", "key": "LEFT" }
}
```

## Commandes build

```bash
./scripts/build.sh          # cmake + make dans build/
./scripts/aura.sh           # lance le binaire
./aura --help
./aura --save-calib myhand  # calibration guidée + sauvegarde
./aura --load-calib myhand  # charge preset
./aura --auto-calibrate     # calibration auto (YCrCb)
./aura --no-input --verbose # debug sans simulation souris
```

## Règles de développement

- C++20 minimum (std::concepts, std::span si utile)
- Pas de dépendances header-only non justifiées
- Chaque module doit compiler et être testable indépendamment
- La calibration ne doit jamais bloquer le démarrage (fallback valeurs par défaut HSV)
- 30 FPS minimum en production (profiler avant d'ajouter des traitements)
- Les fichiers de calibration vont dans `~/.aura/`, jamais dans `build/`
