#!/usr/bin/env python3
"""
AURA Hand Bridge — MediaPipe Tasks API (v0.10+).
Détecte jusqu'à 2 mains et envoie leurs landmarks sur stdout.

Protocole stdout (par frame) :
  Avec mains détectées :
    HAND LEFT  <x0> <y0> <z0> ... <x20> <y20> <z20>
    HAND RIGHT <x0> <y0> <z0> ... <x20> <y20> <z20>
    FRAME_END
  Sans main :
    NONE
  Fermeture :
    QUIT

Indices MediaPipe :
  0=Wrist  4=ThumbTIP  8=IndexTIP  12=MiddleTIP  16=RingTIP  20=PinkyTIP
  PIPs    : 3(Thumb) 6(Index) 10(Middle) 14(Ring) 18(Pinky)

Usage : python3 hand_bridge.py [camera_id=0] [--no-window]
"""

import sys, os, pathlib
import cv2
import numpy as np
import mediapipe as mp
from mediapipe.tasks.python import vision as mpv
from mediapipe.tasks.python.core import base_options as mpo

# --------------------------------------------------------------------------
# Localiser les modèles
# --------------------------------------------------------------------------
SCRIPT_DIR  = pathlib.Path(__file__).parent.resolve()
MODEL_PATHS = [
    SCRIPT_DIR.parent / "models" / "hand_landmarker.task",
    pathlib.Path("models/hand_landmarker.task"),
    pathlib.Path("../models/hand_landmarker.task"),
]

ML_MODEL_PATHS = [
    SCRIPT_DIR.parent / "models" / "gesture_classifier.pkl",
    pathlib.Path("models/gesture_classifier.pkl"),
]
ML_LABELS_PATHS = [
    SCRIPT_DIR.parent / "models" / "gesture_labels.txt",
    pathlib.Path("models/gesture_labels.txt"),
]

def find_model():
    for p in MODEL_PATHS:
        if p.exists():
            return str(p)
    return None


# --------------------------------------------------------------------------
# Classificateur ML (chargé une fois au démarrage)
# --------------------------------------------------------------------------
_ml_clf    = None
_ml_labels = None   # list[str] indexé par la prédiction entière

def _load_ml_model():
    global _ml_clf, _ml_labels
    try:
        import joblib
    except ImportError:
        sys.stderr.write("[bridge] joblib absent — modèle ML ignoré (pip install joblib)\n")
        return

    for p in ML_MODEL_PATHS:
        if p.exists():
            _ml_clf = joblib.load(str(p))
            break

    for p in ML_LABELS_PATHS:
        if p.exists():
            _ml_labels = p.read_text().strip().splitlines()
            break

    if _ml_clf is not None and _ml_labels is not None:
        sys.stderr.write(
            f"[bridge] Modèle ML chargé → {len(_ml_labels)} classes : {_ml_labels}\n")
    else:
        if _ml_clf is None:
            sys.stderr.write("[bridge] Pas de modèle ML (models/gesture_classifier.pkl absent)\n")
        else:
            sys.stderr.write("[bridge] Labels ML absents (models/gesture_labels.txt absent)\n")
        _ml_clf = None


def _normalize(lm_list):
    """Même normalisation que collect_gestures.py : wrist à l'origine + scale max."""
    pts = np.array([[lm.x, lm.y, lm.z] for lm in lm_list], dtype=np.float32)
    pts -= pts[0]
    sc = np.max(np.abs(pts))
    if sc > 1e-6:
        pts /= sc
    return pts.flatten().reshape(1, -1)


def predict_gesture(lm_list) -> str:
    """Retourne le nom du geste prédit, ou '' si pas de modèle / main mal détectée."""
    if _ml_clf is None or _ml_labels is None:
        return ""
    try:
        feat  = _normalize(lm_list)
        idx   = int(_ml_clf.predict(feat)[0])
        if 0 <= idx < len(_ml_labels):
            return _ml_labels[idx]
    except Exception as e:
        sys.stderr.write(f"[bridge] Prédiction échouée : {e}\n")
    return ""

# Connexions entre landmarks pour dessiner le skeleton
HAND_CONNECTIONS = [
    (0,1),(1,2),(2,3),(3,4),        # Thumb
    (0,5),(5,6),(6,7),(7,8),        # Index
    (0,9),(9,10),(10,11),(11,12),   # Middle
    (0,13),(13,14),(14,15),(15,16), # Ring
    (0,17),(17,18),(18,19),(19,20), # Pinky
    (5,9),(9,13),(13,17),           # Palm
]
FINGERTIP_IDS = [4, 8, 12, 16, 20]

