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
    , smoother_(1e-2f, 1e-1f)
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
// Lecture d'une ligne du bridge
// --------------------------------------------------------------------------

bool HandTracker::readFromBridge(LandmarkData& out) {
    if (!bridgePipe_) return false;

    char buf[2048];
    if (!fgets(buf, sizeof(buf), bridgePipe_)) {
        std::cerr << "[HandTracker] Bridge fermé\n";
        pclose(bridgePipe_);
        bridgePipe_ = nullptr;
        return false;
    }

    if (strncmp(buf, "NONE", 4) == 0) {
        out.found = false;
        return true;
    }
    if (strncmp(buf, "QUIT", 4) == 0) {
        out.found = false;
        pclose(bridgePipe_);
        bridgePipe_ = nullptr;
        return false;
    }
    if (strncmp(buf, "FOUND", 5) == 0) {
        out.found = true;
        char* s = buf + 6;
        for (int i = 0; i < 21; ++i) {
            out.pts[i].x = strtof(s, &s);
            out.pts[i].y = strtof(s, &s);
            out.pts[i].z = strtof(s, &s);
        }
        return true;
    }
    return true; // ligne inconnue → ignorer
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
                                                int frameW, int frameH) {
    DetectionResult r;
    r.landmarks = lm;

    if (!lm.found) {
        r.found          = false;
        r.smoothedPoint  = smoother_.predict();
        lastResult_      = r;
        return r;
    }

    r.found = true;

    // Position de la paume (normalisée → pixels)
    cv::Point2f palmNorm = lm.palmCenter();
    r.rawPoint = {palmNorm.x * static_cast<float>(frameW),
                  palmNorm.y * static_cast<float>(frameH)};
    r.smoothedPoint = smoother_.update(r.rawPoint);

    // Fingertips en pixels
    constexpr int tipIds[5] = {4, 8, 12, 16, 20};
    r.fingertips.clear();
    for (int id : tipIds) {
        r.fingertips.push_back({lm.pts[id].x * static_cast<float>(frameW),
                                lm.pts[id].y * static_cast<float>(frameH)});
    }

    // Comptage fiable des doigts étendus (index, majeur, annulaire, auriculaire)
    // TIP.y < PIP.y  ↔  le bout du doigt est AU-DESSUS du PIP dans l'image
    constexpr int pips[4] = {6, 10, 14, 18};
    constexpr int tips[4] = {8, 12, 16, 20};
    int count = 0;
    for (int f = 0; f < 4; ++f) {
        if (lm.pts[tips[f]].y < lm.pts[pips[f]].y) ++count;
    }
    r.fingerCount = count;

    // Fausse aire approximative pour ActivationGuard (basée sur la taille de la main)
    float dx = lm.pts[0].x - lm.pts[9].x;  // wrist → milieu main
    float dy = lm.pts[0].y - lm.pts[9].y;
    float handSizeNorm = std::sqrt(dx*dx + dy*dy);
    r.area = handSizeNorm * static_cast<float>(frameW) * handSizeNorm * static_cast<float>(frameH);

    lastResult_ = r;
    return r;
}

// --------------------------------------------------------------------------
// process() — point d'entrée principal
// --------------------------------------------------------------------------

bool HandTracker::isReady() const {
    if (bridgePipe_) return bridgeReady_;
    return initCounter_ >= kInitFrames;
}

