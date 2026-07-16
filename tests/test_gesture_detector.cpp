#include <gtest/gtest.h>
#include "Aura/Vision/GestureDetector.hpp"
#include "Aura/Vision/Types.hpp"
#include "Aura/Core/GestureEvent.hpp"

using namespace Aura::Vision;
using namespace Aura::Core;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Crée un DetectionResult minimal avec des landmarks valides
static DetectionResult makeResult(const LandmarkData& lm) {
    DetectionResult r;
    r.found = true;
    r.landmarks = lm;
    r.smoothedPoint = {320.f, 240.f};
    r.area = 10000.f;
    return r;
}

// Landmarks de base : tous les points à (0.5, 0.5)
static LandmarkData baseLandmarks() {
    LandmarkData lm;
    lm.found = true;
    for (auto& p : lm.pts) p = {0.5f, 0.5f, 0.f};
    return lm;
}

// Tous les doigts étendus (TIP.y < PIP.y), pouce écarté, pas de pinch.
// Les fingertips sont espacés de ~0.15 en X pour éviter les faux pinch doubles.
static LandmarkData openPalmLandmarks() {
    LandmarkData lm = baseLandmarks();
    // Index : TIP[8] à x=0.40, espacé >0.09 du majeur TIP[12]=0.55
    lm.pts[6] = {0.40f, 0.50f, 0.f}; lm.pts[8] = {0.40f, 0.20f, 0.f};
    // Majeur
    lm.pts[10] = {0.55f, 0.50f, 0.f}; lm.pts[12] = {0.55f, 0.20f, 0.f};
    // Annulaire
    lm.pts[14] = {0.70f, 0.50f, 0.f}; lm.pts[16] = {0.70f, 0.20f, 0.f};
    // Auriculaire
    lm.pts[18] = {0.82f, 0.50f, 0.f}; lm.pts[20] = {0.82f, 0.20f, 0.f};
    // Pouce loin de tous les TIP (x=0.10, loin de 0.40) → pas de pinch
    lm.pts[3] = {0.15f, 0.60f, 0.f};
    lm.pts[4] = {0.10f, 0.65f, 0.f};
    return lm;
}

// Tous les doigts fermés (TIP.y > PIP.y)
static LandmarkData fistLandmarks() {
    LandmarkData lm = baseLandmarks();
    lm.pts[6] = {0.45f, 0.40f, 0.f}; lm.pts[8] = {0.45f, 0.65f, 0.f};
    lm.pts[10] = {0.50f, 0.40f, 0.f}; lm.pts[12] = {0.50f, 0.65f, 0.f};
    lm.pts[14] = {0.55f, 0.40f, 0.f}; lm.pts[16] = {0.55f, 0.65f, 0.f};
    lm.pts[18] = {0.60f, 0.40f, 0.f}; lm.pts[20] = {0.60f, 0.65f, 0.f};
    // Pouce fermé (loin des TIP)
    lm.pts[3] = {0.30f, 0.55f, 0.f};
    lm.pts[4] = {0.25f, 0.60f, 0.f};
    return lm;
}

// ---------------------------------------------------------------------------
// Tests — poses statiques (single-frame, pas de swipe ni de Z-tap possible)
// ---------------------------------------------------------------------------

TEST(GestureDetector, HandNotFoundReturnsNone) {
    GestureDetector det;
    DetectionResult r;
    r.found = false;
    EXPECT_EQ(det.classify(r, 640, 480).type, GestureType::NONE);
}

TEST(GestureDetector, OpenPalmDetected) {
    GestureDetector det;
    EXPECT_EQ(det.classify(makeResult(openPalmLandmarks()), 640, 480).type,
              GestureType::OPEN_PALM);
}

TEST(GestureDetector, FistDetected) {
    GestureDetector det;
    EXPECT_EQ(det.classify(makeResult(fistLandmarks()), 640, 480).type,
              GestureType::FIST);
}

TEST(GestureDetector, PointDetected) {
    GestureDetector det;
    LandmarkData lm = fistLandmarks();
    // Lever uniquement l'index
    lm.pts[6] = {0.45f, 0.50f, 0.f};
    lm.pts[8] = {0.45f, 0.20f, 0.f};
    EXPECT_EQ(det.classify(makeResult(lm), 640, 480).type, GestureType::POINT);
}

TEST(GestureDetector, TwoFingersDetected) {
    GestureDetector det;
    LandmarkData lm = fistLandmarks();
    // Lever index + majeur
    lm.pts[6] = {0.45f, 0.50f, 0.f}; lm.pts[8] = {0.45f, 0.20f, 0.f};
    lm.pts[10] = {0.50f, 0.50f, 0.f}; lm.pts[12] = {0.50f, 0.20f, 0.f};
    EXPECT_EQ(det.classify(makeResult(lm), 640, 480).type, GestureType::TWO_FINGERS);
}

TEST(GestureDetector, PinchIndexDetected) {
    GestureDetector det;
    LandmarkData lm = openPalmLandmarks();
    // Pouce très proche de l'index TIP[8]={0.40,0.20}
    // d(pouce,index)=0.01 < 0.07 ✓   d(pouce,majeur)=0.14 > 0.09 ✓ → PINCH single
    lm.pts[4] = {0.41f, 0.20f, 0.f};
    EXPECT_EQ(det.classify(makeResult(lm), 640, 480).type, GestureType::PINCH);
}

TEST(GestureDetector, PinchMiddleDetected) {
    GestureDetector det;
    LandmarkData lm = openPalmLandmarks();
    // Pouce très proche du majeur TIP[12]={0.55,0.20}
    // d(pouce,majeur)=0.01 < 0.07 ✓   d(pouce,index)=0.15 > 0.09 ✓ → PINCH_MIDDLE
    lm.pts[4] = {0.54f, 0.20f, 0.f};
    EXPECT_EQ(det.classify(makeResult(lm), 640, 480).type, GestureType::PINCH_MIDDLE);
}

TEST(GestureDetector, PinchDoublePrioritizedOverSingle) {
    GestureDetector det;
    LandmarkData lm = openPalmLandmarks();
    // Pouce à mi-chemin entre index[0.40] et majeur[0.55] → x=0.475
    // d(pouce,index)=0.075 < 0.09 ✓   d(pouce,majeur)=0.075 < 0.09 ✓ → PINCH_DOUBLE
    lm.pts[4] = {0.475f, 0.20f, 0.f};
    EXPECT_EQ(det.classify(makeResult(lm), 640, 480).type, GestureType::PINCH_DOUBLE);
}

TEST(GestureDetector, PositionNormalized) {
    GestureDetector det;
    DetectionResult r = makeResult(openPalmLandmarks());
    r.smoothedPoint = {320.f, 240.f};
    GestureEvent e = det.classify(r, 640, 480);
    EXPECT_NEAR(e.position.x, 0.5f, 0.01f);
    EXPECT_NEAR(e.position.y, 0.5f, 0.01f);
}