HAND_COLORS = {
    "LEFT":    (0,  210, 255),   # orange  (BGR)
    "RIGHT":   (255, 80,  50),   # bleu    (BGR)
    "UNKNOWN": (180, 180, 180),
}

def draw_skeleton(frame, landmarks_norm, side="UNKNOWN"):
    h, w = frame.shape[:2]
    pts = [(int(lm[0]*w), int(lm[1]*h)) for lm in landmarks_norm]
    bone_color = HAND_COLORS.get(side, HAND_COLORS["UNKNOWN"])

    # Os
    for a, b in HAND_CONNECTIONS:
        cv2.line(frame, pts[a], pts[b], bone_color, 2, cv2.LINE_AA)

    # Tous les noeuds
    for i, (px, py) in enumerate(pts):
        color = (0, 255, 100) if i in FINGERTIP_IDS else (200, 200, 200)
        r     = 7             if i in FINGERTIP_IDS else 4
        cv2.circle(frame, (px, py), r, color, -1, cv2.LINE_AA)
        cv2.circle(frame, (px, py), r, (255, 255, 255), 1, cv2.LINE_AA)

    # Paume (wrist)
    cv2.circle(frame, pts[0], 10, bone_color, -1, cv2.LINE_AA)
    cv2.circle(frame, pts[0], 10, (255, 255, 255), 2, cv2.LINE_AA)

def main():
    camera_id   = 0
    show_window = True

    for arg in sys.argv[1:]:
        if arg == "--no-window": show_window = False
        elif arg.isdigit():      camera_id = int(arg)

    # ---- Modèle ML (facultatif) ----
    _load_ml_model()

    # ---- Modèle landmarks ----
    model_path = find_model()
    if not model_path:
        sys.stderr.write("[bridge] Modèle introuvable. Placez hand_landmarker.task dans models/\n")
        sys.exit(1)

    base_opts = mpo.BaseOptions(model_asset_path=model_path)
    opts = mpv.HandLandmarkerOptions(
        base_options=base_opts,
        running_mode=mpv.RunningMode.VIDEO,
        num_hands=2,
        min_hand_detection_confidence=0.60,
        min_hand_presence_confidence=0.55,
        min_tracking_confidence=0.55,
    )
    landmarker = mpv.HandLandmarker.create_from_options(opts)

    # ---- Caméra ----
    cap = cv2.VideoCapture(camera_id)
    if not cap.isOpened():
        sys.stderr.write(f"[bridge] Impossible d'ouvrir la caméra {camera_id}\n")
        sys.exit(1)

    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    timestamp_ms = 0

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break

        timestamp_ms += 33  # ~30fps nominal

        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)

        result = landmarker.detect_for_video(mp_image, timestamp_ms)

        if result.hand_landmarks:
            for i, lm_list in enumerate(result.hand_landmarks):
                # MediaPipe donne la latéralité depuis la perspective de la personne
                side = "UNKNOWN"
                if result.handedness and i < len(result.handedness):
                    side = result.handedness[i][0].category_name.upper()  # "LEFT" ou "RIGHT"

                coords = []
                lm_norm = []
                for lm in lm_list:
                    coords += [f"{lm.x:.5f}", f"{lm.y:.5f}", f"{lm.z:.5f}"]
                    lm_norm.append((lm.x, lm.y, lm.z))

                # Prédiction ML (suffixe optionnel)
                gesture_suffix = ""
                ml_pred = predict_gesture(lm_list)
                if ml_pred:
                    gesture_suffix = f" GESTURE {ml_pred}"

                print(f"HAND {side} " + " ".join(coords) + gesture_suffix, flush=True)

                if show_window:
                    draw_skeleton(frame, lm_norm, side)
                    label = f"[{side}]"
                    cx = int(lm_norm[0][0] * frame.shape[1])
                    cy = max(20, int(lm_norm[0][1] * frame.shape[0]) - 20)
                    cv2.putText(frame, label, (cx, cy),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.65,
                                HAND_COLORS.get(side, (200,200,200)), 2, cv2.LINE_AA)
            print("FRAME_END", flush=True)
        else:
            print("NONE", flush=True)
            if show_window:
                cv2.putText(frame, "Aucune main", (10, 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.75,
                            (0, 80, 200), 2, cv2.LINE_AA)

        if show_window:
            cv2.imshow("AURA — MediaPipe", frame)
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                print("QUIT", flush=True)
                break

    cap.release()
    landmarker.close()
    if show_window:
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
