#pragma once
#include "Aura/App/ActivationGuard.hpp"
#include "Aura/Vision/Camera.hpp"
#include "Aura/Vision/HandTracker.hpp"
#include "Aura/Vision/GestureDetector.hpp"
#include "Aura/Vision/Calibrator.hpp"
#include "Aura/Core/GestureEvent.hpp"
#include "Aura/Core/EventQueue.hpp"
#include "Aura/Input/Controller.hpp"
#include "Aura/Input/Mapper.hpp"
#include "Aura/Config/CalibConfig.hpp"
#include <chrono>

namespace Aura::App {

struct RunnerOptions {
    bool verbose      = false;
    bool debug        = false;
    bool inputEnabled = true;
    int  cameraDevice = 0;
    std::string loadCalib;
    std::string saveCalib;
    bool autoCalib    = false;

    // Sensibilité curseur
    float speed    = 1.5f;   // multiplicateur de vitesse (mode relatif)
    float deadzone = 0.02f;  // zone morte en fraction de frame (évite le tremblement)
    bool  absolute = false;  // true = main mappe directement l'écran (ancien comportement)
};

class AuraRunner {
public:
    explicit AuraRunner(RunnerOptions opts);
    void run();
    Core::EventQueue<Core::GestureEvent>& eventQueue() { return queue_; }

private:
    RunnerOptions opts_;

    Config::CalibConfig                  calibConfig_;
    Vision::Camera                       camera_;
    Vision::HandTracker                  tracker_;
    Vision::GestureDetector              detector_;
    Core::EventQueue<Core::GestureEvent> queue_;
    Input::Mapper                        mapper_;
    Input::Controller                    controller_;
    ActivationGuard                      activation_;

    Vision::HSVRange  hsvRange_;
    Core::GestureType lastGesture_    = Core::GestureType::NONE;
    std::chrono::steady_clock::time_point lastActionTime_{};

    // Curseur relatif
    cv::Point2f lastHandPos_   = {-1.f, -1.f};  // -1 = non initialisé
    cv::Point2f virtualCursor_ = {0.f, 0.f};

    // Drag (FIST maintenu)
    bool isDragging_     = false;
    int  fistFrameCount_ = 0;
    static constexpr int kDragActivateFrames = 20;

    // Scroll continu (TWO_FINGERS maintenu)
    bool  twoFingerScrollActive_ = false;
    int   twoFingerHoldFrames_   = 0;
    float lastScrollPosY_        = 0.f;
    float scrollAccumulator_     = 0.f;
    static constexpr int kScrollActivateFrames = 10;  // ~0.33s @ 30fps

    static constexpr int kActionCooldownMs = 350;

    // Initialisation
    bool init();
    void setupCalibration();
    void setupMapping();
    void setupDebugUI();

    // Boucle principale — traitement par frame
    void processFrame(const cv::Mat& frame);
    void handleMovement(const Vision::DetectionResult& result, int frameW, int frameH,
                        Core::GestureType gesture);
    void handleDrag(Core::GestureType gesture);
    void releaseDrag();
    void handleTwoFingerScroll(Core::GestureType gesture, const Vision::DetectionResult& result);
    void renderDebugOverlay(cv::Mat& frame, bool canAct, const Core::GestureEvent& event);

    // Dispatch geste → action
    void dispatchEvent(const Core::GestureEvent& event, int frameW, int frameH);
    void executeAction(const Input::Action& action,
                       const cv::Point2f& normPos, int frameW, int frameH);

    // Debug UI
    bool debugUICreated_ = false;
    void showDebug(cv::Mat& frame, const cv::Mat& mask,
                   const Vision::DetectionResult& result,
                   const Core::GestureEvent& event);
    static void trackbarCb(int, void* ud);
    void updateRangeFromTrackbars();
};

} // namespace Aura::App
