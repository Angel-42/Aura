
![Build](https://img.shields.io/badge/build-passing-brightgreen)
![License](https://img.shields.io/badge/license-MIT-blue)

# Aura

Aura est une application C++ utilisant OpenCV pour la capture et le traitement vidéo, conçue pour être simple à compiler et à exécuter sous Linux. Le but ? S'amuser avec la manipulation vidéo en C++ sur des jeux video.

## Structure du projet

```
Aura/
├── CMakeLists.txt
├── include/
│   └── Camera.hpp
├── src/
│   ├── main.cpp
│   └── Camera.cpp
├── build/           # Dossier de build généré automatiquement
├── scripts/
│   ├── build.sh     # Script de compilation (build, clean, fclean)
│   └── aura.sh      # Script de lancement avec LD_PRELOAD
├── assets/          # (optionnel) Ressources du projet
```

## Prérequis
- CMake
- g++ (recommandé : >= 13)
- OpenCV 4.x

## Compilation

```sh
./scripts/build.sh         # Compile le projet
./scripts/build.sh clean   # Nettoie le dossier de build
./scripts/build.sh fclean  # Nettoie tout (build + exécutable)
```

## Exécution

Depuis la racine du projet :

```sh
./scripts/aura.sh    # Utilise LD_PRELOAD pour éviter les problèmes avec certaines installations de VS Code
or
./aura               # Exécute directement l'application
```

## Notes
- Si vous utilisez VS Code installé via Snap, des problèmes de bibliothèques peuvent survenir. Utilisez le script aura.sh pour contourner les soucis de LD_PRELOAD.
- Le binaire final sera généré dans le dossier racine sous le nom `aura`.

## Licence
MIT

## Auteur
@Angel-42
