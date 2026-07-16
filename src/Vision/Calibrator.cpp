#include "Aura/Vision/Calibrator.hpp"
#include <iostream>
#include <algorithm>
#include <thread>

namespace Aura::Vision {

Calibrator::Calibrator(Camera& camera) : camera_(camera) {}

// --------------------------------------------------------------------------
// Helpers statiques
// --------------------------------------------------------------------------

int Calibrator::detectSkin(const cv::Mat& frame, cv::Mat& mask) {
    cv::Mat ycrcb;
    cv::cvtColor(frame, ycrcb, cv::COLOR_BGR2YCrCb);
    cv::inRange(ycrcb, cv::Scalar(0, 120, 60), cv::Scalar(255, 190, 150), mask);
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, {5, 5}));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, {7, 7}));
    return cv::countNonZero(mask);
}

bool Calibrator::collectSample(const cv::Mat& frame, const cv::Mat& skinMask,
                                SkinSample& out) {
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(skinMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) return false;

    size_t maxIdx = 0;
    double maxArea = 0.0;
    for (size_t i = 0; i < contours.size(); ++i) {
        double a = cv::contourArea(contours[i]);
        if (a > maxArea) { maxArea = a; maxIdx = i; }
    }
    if (maxArea < 500.0) return false;

    cv::Mat contourMask = cv::Mat::zeros(skinMask.size(), CV_8U);
    cv::drawContours(contourMask, contours, static_cast<int>(maxIdx),
                     cv::Scalar(255), cv::FILLED);

    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
    cv::Scalar mean = cv::mean(hsv, contourMask);
    out = {mean[0], mean[1], mean[2]};
    return true;
}

HSVRange Calibrator::buildRange(const std::vector<SkinSample>& samples,
                                 int hPad, int sPad, int vPad) {
    double sumH = 0, sumS = 0, sumV = 0;
    for (const auto& s : samples) { sumH += s.H; sumS += s.S; sumV += s.V; }
    double n = static_cast<double>(samples.size());
    double avgH = sumH / n, avgS = sumS / n, avgV = sumV / n;

    HSVRange r;
    r.H_min = static_cast<int>(std::max(0.0,   avgH - hPad));
    r.H_max = static_cast<int>(std::min(179.0,  avgH + hPad));
    r.S_min = static_cast<int>(std::max(0.0,   avgS - sPad));
    r.S_max = static_cast<int>(std::min(255.0,  avgS + sPad));
    r.V_min = static_cast<int>(std::max(0.0,   avgV - vPad));
    r.V_max = static_cast<int>(std::min(255.0,  avgV + vPad));
    return r;
}

// --------------------------------------------------------------------------
// Wizard guidé
// --------------------------------------------------------------------------

