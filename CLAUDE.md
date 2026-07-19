# AURA — Contexte Projet pour Claude Code

## Objectif du projet

AURA (Abstraction de Contrôle par Vision Artificielle) est un **framework C++20** qui transforme le flux vidéo d'une webcam en commandes système en temps réel. But : remplacer clavier/souris par la **reconnaissance gestuelle des mains** — utilisable dans des jeux, applis, scripts, etc.

Pipeline cible : `Capture vidéo → Détection landmarks → GestureDetector → GestureEvent → Mapper → Controller`

## Architecture — Producteur-Consommateur découplé

```
[Camera / HandTracker]  → DetectionResult (landmarks normalisés, position, aire)
         ↓
[GestureDetector]       → GestureEvent {type, position, side(LEFT|RIGHT)}
         ↓
[ActivationGuard]       → filtre les faux positifs (main stable avant d'agir)
         ↓
[Mapper]                → lit config/default_mapping.txt, lie geste → Action
         ↓
[Controller]            → moveMouse / click / scroll / keyPress / keyCombo
```

**Principe clé :** le Core ne connaît pas la souris/clavier. Le Mapper fait le pont. La séparation rend l'outil réutilisable pour n'importe quel usage.

## Stack technique

| Composant       | Technologie                                           |
|-----------------|-------------------------------------------------------|
| Langage         | **C++20**                                             |
| Vision          | **OpenCV 4.x** + **MediaPipe Tasks** (bridge Python)  |
| Input système   | **X11/Xtst** (Linux), **CGEvent** (macOS)             |
| Demo app        | **SFML** (non commencé)                               |
| Build           | **CMake** modulaire (`aura_lib` + `aura` executable)  |
| Tests           | **GTest** (27 tests, CTest)                           |
| CI              | **GitHub Actions** (build+test / debug / clang-format)|

## État actuel du code (juillet 2026)

### CE QUI EXISTE ET FONCTIONNE

| Module | Fichiers | État |
|--------|----------|------|
| `Core` | `KalmanFilter`, `GestureEvent` (18 types), `EventQueue` | Propre, C++20 |
| `Vision` | `Camera`, `HandTracker`, `Calibrator`, `GestureDetector` | Fonctionne — HSV/contours + Kalman |
| `Input` | `Controller` (moveMouse+click+scroll+doubleClick), `Mapper` | Linux + macOS |
| `App` | `AuraRunner` (pipeline complet), `ActivationGuard` (3 états) | OK |
| `Config` | `CalibConfig` | Chemins `~/.aura/` gérés |
| `Tests` | `test_kalman`, `test_gesture_detector`, `test_mapper` | 27/27 passent |
| `CI` | `.github/workflows/ci.yml` | build+test / warnings / lint |

### Gestes reconnus (GestureType enum — 18 types)

```
Navigation  : OPEN_PALM, POINT, NONE
Clics       : PINCH (index), PINCH_MIDDLE, PINCH_RING, PINCH_PINKY,
              PINCH_DOUBLE, PINCH_SIDE, ZTAP
Actions main: FIST (drag 20 frames), TWO_FINGERS (scroll continu),
              THREE_FINGERS, FOUR_FINGERS
Swipes      : SWIPE_LEFT, SWIPE_RIGHT, SWIPE_UP, SWIPE_DOWN
```

### Mapping actuel (`config/default_mapping.txt`)

```
OPEN_PALM     = NONE           # navigation souris
POINT         = NONE           # navigation souris
PINCH         = MOUSE_CLICK LEFT
PINCH_MIDDLE  = MOUSE_CLICK RIGHT
PINCH_DOUBLE  = MOUSE_DOUBLE_CLICK LEFT
ZTAP          = MOUSE_CLICK LEFT
SWIPE_UP      = SCROLL UP 5
SWIPE_DOWN    = SCROLL DOWN 5
SWIPE_LEFT    = KEY_COMBO ALT LEFT
SWIPE_RIGHT   = KEY_COMBO ALT RIGHT
```

### Cursor mode

- **Relatif** (défaut) : `virtualCursor_` accumulé avec deadzone + speed multiplier
- **Absolu** : `--absolute` flag en CLI
- **Freeze** : curseur gelé pendant les gestes d'action (seuls OPEN_PALM/POINT/NONE bougent)
- **Mirror** : X inversé (`frameW - 1 - frameX`) pour que main droite = curseur droite

---

## CE QUI MANQUE (priorité décroissante)

