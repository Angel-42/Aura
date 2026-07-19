#!/bin/bash
set -e

BUILD_DIR="build"
AURA_EXEC="aura"
DEMO_EXEC="aura_demo"

function cmake_configure() {
    local extra_flags="$1"
    if [[ -n "$OpenCV_DIR" ]]; then
        cmake -DOpenCV_DIR="$OpenCV_DIR" $extra_flags ..
    else
        cmake $extra_flags ..
    fi
}

function build() {
    echo "Building Aura..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake_configure ""
    make "$AURA_EXEC" -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"
    cp "$AURA_EXEC" ../"$AURA_EXEC"
    echo "Build terminé. Exécutable : $AURA_EXEC"
    cd ..
}

function build_demo() {
    echo "Building Aura Demo (SFML)..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake_configure "-DBUILD_DEMO=ON"
    make "$DEMO_EXEC" -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"
    cp "demo/$DEMO_EXEC" ../"$DEMO_EXEC"
    echo "Build terminé. Exécutable : $DEMO_EXEC"
    cd ..
}

function clean() {
    echo "Nettoyage du dossier de build..."
    rm -rf "$BUILD_DIR"
    echo "Nettoyage terminé."
}

function fclean() {
    clean
    echo "Suppression des exécutables..."
    rm -f "$AURA_EXEC" "$DEMO_EXEC"
    echo "Full clean terminé."
}

function help() {
    echo "Usage: $0 [build|demo|clean|fclean|help]"
    echo "  build   : Compile le projet principal (défaut)"
    echo "  demo    : Compile la démo SFML + copie aura_demo à la racine"
    echo "  clean   : Supprime le dossier de build"
    echo "  fclean  : clean + supprime les exécutables à la racine"
    echo "  help    : Affiche cette aide"
    echo ""
    echo "Pour macOS avec Homebrew, exportez le chemin OpenCV si nécessaire :"
    echo "  export OpenCV_DIR=\"/opt/homebrew/opt/opencv/lib/cmake/opencv4\""
}

case "$1" in
    ""|build)
        build
        ;;
    demo)
        build_demo
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
