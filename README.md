# 🌟 Aura

![Build](https://img.shields.io/badge/build-passing-brightgreen)
![License](https://img.shields.io/badge/license-MIT-blue)
![C++](https://img.shields.io/badge/C%2B%2B-17-orange)

**Aura** est pour l'instant un outil de calibration HSV et de capture webcam en OpenCV.
Il prépare une base pour un framework de traitement de geste et de commande système.

---

## 🚀 Fonctionnalités actuelles
- **Calibration HSV** : ajustez les plages de teinte, saturation et valeur en temps réel.
- **Détection de main** : segmentation de la main sur le masque HSV, plus le plus grand contour.
- **Analyse de forme** : calcul de l’enveloppe convexe (convex hull) et détection d’open palm.
- **Lissage** : suivi de la position du centre de masse via un filtre de Kalman 2D.
- **Contrôle système** : simulation de la souris avec X11/XTest sur Linux, et support de base du pointeur via ApplicationServices sur macOS.

## 📂 Structure du projet
```text
Aura/
├── CMakeLists.txt
├── include/
│   └── Aura/
│       ├── Core/
│       │   └── KalmanFilter.hpp
│       ├── Input/
│       │   └── Controller.hpp
│       └── Vision/
│           ├── Camera.hpp
│           └── HandTracker.hpp
├── scripts/
│   ├── aura.sh
│   └── build.sh
└── src/
    ├── main.cpp
    ├── Core/
    │   └── KalmanFilter.cpp
    ├── Input/
    │   └── Controller.cpp
    └── Vision/
        ├── Camera.cpp
        └── HandTracker.cpp
```

## 🛠️ Prérequis
- **Outils** : CMake
- **Librairies** : OpenCV 4.x
- **Système** : macOS ou Linux

### Installation recommandée
- macOS (Homebrew) :
```bash
brew install cmake opencv
```
- Ubuntu/Debian :
```bash
sudo apt update && sudo apt install libopencv-dev build-essential cmake
```

## 🔨 Compilation & Exécution
```bash
./scripts/build.sh
./aura
```

### Options
- `--no-input` : désactive la simulation de souris même si l’API est disponible
- `--verbose` : affiche le statut de détection en temps réel
- `--help` : affiche l’aide

Si CMake ne trouve pas OpenCV automatiquement, exportez le chemin :
```bash
export OpenCV_DIR="/opt/homebrew/opt/opencv/lib/cmake/opencv4"
./scripts/build.sh
```

## 📝 Notes
- La version actuelle est un prototype de calibration HSV.
- L’architecture est prête pour ajouter une détection de contours, le traitement de gestes et l’émission de commandes.

## ⚖️ Licence
Distribué sous licence MIT.

## 👤 Auteur
**Angel SEVERAN** (@Angel-42)
