#pragma once
#include "Aura/Vision/Types.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <string>

namespace Aura::App {

// Machine à états protégeant contre les faux positifs au démarrage.
//
//   IDLE ──(main stable N frames)──> WARMING_UP ──> ACTIVE
//   ACTIVE ──(main absente M frames)──> IDLE
//
// Aucune action n'est exécutée tant que la main n'est pas stablement
// positionnée dans le cadre.
struct ActivationGuard {
    enum class Phase { IDLE, WARMING_UP, ACTIVE };

    Phase phase        = Phase::IDLE;
    int   stableCount  = 0;
    int   missingCount = 0;

    static constexpr int   kActivateFrames   = 18;
    static constexpr int   kDeactivateFrames = 30;
    static constexpr float kMinHandArea      = 3000.f;

    void update(const Vision::DetectionResult& r) {
        bool good = r.found && r.area >= kMinHandArea;
        if (good) {
            ++stableCount;
            missingCount = 0;
        } else {
            ++missingCount;
            stableCount = std::max(0, stableCount - 2);
        }
        switch (phase) {
            case Phase::IDLE:
                if (stableCount > 2)                  phase = Phase::WARMING_UP;
                break;
            case Phase::WARMING_UP:
                if (stableCount >= kActivateFrames)   phase = Phase::ACTIVE;
                else if (missingCount >= 10)         { phase = Phase::IDLE; stableCount = 0; }
                break;
            case Phase::ACTIVE:
                if (missingCount >= kDeactivateFrames){ phase = Phase::IDLE; stableCount = 0; missingCount = 0; }
                break;
        }
    }

    void reset() { phase = Phase::IDLE; stableCount = missingCount = 0; }

    [[nodiscard]] bool isActive()    const { return phase == Phase::ACTIVE;     }
    [[nodiscard]] bool isWarmingUp() const { return phase == Phase::WARMING_UP; }
    [[nodiscard]] bool isIdle()      const { return phase == Phase::IDLE;       }
    [[nodiscard]] int  warmupPct()   const { return std::min(100, stableCount * 100 / kActivateFrames); }

    void drawOverlay(cv::Mat& frame) const {
        cv::Scalar color;
        std::string msg;
        switch (phase) {
            case Phase::IDLE:
                cv::putText(frame, "Montrez votre main pour activer",
                            {10, frame.rows - 12},
                            cv::FONT_HERSHEY_SIMPLEX, 0.55,
                            cv::Scalar(140, 140, 140), 1, cv::LINE_AA);
                return;
            case Phase::WARMING_UP:
                color = cv::Scalar(0, 165, 255);
                msg   = "Activation " + std::to_string(warmupPct()) + "%  — gardez la main";
                break;
            case Phase::ACTIVE:
                color = cv::Scalar(50, 230, 50);
                msg   = "ACTIF";
                break;
        }
        cv::rectangle(frame, {3, 3}, {frame.cols - 3, frame.rows - 3}, color, 4, cv::LINE_AA);
        cv::putText(frame, msg, {10, frame.rows - 12},
                    cv::FONT_HERSHEY_SIMPLEX, 0.65, color, 2, cv::LINE_AA);
    }
};

} // namespace Aura::App
