# 🌟 Aura

![Build](https://img.shields.io/badge/build-passing-brightgreen)
![License](https://img.shields.io/badge/license-MIT-blue)
![C++](https://img.shields.io/badge/C++-20-orange)

**Aura** est un framework de contrôle gestuel en C++ utilisant la vision par ordinateur. Il transforme le flux d'une webcam en commandes système (clavier/souris) pour piloter n'importe quel jeu ou application sous Linux.

---

## 🚀 Fonctionnalités
- **Capture temps réel** : Flux vidéo optimisé et threadé.
- **Traitement d'image** : Isolation via conversion HSV et filtrage morphologique.
- **Analyse de forme** : Détection d'enveloppe convexe (Convex Hull) pour reconnaître les gestes.
- **Contrôle système** : Simulation d'inputs via **X11/Xtst** (compatibilité universelle).
- **Lissage** : Implémentation d'un **Filtre de Kalman** pour une précision accrue.

## 📂 Structure du projet
```text
Aura/
├── CMakeLists.txt      # Configuration du build
├── include/            # Headers (.hpp)
├── src/                # Sources (.cpp)
├── scripts/            # Scripts de build et lancement
└── assets/             # Ressources et config
```

## 🛠️ Prérequis
- **Compilateur** : g++ (>=13 recommandé)
- **Outils** : CMake
- **Librairies** : OpenCV 4.x, X11, Xtst

**Installation rapide (Ubuntu/Debian) :**
```bash
sudo apt update && sudo apt install libopencv-dev build-essential cmake libx11-dev libxtst-dev
```

## 🔨 Compilation & Exécution
Le projet utilise des scripts simplifiés à la racine :
```bash
./scripts/build.sh          # Compiler le projet
./scripts/build.sh fclean    # Nettoyer tout le projet
./scripts/aura.sh           # Lancer l'application (fix LD_PRELOAD)
```

## 📝 Notes
Si vous utilisez VS Code (version Snap), utilisez impérativement ```./scripts/aura.sh``` pour éviter les conflits de bibliothèques système.

## ⚖️ Licence
Distribué sous licence MIT.

## 👤 Auteur
**Angel SEVERAN** (@Angel-42)
