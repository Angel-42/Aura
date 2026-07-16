#include "Aura/Vision/HandTracker.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <string>

namespace Aura::Vision {

// --------------------------------------------------------------------------
// Helpers pour trouver le script bridge
// --------------------------------------------------------------------------

static std::string findBridgeScript() {
    const std::vector<std::string> candidates = {
        "scripts/hand_bridge.py",
        "../scripts/hand_bridge.py",
    };
    for (const auto& p : candidates) {
        if (std::filesystem::exists(p)) return p;
    }
    return {};
}

static bool mediapipeAvailable() {
    int ret = std::system("python3 -c 'import mediapipe' >/dev/null 2>&1");
    return ret == 0;
}

// --------------------------------------------------------------------------
// Construction / destruction
// --------------------------------------------------------------------------

HandTracker::HandTracker()
    : bgSub_(cv::createBackgroundSubtractorMOG2(300, 25.0, false))
    , smoothers_{}   // chaque KalmanFilter2D s'initialise avec ses params par défaut
{
    if (!tryStartBridge(0)) {
        std::cerr << "[HandTracker] Bridge indisponible — mode OpenCV BGSub\n";
        cap_.open(0);
        if (!cap_.isOpened()) {
            std::cerr << "[HandTracker] Impossible d'ouvrir la caméra 0\n";
        } else {
            cap_.set(cv::CAP_PROP_FRAME_WIDTH,  640);
            cap_.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        }
    }
}

HandTracker::~HandTracker() {
    if (bridgePipe_) {
        pclose(bridgePipe_);
        bridgePipe_ = nullptr;
    }
    if (cap_.isOpened()) cap_.release();
}

bool HandTracker::tryStartBridge(int cameraDevice) {
    if (!mediapipeAvailable()) {
        std::cerr << "[HandTracker] MediaPipe non disponible (pip3 install mediapipe)\n";
        return false;
    }
    std::string script = findBridgeScript();
    if (script.empty()) {
        std::cerr << "[HandTracker] scripts/hand_bridge.py introuvable\n";
        return false;
    }
    std::string cmd = "python3 " + script
                    + " " + std::to_string(cameraDevice)
                    + " 2>/dev/null";
    bridgePipe_ = popen(cmd.c_str(), "r");
    if (!bridgePipe_) {
        std::cerr << "[HandTracker] Échec popen du bridge\n";
        return false;
    }
    // Lire quelques lignes pour confirmer que le bridge a démarré
    char buf[128];
    for (int i = 0; i < 5; ++i) {
        if (fgets(buf, sizeof(buf), bridgePipe_)) {
            bridgeReady_ = true;
            std::cout << "[HandTracker] Bridge MediaPipe actif\n";
            return true;
        }
    }
    pclose(bridgePipe_);
    bridgePipe_ = nullptr;
    return false;
}

// --------------------------------------------------------------------------
// Lecture de toutes les mains d'une frame depuis le bridge
// --------------------------------------------------------------------------

std::vector<DetectionResult> HandTracker::readAllFromBridge(int fw, int fh) {
    if (!bridgePipe_) return {};

    std::vector<DetectionResult> results;
    char buf[4096];

    while (fgets(buf, sizeof(buf), bridgePipe_)) {
        if (strncmp(buf, "NONE", 4) == 0) break;       // frame sans main

        if (strncmp(buf, "QUIT", 4) == 0) {
            pclose(bridgePipe_);
            bridgePipe_ = nullptr;
            break;
        }

        if (strncmp(buf, "FRAME_END", 9) == 0) break;  // toutes les mains lues

        if (strncmp(buf, "HAND ", 5) == 0) {
            char* s = buf + 5;

            // Latéralité : "LEFT " ou "RIGHT " ou "UNKNOWN "
            Core::HandSide side = Core::HandSide::UNKNOWN;
            if (strncmp(s, "LEFT ", 5) == 0)  { side = Core::HandSide::LEFT;  s += 5; }
            else if (strncmp(s, "RIGHT ", 6) == 0) { side = Core::HandSide::RIGHT; s += 6; }
            else if (strncmp(s, "UNKNOWN ", 8) == 0) { s += 8; }

            LandmarkData lm;
            lm.found = true;
            for (int i = 0; i < 21; ++i) {
                lm.pts[i].x = strtof(s, &s);
                lm.pts[i].y = strtof(s, &s);
                lm.pts[i].z = strtof(s, &s);
            }

            int idx = (side == Core::HandSide::RIGHT) ? 1 : 0;
            results.push_back(landmarksToResult(lm, side, idx, fw, fh));
        }
        // lignes inconnues → ignorer
    }

    if (!bridgePipe_) return results;

    lastResults_ = results;
    return results;
}

// --------------------------------------------------------------------------
// Conversion landmarks → DetectionResult
//
// Comptage des doigts :
//   Non-pouce : TIP.y < PIP.y  (TIP est plus haut dans l'image)
//   Pouce      : TIP.x < IP.x  (pouce étendu vers la gauche, main droite)
//   On ignore le pouce dans le total pour rester robuste sans info de main D/G
// --------------------------------------------------------------------------

DetectionResult HandTracker::landmarksToResult(const LandmarkData& lm,
                                                Core::HandSide side,
                                                int smootherIdx,
                                                int frameW, int frameH) {
    DetectionResult r;
    r.landmarks = lm;
    r.side      = side;

    if (!lm.found) {
        r.found         = false;
        r.smoothedPoint = smoothers_[smootherIdx].predict();
        return r;
    }

    r.found = true;

    cv::Point2f palmNorm = lm.palmCenter();
    r.rawPoint = {palmNorm.x * static_cast<float>(frameW),
                  palmNorm.y * static_cast<float>(frameH)};
    r.smoothedPoint = smoothers_[smootherIdx].update(r.rawPoint);

    constexpr int tipIds[5] = {4, 8, 12, 16, 20};
    r.fingertips.clear();
    for (int id : tipIds) {
        r.fingertips.push_back({lm.pts[id].x * static_cast<float>(frameW),
                                lm.pts[id].y * static_cast<float>(frameH)});
    }

    constexpr int pips[4] = {6, 10, 14, 18};
    constexpr int tips[4] = {8, 12, 16, 20};
    int count = 0;
    for (int f = 0; f < 4; ++f) {
        if (lm.pts[tips[f]].y < lm.pts[pips[f]].y) ++count;
    }
    r.fingerCount = count;

    float dx = lm.pts[0].x - lm.pts[9].x;
    float dy = lm.pts[0].y - lm.pts[9].y;
    float handSizeNorm = std::sqrt(dx*dx + dy*dy);
    r.area = handSizeNorm * static_cast<float>(frameW) * handSizeNorm * static_cast<float>(frameH);

    return r;
}

// --------------------------------------------------------------------------
// process() — point d'entrée principal
// --------------------------------------------------------------------------

bool HandTracker::isReady() const {
    if (bridgePipe_) return bridgeReady_;
    return initCounter_ >= kInitFrames;
}

std::vector<DetectionResult> HandTracker::process(const cv::Mat& frame,
                                                   const HSVRange& hsvHint,
                                                   cv::Mat& outMask) {
    // ---- Mode bridge MediaPipe ----
    if (bridgePipe_) {
        outMask = cv::Mat();
        int fw = frame.empty() ? 640 : frame.cols;
        int fh = frame.empty() ? 480 : frame.rows;
        return readAllFromBridge(fw, fh);
    }

    // ---- Mode OpenCV fallback (une seule main, contour) ----
    cv::Mat src = frame.empty() ? cv::Mat() : frame;
    if (src.empty() && cap_.isOpened()) cap_ >> src;
    if (src.empty()) {
        lastResults_ = {};
        return {};
    }

    outMask = computeCvMask(src, hsvHint);

    DetectionResult result;
    result.side = Core::HandSide::UNKNOWN;

    float area = 0.f;
    if (!findLargestContour(outMask, result.contour, area)) {
        lastResults_ = {};
        return {};
    }

    cv::Moments m = cv::moments(result.contour);
    if (m.m00 <= 1.0) {
        lastResults_ = {};
        return {};
    }

    result.found    = true;
    result.area     = area;
    result.rawPoint = cv::Point2f(static_cast<float>(m.m10 / m.m00),
                                  static_cast<float>(m.m01 / m.m00));

    cv::convexHull(result.contour, result.hull);
    result.hullArea = static_cast<float>(cv::contourArea(result.hull));

    cv::Rect bbox = cv::boundingRect(result.contour);
    result.fingertips = findFingertips(result.contour, result.smoothedPoint,
                                       static_cast<float>(bbox.height));
    result.fingerCount = static_cast<int>(result.fingertips.size());

    result.smoothedPoint = smoothers_[0].update(result.rawPoint);
    lastResults_ = {result};
    return {result};
}

// --------------------------------------------------------------------------
// Mode CV — pipeline de masquage
// --------------------------------------------------------------------------

cv::Mat HandTracker::computeCvMask(const cv::Mat& frame, const HSVRange& /*hint*/) {
    cv::Mat bgMask;
    double lr = (initCounter_ < kInitFrames) ? -1.0 : 0.005;
    bgSub_->apply(frame, bgMask, lr);
    ++initCounter_;

    cv::morphologyEx(bgMask, bgMask, cv::MORPH_OPEN,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, {5, 5}));
    cv::morphologyEx(bgMask, bgMask, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, {9, 9}));

    cv::Mat ycrcb;
    cv::cvtColor(frame, ycrcb, cv::COLOR_BGR2YCrCb);
    cv::Mat skinMask;
    cv::inRange(ycrcb, cv::Scalar(0, 128, 70), cv::Scalar(255, 185, 135), skinMask);
    cv::morphologyEx(skinMask, skinMask, cv::MORPH_OPEN,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, {5, 5}));
    cv::morphologyEx(skinMask, skinMask, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, {9, 9}));

    cv::Mat combined;
    cv::bitwise_and(bgMask, skinMask, combined);
    constexpr int kMinPixels = 1500;
    if (cv::countNonZero(combined) >= kMinPixels) return combined;
    if (cv::countNonZero(skinMask) >= kMinPixels) return skinMask;
    return bgMask;
}

