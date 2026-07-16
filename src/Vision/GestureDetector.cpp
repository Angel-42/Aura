#include "Aura/Vision/GestureDetector.hpp"
#include <cmath>
#include <algorithm>

namespace Aura::Vision {

using GestureType  = Core::GestureType;
using GestureEvent = Core::GestureEvent;

// --------------------------------------------------------------------------
// Détection multi-pinch — pouce vs chaque fingertip
//
//  Seuil single  0.07  : un seul doigt clairement en pince
//  Seuil double  0.09  : deux doigts approchent en même temps (chacun légèrement plus loin)
//  Priorité doubles > singles pour éviter les ambiguïtés
// --------------------------------------------------------------------------

static GestureType detectPinch(const LandmarkData& lm) {
    constexpr int   tips[4] = {8, 12, 16, 20};
    constexpr float kSingle = 0.07f;
    constexpr float kDouble = 0.09f;

    float d[4];
    for (int f = 0; f < 4; ++f) {
        float dx = lm.pts[4].x - lm.pts[tips[f]].x;
        float dy = lm.pts[4].y - lm.pts[tips[f]].y;
        d[f] = std::sqrt(dx*dx + dy*dy);
    }

    bool p[4] = { d[0] < kDouble, d[1] < kDouble, d[2] < kDouble, d[3] < kDouble };

    // Doubles vérifiés en premier (cas plus restrictif)
    if (p[0] && p[1]) return GestureType::PINCH_DOUBLE;  // pouce + index + majeur
    if (p[2] && p[3]) return GestureType::PINCH_SIDE;    // pouce + annulaire + auriculaire

    // Singles
    if (d[0] < kSingle) return GestureType::PINCH;
    if (d[1] < kSingle) return GestureType::PINCH_MIDDLE;
    if (d[2] < kSingle) return GestureType::PINCH_RING;
    if (d[3] < kSingle) return GestureType::PINCH_PINKY;

    return GestureType::NONE;
}

// --------------------------------------------------------------------------
// Classification landmarks MediaPipe
// --------------------------------------------------------------------------

static GestureType classifyFromLandmarks(const LandmarkData& lm) {
    // Pinch prioritaire sur le comptage de doigts
    GestureType pinch = detectPinch(lm);
    if (pinch != GestureType::NONE) return pinch;

    constexpr int tips[4] = {8, 12, 16, 20};
    constexpr int pips[4] = {6, 10, 14, 18};
    int extended = 0;
    for (int f = 0; f < 4; ++f) {
        if (lm.pts[tips[f]].y < lm.pts[pips[f]].y) ++extended;
    }

    bool thumbOut = (lm.pts[4].x < lm.pts[3].x);

    if (extended == 0)               return GestureType::FIST;
    if (extended == 1)               return GestureType::POINT;
    if (extended == 2)               return GestureType::TWO_FINGERS;
    if (extended == 3)               return GestureType::THREE_FINGERS;
    if (extended == 4 && !thumbOut)  return GestureType::FOUR_FINGERS;
    return GestureType::OPEN_PALM;
    (void)thumbOut;
}

// --------------------------------------------------------------------------
// Classifiction contours (fallback CV)
// --------------------------------------------------------------------------

GestureType GestureDetector::classifyShape(int defects, float solidity) {
    if (defects == 0) return (solidity > 0.85f) ? GestureType::FIST : GestureType::POINT;
    if (defects == 1) return GestureType::TWO_FINGERS;
    if (defects == 2) return GestureType::THREE_FINGERS;
    return GestureType::OPEN_PALM;
}

int GestureDetector::countDeepDefects(const std::vector<cv::Point>& contour,
                                       float minDepth) {
    if (contour.size() < 5) return 0;
    std::vector<int> hullIdx;
    cv::convexHull(contour, hullIdx, false, false);
    if (hullIdx.size() < 3) return 0;
    std::vector<cv::Vec4i> defects;
    cv::convexityDefects(contour, hullIdx, defects);
    int count = 0;
    for (const auto& d : defects) {
        if (d[3] / 256.0f > minDepth) ++count;
    }
    return count;
}

// --------------------------------------------------------------------------
// Swipe via historique de position
// --------------------------------------------------------------------------

GestureType GestureDetector::detectSwipe(const cv::Point2f& normPos) {
    posHistory_.push_back(normPos);
    if (static_cast<int>(posHistory_.size()) > kHistorySize)
        posHistory_.pop_front();

    if (static_cast<int>(posHistory_.size()) < kHistorySize)
        return GestureType::NONE;

    auto now = std::chrono::steady_clock::now();
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now - lastSwipeTime_).count();
    if (ms < kSwipeCooldownMs) return GestureType::NONE;

    cv::Point2f delta = posHistory_.back() - posHistory_.front();
    float dx  = std::abs(delta.x);
    float dy  = std::abs(delta.y);
    float mag = std::sqrt(dx * dx + dy * dy);
    if (mag < kSwipeThreshold) return GestureType::NONE;

