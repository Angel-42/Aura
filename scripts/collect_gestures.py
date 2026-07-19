#!/usr/bin/env python3
"""
AURA — Collecte de données gestuelles pour le classificateur ML.
Usage : python3 scripts/collect_gestures.py [--camera 0]

Crée : data/gestures.csv  (63 features normalisées + label par ligne)
"""

import sys
import csv
import pathlib
import time

import cv2
import mediapipe as mp
import numpy as np
from mediapipe.tasks.python import vision as mpv
from mediapipe.tasks.python.core import base_options as mpo

SCRIPT_DIR = pathlib.Path(__file__).parent.resolve()

# Gestes statiques reconnus par le classificateur ML.
# Swipes (temporels) et ZTAP (push) restent gérés par le détecteur C++.
GESTURES = [
    ("NONE",           "Main visible mais aucun geste — doigts quelconques"),
    ("OPEN_PALM",      "5 doigts ouverts, paume face caméra"),
    ("FIST",           "Poing fermé — tous les doigts repliés"),
    ("POINT",          "Index seul levé, autres doigts repliés"),
    ("TWO_FINGERS",    "Index + majeur levés (V de victoire)"),
    ("THREE_FINGERS",  "Index + majeur + annulaire levés"),
    ("FOUR_FINGERS",   "4 doigts levés, pouce replié"),
    ("PINCH",          "Pouce + index se touchent"),
    ("PINCH_MIDDLE",   "Pouce + majeur se touchent"),
    ("PINCH_DOUBLE",   "Pouce + index + majeur se touchent"),
    ("PINCH_RING",     "Pouce + annulaire se touchent"),
    ("PINCH_PINKY",    "Pouce + auriculaire se touchent"),
    ("PINCH_SIDE",     "Pouce + annulaire + auriculaire"),
]

SAMPLES_PER_GESTURE = 200
DATA_DIR  = SCRIPT_DIR.parent / "data"
OUT_FILE  = DATA_DIR / "gestures.csv"
COUNTDOWN = 3

MODEL_PATHS = [
    SCRIPT_DIR.parent / "models" / "hand_landmarker.task",
    pathlib.Path("models/hand_landmarker.task"),
]

HAND_CONNECTIONS = [
    (0,1),(1,2),(2,3),(3,4),
    (0,5),(5,6),(6,7),(7,8),
    (0,9),(9,10),(10,11),(11,12),
    (0,13),(13,14),(14,15),(15,16),
    (0,17),(17,18),(18,19),(19,20),
    (5,9),(9,13),(13,17),
]
TIPS = [4, 8, 12, 16, 20]

def find_model():
    for p in MODEL_PATHS:
        if p.exists():
            return str(p)
    return None


def normalize(lm_list):
    """Normalise les landmarks : translation wrist + scale max — invariant position/taille."""
    pts = np.array([[lm.x, lm.y, lm.z] for lm in lm_list], dtype=np.float32)
    pts -= pts[0]  # wrist à l'origine
    sc = np.max(np.abs(pts))
    if sc > 1e-6:
        pts /= sc
    return pts.flatten()  # 63 valeurs


def draw_hand(frame, lm_list):
    h, w = frame.shape[:2]
    pts = [(int(lm.x * w), int(lm.y * h)) for lm in lm_list]
    for a, b in HAND_CONNECTIONS:
        cv2.line(frame, pts[a], pts[b], (0, 200, 100), 2, cv2.LINE_AA)
    for i, (px, py) in enumerate(pts):
        r = 7 if i in TIPS else 4
        c = (0, 255, 80) if i in TIPS else (200, 200, 200)
        cv2.circle(frame, (px, py), r, c, -1, cv2.LINE_AA)
        cv2.circle(frame, (px, py), r, (255, 255, 255), 1, cv2.LINE_AA)


