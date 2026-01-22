#!/bin/bash
set -e

BUILD_DIR="build"

mkdir -p $BUILD_DIR
cd $BUILD_DIR
cmake -DOpenCV_DIR=/usr/lib/x86_64-linux-gnu/cmake/opencv4 ..
make

echo "Build terminé. Exécutable : $BUILD_DIR/aura"