bool HandTracker::findLargestContour(const cv::Mat& mask,
                                      std::vector<cv::Point>& contour, float& area) {
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) return false;
    double maxArea = 0.0; std::size_t maxIdx = 0;
    for (std::size_t i = 0; i < contours.size(); ++i) {
        double a = cv::contourArea(contours[i]);
        if (a > maxArea) { maxArea = a; maxIdx = i; }
    }
    if (maxArea < 800.0) return false;
    contour = std::move(contours[maxIdx]);
    area = static_cast<float>(maxArea);
    return true;
}

std::vector<cv::Point2f> HandTracker::findFingertips(const std::vector<cv::Point>& contour,
                                                       const cv::Point2f& centroid,
                                                       float handHeight) {
    int n = static_cast<int>(contour.size());
    if (n < 40) return {};
    std::vector<float> dist(n);
    float maxDist = 0.f;
    for (int i = 0; i < n; ++i) {
        float dx = static_cast<float>(contour[i].x) - centroid.x;
        float dy = static_cast<float>(contour[i].y) - centroid.y;
        dist[i] = std::sqrt(dx*dx + dy*dy);
        maxDist = std::max(maxDist, dist[i]);
    }
    if (maxDist < 20.f) return {};
    int sr = std::max(6, n/25);
    std::vector<float> smoothed(n, 0.f);
    for (int i = 0; i < n; ++i) {
        float sum = 0.f, w = 0.f;
        for (int j = -sr; j <= sr; ++j) {
            float wj = 1.f - std::abs(static_cast<float>(j))/static_cast<float>(sr+1);
            sum += wj * dist[(i+j+n)%n]; w += wj;
        }
        smoothed[i] = sum / w;
    }
    float thresh = maxDist * 0.62f;
    float yLimit = centroid.y + handHeight * 0.25f;
    int window = std::max(8, n/20);
    std::vector<std::pair<float,cv::Point2f>> candidates;
    for (int i = 0; i < n; ++i) {
        if (smoothed[i] < thresh) continue;
        if (static_cast<float>(contour[i].y) > yLimit) continue;
        bool isMax = true;
        for (int j = -window; j <= window && isMax; ++j) {
            if (j == 0) continue;
            if (smoothed[(i+j+n)%n] >= smoothed[i]) isMax = false;
        }
        if (isMax) candidates.push_back({smoothed[i],
            cv::Point2f(static_cast<float>(contour[i].x),
                        static_cast<float>(contour[i].y))});
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b){ return a.first > b.first; });
    float minSep = maxDist * 0.18f;
    std::vector<cv::Point2f> tips;
    for (auto& [d,p] : candidates) {
        bool tooClose = false;
        for (const auto& t : tips) {
            float dx=p.x-t.x, dy=p.y-t.y;
            if (std::sqrt(dx*dx+dy*dy) < minSep) { tooClose=true; break; }
        }
        if (!tooClose) tips.push_back(p);
        if (static_cast<int>(tips.size()) >= 5) break;
    }
    return tips;
}

