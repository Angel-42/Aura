#pragma once
#include "Aura/Vision/Types.hpp"
#include "Aura/Core/KalmanFilter.hpp"
#include <opencv2/video.hpp>
#include <cstdio>
#include <string>

namespace Aura::Vision {

// HandTracker tente d'abord de démarrer le bridge Python/MediaPipe.
// Si MediaPipe est indisponible, il bascule en mode OpenCV (BGSub).
//
// En mode bridge : la caméra est gérée côté Python. C++ lit les landmarks.
// En mode CV     : C++ gère la caméra avec BGSub + YCrCb.
class HandTracker {
public:
    HandTracker();
    ~HandTracker();

    HandTracker(const HandTracker&) = delete;
    HandTracker& operator=(const HandTracker&) = delete;

    // Met à jour la détection. En mode bridge, frame peut être vide.
    // outMask est rempli en mode CV (vide en mode bridge).
    DetectionResult process(const cv::Mat& frame, const HSVRange& hsvHint, cv::Mat& outMask);

    // Affiche le skeleton sur une frame (mode CV : contours, mode bridge : landmarks)
    void drawSkeleton(cv::Mat& frame) const;
    void drawDebug(cv::Mat& frame) const;

    [[nodiscard]] bool usesBridge()  const { return bridgePipe_ != nullptr; }
    [[nodiscard]] bool isReady()     const;
    [[nodiscard]] bool hasCvCamera() const { return cap_.isOpened(); }

    static constexpr int kInitFrames = 40;

private:
    // ---- Bridge MediaPipe ----
    FILE*       bridgePipe_    = nullptr;
    bool        bridgeReady_   = false;

    bool tryStartBridge(int cameraDevice);
    bool readFromBridge(LandmarkData& out);

    // ---- Mode CV (fallback) ----
    cv::VideoCapture                  cap_;
    cv::Ptr<cv::BackgroundSubtractor> bgSub_;
    int                               initCounter_ = 0;

    cv::Mat computeCvMask(const cv::Mat& frame, const HSVRange& hint);
    static bool findLargestContour(const cv::Mat& mask,
                                   std::vector<cv::Point>& contour, float& area);
    static std::vector<cv::Point2f> findFingertips(const std::vector<cv::Point>& contour,
                                                    const cv::Point2f& centroid,
                                                    float handHeight);

    // ---- Lissage commun ----
    Core::KalmanFilter2D smoother_;
    DetectionResult      lastResult_;

    // ---- Conversion landmarks → DetectionResult ----
    DetectionResult landmarksToResult(const LandmarkData& lm, int frameW, int frameH);
};

} // namespace Aura::Vision
