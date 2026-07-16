#pragma once
#include "Aura/Core/GestureEvent.hpp"  // for HandSide
#include <opencv2/opencv.hpp>
#include <array>
#include <vector>

namespace Aura::Vision {

struct HSVRange {
    int H_min = 0,   H_max = 179;
    int S_min = 48,  S_max = 255;
    int V_min = 40,  V_max = 255;
};

// 21 landmarks MediaPipe, coordonnées normalisées [0,1]
// Indices TIP : 4=Pouce 8=Index 12=Majeur 16=Annulaire 20=Auriculaire
// Indices PIP : 3       6       10        14           18
struct LandmarkData {
    bool found = false;
    std::array<cv::Point3f, 21> pts{};

    [[nodiscard]] cv::Point3f tip(int finger) const {
        // finger 0=pouce…4=auriculaire  →  landmark TIP
        static constexpr int tips[5] = {4, 8, 12, 16, 20};
        return pts[tips[finger]];
    }
    [[nodiscard]] cv::Point3f pip(int finger) const {
        static constexpr int pips[5] = {3, 6, 10, 14, 18};
        return pts[pips[finger]];
    }
    // Centre de la paume (moyenne wrist + bases des 4 doigts)
    [[nodiscard]] cv::Point2f palmCenter() const {
        float x = 0.f, y = 0.f;
        for (int i : {0, 5, 9, 13, 17}) { x += pts[i].x; y += pts[i].y; }
        return {x / 5.f, y / 5.f};
    }
};

struct DetectionResult {
    bool              found         = false;
    Core::HandSide    side          = Core::HandSide::UNKNOWN;
    cv::Point2f       rawPoint      = {0.f, 0.f};
    cv::Point2f     smoothedPoint = {0.f, 0.f};
    float           area          = 0.f;
    float           hullArea      = 0.f;
    std::vector<cv::Point>   contour;
    std::vector<cv::Point>   hull;
    std::vector<cv::Point2f> fingertips;
    int                      fingerCount = 0;
    LandmarkData             landmarks;   // rempli si bridge MediaPipe actif
};

} // namespace Aura::Vision