def collect():
    model_path = find_model()
    if not model_path:
        print("[collect] Modèle hand_landmarker.task introuvable dans models/")
        sys.exit(1)

    camera_id = 0
    for i, a in enumerate(sys.argv[1:]):
        if a == "--camera" and i + 1 < len(sys.argv) - 1:
            camera_id = int(sys.argv[i + 2])

    base_opts = mpo.BaseOptions(model_asset_path=model_path)
    opts = mpv.HandLandmarkerOptions(
        base_options=base_opts,
        running_mode=mpv.RunningMode.VIDEO,
        num_hands=1,
        min_hand_detection_confidence=0.65,
        min_tracking_confidence=0.55,
    )
    landmarker = mpv.HandLandmarker.create_from_options(opts)
    cap = cv2.VideoCapture(camera_id)
    if not cap.isOpened():
        print(f"[collect] Impossible d'ouvrir la caméra {camera_id}")
        sys.exit(1)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    DATA_DIR.mkdir(exist_ok=True)
    rows = []
    header = [f"f{i}" for i in range(63)] + ["label"]
    timestamp_ms = 0

    print(f"\n=== AURA — Collecte gestures ===")
    print(f"  {len(GESTURES)} gestes × {SAMPLES_PER_GESTURE} samples = {len(GESTURES)*SAMPLES_PER_GESTURE} lignes")
    print(f"  Sortie : {OUT_FILE}\n")
    print("Commandes : ESPACE = commencer | S = passer | Q = sauvegarder et quitter\n")

    for g_idx, (gesture_name, gesture_desc) in enumerate(GESTURES):
        print(f"[{g_idx+1}/{len(GESTURES)}] {gesture_name}")
        print(f"  {gesture_desc}")
        print(f"  Prépare le geste et appuie sur ESPACE quand tu es prêt...")

        # Attendre ESPACE ou Q
        while True:
            ret, frame = cap.read()
            if not ret:
                break
            frame = cv2.flip(frame, 1)
            timestamp_ms += 33
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            res = landmarker.detect_for_video(
                mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb), timestamp_ms)
            if res.hand_landmarks:
                draw_hand(frame, res.hand_landmarks[0])
            cv2.putText(frame, f"Geste : {gesture_name}", (10, 40),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 255, 100), 2)
            cv2.putText(frame, gesture_desc, (10, 70),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.55, (180, 180, 180), 1)
            cv2.putText(frame, "ESPACE = commencer  |  S = passer  |  Q = quitter", (10, frame.shape[0]-15),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (120, 120, 120), 1)
            cv2.putText(frame, f"[{g_idx+1}/{len(GESTURES)}]", (frame.shape[1]-80, 40),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (100, 100, 255), 2)
            cv2.imshow("AURA — Collecte", frame)
            key = cv2.waitKey(1) & 0xFF
            if key == ord(' '):
                break
            if key == ord('s'):
                print(f"  → Passé\n")
                break
            if key == ord('q'):
                cap.release(); cv2.destroyAllWindows(); landmarker.close()
                _save(rows, header, OUT_FILE)
                return
        else:
            continue

        # Vérifier si on a sauté
        if key == ord('s'):
            continue

        # Countdown
        for t in range(COUNTDOWN, 0, -1):
            ret, frame = cap.read()
            if not ret:
                break
            frame = cv2.flip(frame, 1)
            overlay = frame.copy()
            cx, cy = frame.shape[1]//2, frame.shape[0]//2
            cv2.circle(overlay, (cx, cy), 80, (0, 0, 0), -1)
            cv2.putText(overlay, str(t), (cx - 25, cy + 20),
                        cv2.FONT_HERSHEY_SIMPLEX, 2.5, (0, 80, 255), 5)
            cv2.putText(overlay, "PRÊT", (cx - 35, cy + 60),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (180, 180, 180), 2)
            cv2.addWeighted(overlay, 0.75, frame, 0.25, 0, frame)
            cv2.imshow("AURA — Collecte", frame)
            cv2.waitKey(1000)

        # Collecte
        collected = 0
        while collected < SAMPLES_PER_GESTURE:
            ret, frame = cap.read()
            if not ret:
                break
            frame = cv2.flip(frame, 1)
            timestamp_ms += 33
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            res = landmarker.detect_for_video(
                mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb), timestamp_ms)

            if res.hand_landmarks:
                draw_hand(frame, res.hand_landmarks[0])
                feats = normalize(res.hand_landmarks[0])
                rows.append(list(feats) + [gesture_name])
                collected += 1

            # Barre de progression
            pct  = collected / SAMPLES_PER_GESTURE
            bw   = frame.shape[1] - 20
            bx, by, bh = 10, frame.shape[0] - 30, 16
            cv2.rectangle(frame, (bx, by), (bx + bw, by + bh), (50, 50, 50), -1)
            cv2.rectangle(frame, (bx, by), (bx + int(bw * pct), by + bh), (0, 220, 80), -1)
            cv2.putText(frame, f"{gesture_name}  {collected}/{SAMPLES_PER_GESTURE}", (10, by - 8),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.65, (0, 255, 120), 2)
            cv2.imshow("AURA — Collecte", frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                cap.release(); cv2.destroyAllWindows(); landmarker.close()
                _save(rows, header, OUT_FILE)
                return

        print(f"  ✓ {collected} samples — total : {len(rows)}\n")

    cap.release()
    cv2.destroyAllWindows()
    landmarker.close()
    _save(rows, header, OUT_FILE)


def _save(rows, header, path):
    with open(path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(rows)
    print(f"\n✓ Dataset sauvegardé : {path}  ({len(rows)} samples, {len(rows[0])-1 if rows else 0} features)")
    print("  → Lance maintenant : python3 scripts/train_classifier.py")


if __name__ == "__main__":
    collect()
