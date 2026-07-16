#!/bin/bash

# Script pour lancer Aura.
# Sur Linux, il peut précharger une bibliothèque si nécessaire.

AURA_EXEC="./aura"

if [[ "$OSTYPE" == "linux-gnu"* && -n "$LD_PRELOAD_LIB" ]]; then
    LD_PRELOAD="$LD_PRELOAD_LIB" "$AURA_EXEC" "$@"
else
    "$AURA_EXEC" "$@"
fi