// --------------------------------------------------------------------------
// Affichage
// --------------------------------------------------------------------------

void HandTracker::drawSkeleton(cv::Mat& frame) const {
    static constexpr std::pair<int,int> connections[] = {
        {0,1},{1,2},{2,3},{3,4},
        {0,5},{5,6},{6,7},{7,8},
        {0,9},{9,10},{10,11},{11,12},
        {0,13},{13,14},{14,15},{15,16},
        {0,17},{17,18},{18,19},{19,20},
        {5,9},{9,13},{13,17},
    };
    static constexpr int tipIds[5] = {4, 8, 12, 16, 20};

    for (const auto& result : lastResults_) {
        if (!result.found) continue;

        // Couleur selon la main : orange=LEFT, bleu=RIGHT, blanc=UNKNOWN
        cv::Scalar boneColor = (result.side == Core::HandSide::LEFT)  ? cv::Scalar(0, 210, 255)
                             : (result.side == Core::HandSide::RIGHT) ? cv::Scalar(255, 80, 50)
                             :                                           cv::Scalar(200, 200, 200);

        if (result.landmarks.found) {
            // Mode bridge : skeleton précis 21 points
            const auto& lm = result.landmarks;
            int w = frame.cols, h = frame.rows;
            auto pt = [&](int i) -> cv::Point {
                return {static_cast<int>(lm.pts[i].x * w),
                        static_cast<int>(lm.pts[i].y * h)};
            };
            for (auto [a, b] : connections)
                cv::line(frame, pt(a), pt(b), boneColor, 2, cv::LINE_AA);
            for (int i = 0; i < 21; ++i) {
                bool isTip = false;
                for (int t : tipIds) if (t == i) { isTip = true; break; }
                cv::Scalar col = isTip ? cv::Scalar(0,255,100) : cv::Scalar(200,200,200);
                int r = isTip ? 8 : 4;
                cv::circle(frame, pt(i), r, col, -1, cv::LINE_AA);
                cv::circle(frame, pt(i), r, {255,255,255}, 1, cv::LINE_AA);
            }
            cv::circle(frame, pt(0), 12, boneColor, -1, cv::LINE_AA);
            cv::circle(frame, pt(0), 12, {255,255,255}, 2, cv::LINE_AA);

            // Label latéralité
            std::string label = (result.side == Core::HandSide::LEFT)  ? "L"
                              : (result.side == Core::HandSide::RIGHT) ? "R" : "";
            if (!label.empty()) {
                cv::putText(frame, label, pt(0) + cv::Point(14, -14),
                            cv::FONT_HERSHEY_SIMPLEX, 0.7, boneColor, 2, cv::LINE_AA);
            }
        } else {
            // Mode CV : skeleton simplifié
            cv::Point2f palm = result.smoothedPoint;
            for (const auto& tip : result.fingertips)
                cv::line(frame, palm, tip, boneColor, 2, cv::LINE_AA);
            cv::circle(frame, palm, 14, boneColor, -1, cv::LINE_AA);
            for (const auto& tip : result.fingertips)
                cv::circle(frame, tip, 9, cv::Scalar(0,255,100), -1, cv::LINE_AA);
        }
    }
}

void HandTracker::drawDebug(cv::Mat& frame) const {
    if (lastResults_.empty()) {
        cv::putText(frame, "Aucune main",
                    {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.8,
                    cv::Scalar(0,0,255), 2, cv::LINE_AA);
        return;
    }
    std::string info;
    for (const auto& r : lastResults_) {
        if (!r.found) continue;
        std::string side = (r.side == Core::HandSide::LEFT)  ? "L:"
                         : (r.side == Core::HandSide::RIGHT) ? "R:" : "";
        info += side + std::to_string(r.fingerCount) + "f  ";
    }
    info += usesBridge() ? "[MediaPipe]" : "[CV]";
    cv::putText(frame, info, {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(0,255,0), 2, cv::LINE_AA);
}

} // namespace Aura::Vision
