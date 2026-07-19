#!/usr/bin/env python3
"""
AURA — Entraînement du classificateur de gestes (MLP 128→64).
Usage : python3 scripts/train_classifier.py [--data data/gestures.csv]

Produit :
  models/gesture_classifier.pkl   (sklearn MLPClassifier)
  models/gesture_labels.txt        (liste des classes dans l'ordre)
"""

import sys
import pathlib

import numpy as np
import pandas as pd
from sklearn.neural_network import MLPClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.preprocessing import LabelEncoder
import joblib

SCRIPT_DIR = pathlib.Path(__file__).parent.resolve()
DATA_FILE   = SCRIPT_DIR.parent / "data" / "gestures.csv"
MODELS_DIR  = SCRIPT_DIR.parent / "models"
MODEL_FILE  = MODELS_DIR / "gesture_classifier.pkl"
LABELS_FILE = MODELS_DIR / "gesture_labels.txt"

for a in sys.argv[1:]:
    if a.startswith("--data") and "=" in a:
        DATA_FILE = pathlib.Path(a.split("=", 1)[1])
    elif a == "--data" and sys.argv.index(a) + 1 < len(sys.argv):
        DATA_FILE = pathlib.Path(sys.argv[sys.argv.index(a) + 1])


def train():
    if not DATA_FILE.exists():
        print(f"[train] Fichier introuvable : {DATA_FILE}")
        print("  Lance d'abord : python3 scripts/collect_gestures.py")
        sys.exit(1)

    print(f"Chargement : {DATA_FILE}")
    df = pd.read_csv(DATA_FILE)
    X  = df.iloc[:, :63].values.astype(np.float32)
    y  = df["label"].values

    classes  = sorted(set(y))
    n_cls    = len(classes)
    n_samp   = len(X)
    print(f"  {n_samp} samples — {n_cls} classes : {classes}\n")

    le    = LabelEncoder()
    y_enc = le.fit_transform(y)

    X_train, X_test, y_train, y_test = train_test_split(
        X, y_enc, test_size=0.2, random_state=42, stratify=y_enc)

    print(f"Train : {len(X_train)} | Test : {len(X_test)}")
    print("Entraînement MLP(128, 64)...")

    clf = MLPClassifier(
        hidden_layer_sizes=(128, 64),
        activation="relu",
        solver="adam",
        max_iter=500,
        random_state=42,
        early_stopping=True,
        validation_fraction=0.15,
        n_iter_no_change=15,
        verbose=False,
    )
    clf.fit(X_train, y_train)

    acc = clf.score(X_test, y_test)
    print(f"\nPrécision test : {acc*100:.1f}%  (iters={clf.n_iter_})\n")

    y_pred = clf.predict(X_test)
    print(classification_report(y_test, y_pred, target_names=le.classes_))

    # Matrice de confusion compacte
    cm = confusion_matrix(y_test, y_pred)
    col_w = max(len(c) for c in le.classes_) + 1
    header = " " * col_w + "  ".join(f"{c[:4]:>4}" for c in le.classes_)
    print("Confusion matrix (lignes=réel, cols=prédit) :")
    print(header)
    for i, row in enumerate(cm):
        label = le.classes_[i]
        cells = "  ".join(
            f"\033[32m{v:>4}\033[0m" if j == i else f"\033[31m{v:>4}\033[0m" if v > 0 else f"{v:>4}"
            for j, v in enumerate(row)
        )
        print(f"{label:<{col_w}}{cells}")

    # Sauvegarde
    MODELS_DIR.mkdir(exist_ok=True)
    joblib.dump(clf, MODEL_FILE)
    with open(LABELS_FILE, "w") as f:
        f.write("\n".join(le.classes_))

    print(f"\n✓ Modèle   → {MODEL_FILE}")
    print(f"✓ Labels   → {LABELS_FILE}")
    print("\nLe bridge chargera automatiquement ce modèle au prochain démarrage.")


if __name__ == "__main__":
    train()
