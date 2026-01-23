#!/bin/bash
set -e

BUILD_DIR="build"
AURA_EXEC="aura"

function build() {
    echo "Building Aura..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake -DOpenCV_DIR=/usr/lib/x86_64-linux-gnu/cmake/opencv4 ..
    make
    ls -la
    cp "$AURA_EXEC" ../"$AURA_EXEC"
    echo "Build terminé. Exécutable : $BUILD_DIR/$AURA_EXEC"
    cd ..
}

function clean() {
    echo "Nettoyage du dossier de build..."
    rm -rf "$BUILD_DIR"
    echo "Nettoyage terminé."
}

function fclean() {
    clean
    echo "Suppression de l'exécutable principal..."
    rm -f "$AURA_EXEC"
    echo "Full clean terminé."
}

function help() {
    echo "Usage: $0 [build|clean|fclean|help]"
    echo "  build   : Compile le projet (défaut)"
    echo "  clean   : Supprime le dossier de build"
    echo "  fclean  : clean + supprime l'exécutable principal"
    echo "  help    : Affiche cette aide"
}

case "$1" in
    ""|build)
        build
        ;;
    clean)
        clean
        ;;
    fclean)
        fclean
        ;;
    help)
        help
        ;;
    *)
        echo "Commande inconnue : $1"
        help
        exit 1
        ;;
esac