### 1. Simulation clavier (bloquant — bindings non exécutés)

`Controller` n'implémente pas encore `keyPress` / `keyCombo`. Les bindings
`SWIPE_LEFT = KEY_COMBO ALT LEFT` sont parsés par le Mapper mais ignorés à l'exécution.

À implémenter dans `src/Input/Controller.cpp` :
- Linux : `XTestFakeKeyEvent` avec `XKeysymToKeycode`
- macOS : `CGEventCreateKeyboardEvent` + `CGEventSetFlags` pour les modifiers
- Modifiers supportés : CTRL, SHIFT, ALT, META

### 2. MediaPipe (critique pour la fiabilité)

Actuellement : HSV couleur peau — fragile selon lumière/fond.  
`scripts/hand_bridge.py` + `models/` existent mais ne sont pas intégrés dans AuraRunner.

Plan d'intégration :
- `AuraRunner` lance `hand_bridge.py` en subprocess (popen)
- Lit les landmarks JSON sur stdout (21 points normalisés par main)
- Remplace `HandTracker` comme source de `DetectionResult`
- Fallback HSV si le bridge échoue ou n'est pas disponible

### 3. Support deux mains simultanées ← NOUVELLE EXIGENCE

**Objectif :** utiliser les deux mains en parallèle — navigation + actions indépendantes,
et à terme, langage des signes (combinaisons bimanuelles).

#### Changements architecturaux nécessaires

**`HandSide` dans `GestureEvent.hpp` :**
```cpp
enum class HandSide { UNKNOWN, LEFT, RIGHT };

struct GestureEvent {
    GestureType type;
    HandSide    side;       // ← nouveau
    cv::Point2f position;
    float       confidence;
};
```

**`DetectionResult` dans `Types.hpp` :**
```cpp
struct DetectionResult {
    bool        found;
    HandSide    side;       // ← nouveau (MediaPipe donne la latéralité)
    LandmarkData landmarks;
    cv::Point2f smoothedPoint;
    float       area;
};
```

**`AuraRunner` — deux machines d'état indépendantes :**
```cpp
// AuraRunner traite un vecteur de résultats (0, 1 ou 2 mains)
void processFrame(const std::vector<DetectionResult>& hands, cv::Mat& frame);

// État séparé par main
struct HandState {
    bool isDragging = false;
    int  fistFrameCount = 0;
    bool twoFingerScrollActive = false;
    int  twoFingerHoldFrames = 0;
    float lastScrollPosY = 0.f;
    float scrollAccumulator = 0.f;
    cv::Point2f virtualCursor;
    ActivationGuard guard;
};
std::array<HandState, 2> handStates_; // [0]=LEFT, [1]=RIGHT
```

**Threading :** MediaPipe retourne les 2 mains dans un seul appel d'inférence.
Le traitement des deux `HandState` peut se faire séquentiellement (simple)
ou en parallèle avec `std::thread` / `std::async` (gain marginal, à faire après que
le mode mono-main soit validé en prod).

**Mapper — préfixe de main optionnel :**
```
# Sans préfixe = s'applique aux deux mains
PINCH         = MOUSE_CLICK LEFT

# Avec préfixe = spécifique à une main
LEFT_OPEN_PALM  = MOUSE_MOVE      # main gauche = déplace souris
RIGHT_PINCH     = MOUSE_CLICK LEFT
RIGHT_FIST      = KEY_PRESS SPACE
```

**Couche combinatoire (Phase ultérieure — langage des signes) :**
```cpp
// DualGestureEvent — déclenché quand les deux mains ont un geste actif simultanément
struct DualGestureEvent {
    GestureType left;
    GestureType right;
};
// config/dual_mapping.txt :
// FIST + OPEN_PALM = KEY_PRESS ENTER
// TWO_FINGERS + TWO_FINGERS = KEY_COMBO CTRL Z
```

### 4. Auto-switch de profil selon l'application active

Détecter la fenêtre au premier plan et recharger le bon profil automatiquement.

Fichier de config `~/.aura/auto_profile.txt` :
```
firefox   = browser
minecraft = gaming
code      = default
```

Implémentation : thread de surveillance dans AuraRunner + hot-reload du Mapper.
- Linux : `xdotool getactivewindow getwindowname`
- macOS : AppleScript / `NSWorkspace.frontmostApplication`

**À faire après** avoir configuré plusieurs profils en conditions réelles.

### 5. Live rebinding (dépend de SFML)

