#pragma once
#include <opencv2/core/types.hpp>
#include <chrono>
#include <string>

namespace Aura::Core {

enum class GestureType {
    NONE,
    OPEN_PALM,       // 5 doigts (pouce compris)
    FOUR_FINGERS,    // 4 doigts levés sans pouce
    FIST,            // poing fermé (maintenu = drag)
    POINT,           // index seul levé
    TWO_FINGERS,     // index + majeur (V)
    THREE_FINGERS,   // trois doigts levés
    PINCH,           // pouce + index
    PINCH_MIDDLE,    // pouce + majeur
    PINCH_RING,      // pouce + annulaire
    PINCH_PINKY,     // pouce + auriculaire
    PINCH_DOUBLE,    // pouce + index + majeur (3 doigts)
    PINCH_SIDE,      // pouce + annulaire + auriculaire
    ZTAP,            // push rapide vers la caméra
    SWIPE_LEFT,
    SWIPE_RIGHT,
    SWIPE_UP,
    SWIPE_DOWN,
};

inline std::string gestureName(GestureType t) {
    switch (t) {
        case GestureType::OPEN_PALM:     return "OPEN_PALM";
        case GestureType::FOUR_FINGERS:  return "FOUR_FINGERS";
        case GestureType::FIST:          return "FIST";
        case GestureType::POINT:         return "POINT";
        case GestureType::TWO_FINGERS:   return "TWO_FINGERS";
        case GestureType::THREE_FINGERS: return "THREE_FINGERS";
        case GestureType::PINCH:         return "PINCH";
        case GestureType::PINCH_MIDDLE:  return "PINCH_MIDDLE";
        case GestureType::PINCH_RING:    return "PINCH_RING";
        case GestureType::PINCH_PINKY:   return "PINCH_PINKY";
        case GestureType::PINCH_DOUBLE:  return "PINCH_DOUBLE";
        case GestureType::PINCH_SIDE:    return "PINCH_SIDE";
        case GestureType::ZTAP:          return "ZTAP";
        case GestureType::SWIPE_LEFT:    return "SWIPE_LEFT";
        case GestureType::SWIPE_RIGHT:   return "SWIPE_RIGHT";
        case GestureType::SWIPE_UP:      return "SWIPE_UP";
        case GestureType::SWIPE_DOWN:    return "SWIPE_DOWN";
        default:                          return "NONE";
    }
}

enum class HandSide { UNKNOWN, LEFT, RIGHT };

struct GestureEvent {
    GestureType     type       = GestureType::NONE;
    HandSide        side       = HandSide::UNKNOWN;
    cv::Point2f     position   = {0.f, 0.f};  // normalisé [0,1]
    float           confidence = 0.f;
    std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

} // namespace Aura::Core