bool Calibrator::runGuidedWizard(HSVRange& outRange, int framesPerStep, int delayMs) {
    const std::vector<std::string> steps = {
        "Etape 1/3 — PAUME OUVERTE (doigts ecartés, face camera)",
        "Etape 2/3 — POING FERME (centre du cadre)",
        "Etape 3/3 — DOIGTS ECARTES (large, proche camera)",
    };

    cv::namedWindow("AURA Calibration", cv::WINDOW_NORMAL);
    cv::namedWindow("AURA Mask",        cv::WINDOW_NORMAL);

    std::vector<SkinSample> allSamples;

    for (size_t step = 0; step < steps.size(); ++step) {
        std::cout << "\n[Calibrator] " << steps[step] << "\n";
        int stepSamples = 0;

        for (int f = 0; f < framesPerStep; ++f) {
            cv::Mat frame;
            if (!camera_.captureFrame(frame)) continue;

            cv::Mat skinMask;
            int pixels = detectSkin(frame, skinMask);

            // UI : cadre de guidage jaune
            cv::rectangle(frame,
                          cv::Point(50, 50),
                          cv::Point(frame.cols - 50, frame.rows - 50),
                          cv::Scalar(0, 255, 255), 3);
            cv::putText(frame, steps[step],
                        cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX,
                        0.7, cv::Scalar(255, 255, 0), 2);
            cv::putText(frame,
                        "Frame " + std::to_string(f + 1) + "/" + std::to_string(framesPerStep),
                        cv::Point(frame.cols - 220, 30), cv::FONT_HERSHEY_SIMPLEX,
                        0.65, cv::Scalar(0, 255, 0), 1);

            SkinSample sample{};
            bool collected = (pixels > 500) && collectSample(frame, skinMask, sample);
            if (collected) {
                allSamples.push_back(sample);
                ++stepSamples;
                cv::putText(frame, "COLLECTE!",
                            cv::Point(frame.cols / 2 - 80, frame.rows / 2),
                            cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(0, 255, 0), 3);
            } else {
                cv::putText(frame, "Gardez la main dans le cadre",
                            cv::Point(10, frame.rows - 20),
                            cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(0, 165, 255), 2);
            }

            cv::imshow("AURA Calibration", frame);
            cv::imshow("AURA Mask", skinMask);
            if ((cv::waitKey(delayMs) & 0xFF) == 'q') {
                std::cout << "[Calibrator] Annulé par l'utilisateur.\n";
                cv::destroyWindow("AURA Calibration");
                cv::destroyWindow("AURA Mask");
                return false;
            }
        }
        std::cout << "[Calibrator] Etape " << (step + 1)
                  << " : " << stepSamples << " samples collectés.\n";

        // Courte pause entre les étapes
        cv::Mat pause = cv::Mat::zeros(80, 400, CV_8UC3);
        cv::putText(pause, "Etape suivante dans 1s...",
                    cv::Point(10, 45), cv::FONT_HERSHEY_SIMPLEX,
                    0.7, cv::Scalar(255, 255, 255), 1);
        cv::imshow("AURA Calibration", pause);
        cv::waitKey(200);
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

    cv::destroyWindow("AURA Calibration");
    cv::destroyWindow("AURA Mask");

    if (allSamples.empty()) {
        std::cerr << "[Calibrator] Echec : aucun sample collecté."
                     " Vérifiez l'éclairage et réessayez.\n";
        return false;
    }

    outRange = buildRange(allSamples);
    std::cout << "[Calibrator] Plage HSV calculée :"
              << " H=[" << outRange.H_min << "," << outRange.H_max << "]"
              << " S=[" << outRange.S_min << "," << outRange.S_max << "]"
              << " V=[" << outRange.V_min << "," << outRange.V_max << "]\n";
    return true;
}

// --------------------------------------------------------------------------
// Calibration automatique (sans guidance utilisateur)
// --------------------------------------------------------------------------

bool Calibrator::runAuto(HSVRange& outRange, int frames, int delayMs) {
    std::vector<SkinSample> samples;

    for (int i = 0; i < frames; ++i) {
        cv::Mat frame;
        if (!camera_.captureFrame(frame)) continue;

        cv::Mat skinMask;
        if (detectSkin(frame, skinMask) < 50) continue;

        SkinSample s{};
        if (collectSample(frame, skinMask, s))
            samples.push_back(s);

        cv::waitKey(delayMs);
    }

    // Fallback : différence inter-frame (détection de mouvement)
    if (samples.empty()) {
        cv::Mat prevGray;
        for (int i = 0; i < frames * 2; ++i) {
            cv::Mat frame;
            if (!camera_.captureFrame(frame)) continue;
            cv::Mat gray;
            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
            cv::GaussianBlur(gray, gray, {7, 7}, 0);

            if (!prevGray.empty()) {
                cv::Mat diff;
                cv::absdiff(gray, prevGray, diff);
                cv::threshold(diff, diff, 25, 255, cv::THRESH_BINARY);
                cv::morphologyEx(diff, diff, cv::MORPH_OPEN,
                                 cv::getStructuringElement(cv::MORPH_ELLIPSE, {5, 5}));

                SkinSample s{};
                if (collectSample(frame, diff, s))
                    samples.push_back(s);
            }
            prevGray = gray;
            cv::waitKey(delayMs);
        }
    }

    if (samples.empty()) {
        std::cerr << "[Calibrator] Calibration auto échouée : aucune peau détectée.\n";
        return false;
    }

    outRange = buildRange(samples);
    std::cout << "[Calibrator] Auto — plage HSV :"
              << " H=[" << outRange.H_min << "," << outRange.H_max << "]"
              << " S=[" << outRange.S_min << "," << outRange.S_max << "]"
              << " V=[" << outRange.V_min << "," << outRange.V_max << "]\n";
    return true;
}

} // namespace Aura::Vision
