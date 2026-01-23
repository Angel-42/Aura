#!/bin/bash

# Script pour lancer Aura avec LD_PRELOAD

# Chemin vers la bibliothèque à précharger (modifie-le si besoin)
LD_PRELOAD_LIB="/lib/x86_64-linux-gnu/libpthread.so.0"

# Chemin vers l'exécutable Aura (modifie-le si besoin)
AURA_EXEC="./aura"

# Lancement du programme avec LD_PRELOAD
LD_PRELOAD="$LD_PRELOAD_LIB" "$AURA_EXEC" "$@"