Appui sur un bouton dans la démo → geste → bind immédiat dans le profil actif.
Nécessite la démo SFML comme surface d'affichage.

### 5. Refactor bindings — intuitivité et fluidité

Le système actuel (cooldown fixe, pas de feedback progressif) n'est pas assez fluide.
À retravailler après tests en conditions réelles : temps de maintien graduel, enchaînement
de gestes naturel, feedback visuel progressif. **À faire après la démo SFML** qui servira
de terrain de test.

### 6. Classifieur ML custom (remplacement de GestureDetector)

Entraîner un classificateur sur les landmarks MediaPipe plutôt que des règles géométriques.

Pipeline :
```
MediaPipe → 63 features (21 pts × xyz) → MLP 3 couches → GestureType
```

Étapes :
1. Script Python de collecte : enregistre les 63 features + label pour chaque frame
2. Entraînement (scikit-learn ou PyTorch, CPU, ~30min)
3. Export ONNX → chargement C++ via ONNX Runtime
4. Remplacement de `src/Vision/GestureDetector.cpp` par `MLGestureDetector`

Avantages : personnalisé à ta main/lumière, plus robuste, montre la compréhension du sujet.
Ce modèle est le "head" sur le backbone MediaPipe — exactement comme ResNet+tête custom.

### 7. C++ pur sans bridge Python (facultatif)

Objectif : supprimer `hand_bridge.py` et la dépendance Python.
Options (par ordre de réalisme) :
- **MediaPipe C Tasks API** — API C propre, binaires pré-compilés à trouver
- **ONNX Runtime C++** — convertir `.task` → ONNX, API CMake-friendly
- **TFLite C++ direct** — extraire le `.tflite` du `.task` et le loader manuellement

Note : le bridge coûte ~2ms/frame de latence IPC — pas le goulot d'étranglement.
À faire pour la propreté de l'archi / distribution sans dépendance Python, pas pour les perfs.

### 8. Distribution — macOS .app bundle

Objectif : une appli `AURA.app` qu'on glisse dans Applications, rien à installer.

Contenu du bundle :
- Binaire `aura` avec OpenCV dylibs bundlées (`dylibbundler`)
- `hand_bridge` standalone compilé avec PyInstaller (élimine la dépendance Python)
- `models/hand_landmarker.task` + `config/` + profils par défaut

Étapes :
1. `pyinstaller --onefile scripts/hand_bridge.py` → `hand_bridge` binaire
2. `dylibbundler` pour copier + patcher les dylibs OpenCV dans `AURA.app/Contents/Frameworks/`
3. Adapter `HandTracker` pour chercher `hand_bridge` dans `../Resources/` (bundle path)
4. `Info.plist` minimal, icône optionnelle
5. Optionnel : signature `codesign` pour éviter le warning Gatekeeper

**À faire après que le moteur soit stable.**

### 9. Tests additionnels

- Test d'intégration AuraRunner (pipeline frame-par-frame mocké)
- Tests Controller keyboard (Linux XTest / macOS CGEvent)
- Tests DualGestureEvent combinator (quand implémenté)

### 6. Demo SFML

App visuelle pour valider le pipeline end-to-end sans dépendre d'une webcam physique.
À faire en dernier, après que le moteur soit stable.

---

## Commandes build

```bash
./scripts/build.sh                        # cmake + make dans build/
./scripts/aura.sh                         # lance le binaire
./aura --help
./aura --speed 2.0 --deadzone 0.03        # cursor relatif paramétré
./aura --absolute                         # cursor absolu
./aura --profile gaming                   # (futur) charge ~/.aura/profiles/gaming.txt
./aura --save-calib myhand                # calibration guidée + sauvegarde
./aura --load-calib myhand                # charge preset
./aura --auto-calibrate                   # calibration auto (YCrCb)
./aura --no-input --verbose               # debug sans simulation souris

# Tests
cd build_test && ctest --output-on-failure
```

## Règles de développement

- C++20 minimum (std::concepts, std::span si utile)
- Pas de dépendances header-only non justifiées
- Chaque module compile et est testable indépendamment
- La calibration ne bloque jamais le démarrage (fallback HSV par défaut)
- 30 FPS minimum en production — profiler avant d'ajouter des traitements
- Les fichiers utilisateur vont dans `~/.aura/`, jamais dans `build/`
- Commits : format conventionnel `feat/fix/build/test/docs(): description`
- Auteur des commits : Angel — **pas de co-auteur**