DetectionResult HandTracker::process(const cv::Mat& frame,
                                      const HSVRange& hsvHint,
                                      cv::Mat& outMask) {
    // ---- Mode bridge MediaPipe ----
    if (bridgePipe_) {
        outMask = cv::Mat(); // pas de masque en mode bridge
        LandmarkData lm;
        if (!readFromBridge(lm)) {
            DetectionResult r;
            r.smoothedPoint = smoother_.predict();
            lastResult_ = r;
            return r;
        }
        // Taille de frame par défaut si on n'a pas de frame C++
        int fw = frame.empty() ? 640 : frame.cols;
        int fh = frame.empty() ? 480 : frame.rows;
        return landmarksToResult(lm, fw, fh);
    }

    // ---- Mode OpenCV fallback ----
    DetectionResult result;

    cv::Mat src = frame.empty() ? cv::Mat() : frame;
    if (src.empty() && cap_.isOpened()) {
        cap_ >> src;
    }
    if (src.empty()) {
        result.found        = false;
        result.smoothedPoint = smoother_.predict();
        lastResult_         = result;
        return result;
    }

    outMask = computeCvMask(src, hsvHint);

    float area = 0.f;
    if (!findLargestContour(outMask, result.contour, area)) {
        result.found        = false;
        result.smoothedPoint = smoother_.predict();
        lastResult_         = result;
        return result;
    }

    cv::Moments m = cv::moments(result.contour);
    if (m.m00 <= 1.0) {
        result.found        = false;
        result.smoothedPoint = smoother_.predict();
        lastResult_         = result;
        return result;
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

    result.smoothedPoint = smoother_.update(result.rawPoint);
    lastResult_ = result;
    return result;
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
    if (!lastResult_.found) return;

    if (lastResult_.landmarks.found) {
        // Mode bridge : skeleton MediaPipe précis
        const auto& lm = lastResult_.landmarks;
        int w = frame.cols, h = frame.rows;

        auto pt = [&](int idx) -> cv::Point {
            return {static_cast<int>(lm.pts[idx].x * w),
                    static_cast<int>(lm.pts[idx].y * h)};
        };

        // Connexions
        static constexpr std::pair<int,int> connections[] = {
            {0,1},{1,2},{2,3},{3,4},        // Thumb
            {0,5},{5,6},{6,7},{7,8},        // Index
            {0,9},{9,10},{10,11},{11,12},   // Middle
            {0,13},{13,14},{14,15},{15,16}, // Ring
            {0,17},{17,18},{18,19},{19,20}, // Pinky
            {5,9},{9,13},{13,17},           // Palm
        };
        for (auto [a,b] : connections) {
            cv::line(frame, pt(a), pt(b), cv::Scalar(0, 210, 255), 2, cv::LINE_AA);
        }

        // Noeuds
        static constexpr int tipIds[5] = {4, 8, 12, 16, 20};
        for (int i = 0; i < 21; ++i) {
            bool isTip = false;
            for (int t : tipIds) if (t == i) { isTip = true; break; }
            cv::Scalar col = isTip ? cv::Scalar(0,255,100) : cv::Scalar(200,200,200);
            int r = isTip ? 8 : 4;
            cv::circle(frame, pt(i), r, col, -1, cv::LINE_AA);
            cv::circle(frame, pt(i), r, {255,255,255}, 1, cv::LINE_AA);
        }
        // Paume (wrist)
        cv::circle(frame, pt(0), 12, cv::Scalar(0,140,255), -1, cv::LINE_AA);
        cv::circle(frame, pt(0), 12, {255,255,255}, 2, cv::LINE_AA);
    } else {
        // Mode CV : skeleton simplifié (hull + fingertips)
        cv::Point2f palm = lastResult_.smoothedPoint;
        for (const auto& tip : lastResult_.fingertips) {
            cv::line(frame, palm, tip, cv::Scalar(0,220,255), 2, cv::LINE_AA);
        }
        cv::circle(frame, palm, 14, cv::Scalar(0,140,255), -1, cv::LINE_AA);
        for (const auto& tip : lastResult_.fingertips) {
            cv::circle(frame, tip, 9, cv::Scalar(0,255,100), -1, cv::LINE_AA);
        }
    }
}

void HandTracker::drawDebug(cv::Mat& frame) const {
    if (!lastResult_.found) {
        cv::putText(frame, "Aucune main",
                    {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.8,
                    cv::Scalar(0,0,255), 2, cv::LINE_AA);
        return;
    }
    std::string info = "Doigts: " + std::to_string(lastResult_.fingerCount)
                     + (usesBridge() ? " [MediaPipe]" : " [CV]");
    cv::putText(frame, info, {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(0,255,0), 2, cv::LINE_AA);
}

} // namespace Aura::Vision