    lastSwipeTime_ = now;
    if (dx > dy) return (delta.x > 0) ? GestureType::SWIPE_RIGHT : GestureType::SWIPE_LEFT;
    return (delta.y > 0) ? GestureType::SWIPE_DOWN : GestureType::SWIPE_UP;
}

// --------------------------------------------------------------------------
// Z-tap : push rapide vers la caméra détecté via l'augmentation de scale
// (distance wrist → milieu de la paume = proxy de profondeur)
// --------------------------------------------------------------------------

GestureType GestureDetector::detectZTap(const LandmarkData& lm) {
    if (!lm.found) {
        scaleHistory_.clear();
        ztapArmed_ = false;
        return GestureType::NONE;
    }

    float dx = lm.pts[9].x - lm.pts[0].x;
    float dy = lm.pts[9].y - lm.pts[0].y;
    float scale = std::sqrt(dx*dx + dy*dy);

    scaleHistory_.push_back(scale);
    if (static_cast<int>(scaleHistory_.size()) > kScaleHistorySize)
        scaleHistory_.pop_front();
    if (static_cast<int>(scaleHistory_.size()) < kScaleHistorySize)
        return GestureType::NONE;

    // Baseline = moyenne de la première moitié de l'historique
    float baseline = 0.f;
    for (int i = 0; i < kScaleHistorySize / 2; ++i) baseline += scaleHistory_[i];
    baseline /= static_cast<float>(kScaleHistorySize / 2);

    float current = scaleHistory_.back();
    float ratio   = (baseline > 0.f) ? current / baseline : 1.f;

    auto now = std::chrono::steady_clock::now();
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now - lastZTapTime_).count();

    if (!ztapArmed_ && ratio > 1.f + kZTapThreshold) {
        ztapArmed_     = true;
        peakScale_     = current;
        ztapArmedTime_ = now;
    } else if (ztapArmed_) {
        auto tapMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - ztapArmedTime_).count();

        if (tapMs > kZTapMaxDurationMs) {
            // Trop lent pour être un tap — c'est un mouvement vers l'avant, on annule
            ztapArmed_ = false;
        } else if (current < peakScale_ * 0.88f) {
            ztapArmed_ = false;
            if (ms >= kZTapCooldownMs) {
                lastZTapTime_ = now;
                return GestureType::ZTAP;
            }
        }
    }
    return GestureType::NONE;
}

// --------------------------------------------------------------------------
// classify — point d'entrée
// --------------------------------------------------------------------------

GestureEvent GestureDetector::classify(const DetectionResult& result,
                                        int frameW, int frameH) {
    GestureEvent event;
    event.timestamp = std::chrono::steady_clock::now();

    if (!result.found) {
        posHistory_.clear();
        scaleHistory_.clear();
        ztapArmed_ = false;
        return event; // NONE
    }

    float nx = (frameW > 0) ? result.smoothedPoint.x / static_cast<float>(frameW) : 0.f;
    float ny = (frameH > 0) ? result.smoothedPoint.y / static_cast<float>(frameH) : 0.f;
    event.position = {std::clamp(nx, 0.f, 1.f), std::clamp(ny, 0.f, 1.f)};

    // Priorité 1 : Z-tap — seulement en pose de navigation (OPEN_PALM ou POINT)
    // pour éviter les faux positifs quand on fait un geste d'action
    if (result.landmarks.found) {
        GestureType pose = classifyFromLandmarks(result.landmarks);
        bool isNavPose   = (pose == GestureType::OPEN_PALM || pose == GestureType::POINT);
        if (isNavPose) {
            GestureType ztap = detectZTap(result.landmarks);
            if (ztap != GestureType::NONE) {
                event.type       = ztap;
                event.confidence = 0.85f;
                return event;
            }
        } else {
            // Réinitialiser l'état ZTAP pour éviter un faux trigger à la sortie du geste
            scaleHistory_.clear();
            ztapArmed_ = false;
        }
    }

    // Priorité 2 : swipe
    GestureType swipe = detectSwipe(event.position);
    if (swipe != GestureType::NONE) {
        event.type       = swipe;
        event.confidence = 0.9f;
        return event;
    }

    // Priorité 3 : classification de pose
    if (result.landmarks.found) {
        event.type       = classifyFromLandmarks(result.landmarks);
        event.confidence = 0.92f;
    } else {
        int fingers    = result.fingerCount;
        float solidity = (result.hullArea > 0.f) ? result.area / result.hullArea : 1.f;
        if (fingers == 0 && !result.contour.empty())
            fingers = countDeepDefects(result.contour);
        event.type       = classifyShape(fingers, solidity);
        event.confidence = 0.65f;
    }

    return event;
}

} // namespace Aura::Vision
