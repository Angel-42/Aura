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
#include <array>
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
    float speed    = 1.5f;
    float deadzone = 0.02f;
    bool  absolute = false;
};

// État indépendant par main — machine d'état drag/scroll/activation.
struct HandState {
    Vision::GestureDetector detector{};
    ActivationGuard         guard{};

    Core::GestureType lastGesture = Core::GestureType::NONE;
    std::chrono::steady_clock::time_point lastActionTime{};

    cv::Point2f lastHandPos = {-1.f, -1.f};  // pour le delta curseur relatif

    bool isDragging     = false;
    int  fistFrameCount = 0;

    bool  twoFingerScrollActive = false;
    int   twoFingerHoldFrames   = 0;
    float lastScrollPosY        = 0.f;
    float scrollAccumulator     = 0.f;
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
    Core::EventQueue<Core::GestureEvent> queue_;
    Input::Mapper                        mapper_;
    Input::Controller                    controller_;

    Vision::HSVRange hsvRange_;

    // [0] = LEFT / UNKNOWN,  [1] = RIGHT
    std::array<HandState, 2> handStates_;

    // Curseur virtuel partagé (une seule position physique de souris)
    cv::Point2f virtualCursor_ = {0.f, 0.f};

    static constexpr int kDragActivateFrames   = 20;
    static constexpr int kScrollActivateFrames = 10;
    static constexpr int kActionCooldownMs     = 350;

    // Initialisation
    bool init();
    void setupCalibration();
    void setupMapping();
    void setupDebugUI();

    // Boucle principale
    void processFrame(const cv::Mat& frame);
    void processHand(HandState& hs, const Vision::DetectionResult& result,
                     int frameW, int frameH);

    // Handlers par main
    void handleMovement(HandState& hs, const Vision::DetectionResult& result,
                        int frameW, int frameH, Core::GestureType gesture);
    void handleDrag(HandState& hs, Core::GestureType gesture);
    void releaseDrag(HandState& hs);
    void handleTwoFingerScroll(HandState& hs, Core::GestureType gesture,
                               const Vision::DetectionResult& result);

    // Dispatch
    void dispatchEvent(HandState& hs, const Core::GestureEvent& event,
                       int frameW, int frameH);
    void executeAction(const Input::Action& action,
                       const cv::Point2f& normPos, int frameW, int frameH);

    // Debug UI
    bool debugUICreated_ = false;
    void renderDebugOverlay(cv::Mat& frame,
                            const std::vector<Vision::DetectionResult>& results);
    void showDebug(cv::Mat& frame, const cv::Mat& mask,
                   const std::vector<Vision::DetectionResult>& results);
    static void trackbarCb(int, void* ud);
    void updateRangeFromTrackbars();
};

} // namespace Aura::App
