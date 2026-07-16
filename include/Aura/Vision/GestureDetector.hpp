#pragma once
#include "Aura/Vision/Types.hpp"
#include "Aura/Core/GestureEvent.hpp"
#include <deque>
#include <chrono>

namespace Aura::Vision {

class GestureDetector {
public:
    GestureDetector() = default;

    // Classifie la DetectionResult en GestureEvent.
    // frameW/frameH servent à normaliser la position [0,1].
    Core::GestureEvent classify(const DetectionResult& result,
                                int frameW, int frameH);

private:
    // ---- Analyse de forme ----
    static int countDeepDefects(const std::vector<cv::Point>& contour,
                                float minDepth = 20.f);
    static Core::GestureType classifyShape(int defects, float solidity);

    // ---- Détection de swipe ----
    Core::GestureType detectSwipe(const cv::Point2f& normPos);

    std::deque<cv::Point2f> posHistory_;
    std::chrono::steady_clock::time_point lastSwipeTime_{};

    static constexpr int   kHistorySize     = 14;
    static constexpr float kSwipeThreshold  = 0.22f;
    static constexpr int   kSwipeCooldownMs = 700;

    // ---- Détection de Z-tap (push vers caméra via scale main) ----
    Core::GestureType detectZTap(const LandmarkData& lm);

    std::deque<float> scaleHistory_;
    std::chrono::steady_clock::time_point lastZTapTime_{};
    std::chrono::steady_clock::time_point ztapArmedTime_{};
    float peakScale_ = 0.f;
    bool  ztapArmed_ = false;

    static constexpr int   kScaleHistorySize = 20;
    static constexpr float kZTapThreshold    = 0.15f;  // 15% d'augmentation de scale
    static constexpr int   kZTapCooldownMs   = 600;
    static constexpr int   kZTapMaxDurationMs = 450;   // un tap > 450ms = mouvement lent, ignoré
};

} // namespace Aura::Vision
