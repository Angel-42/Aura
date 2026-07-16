#include "Aura/App/AuraRunner.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace Aura::App {

// --------------------------------------------------------------------------
// Construction
// --------------------------------------------------------------------------

AuraRunner::AuraRunner(RunnerOptions opts)
    : opts_(std::move(opts))
    , camera_(opts_.cameraDevice)
{}

// --------------------------------------------------------------------------
// Initialisation
// --------------------------------------------------------------------------

bool AuraRunner::init() {
    // En mode bridge, la caméra est gérée par Python — pas besoin de camera_ C++
    if (!tracker_.usesBridge() && !camera_.isOpened()) {
        std::cerr << "[AuraRunner] Caméra inaccessible (device " << opts_.cameraDevice << ")\n";
        return false;
    }
    // La calibration HSV n'est utile qu'en mode CV fallback
    if (!tracker_.usesBridge()) setupCalibration();
    setupMapping();
    if (opts_.debug && !tracker_.usesBridge()) setupDebugUI();
    if (tracker_.usesBridge())
        std::cout << "[AuraRunner] Mode MediaPipe bridge — fenêtre Python active\n";
    return true;
}

void AuraRunner::setupCalibration() {
    // 1. Calibration guidée demandée explicitement via --save-calib
    if (!opts_.saveCalib.empty()) {
        Vision::Calibrator calib(camera_);
        Vision::HSVRange range;
        if (calib.runGuidedWizard(range)) {
            calibConfig_.save(opts_.saveCalib, range);
            hsvRange_ = range;
        } else {
            std::cerr << "[AuraRunner] Calibration annulée, valeurs par défaut utilisées.\n";
        }
        return;
    }

    // 2. Calibration auto
    if (opts_.autoCalib) {
        Vision::Calibrator calib(camera_);
        Vision::HSVRange range;
        if (calib.runAuto(range)) {
            calibConfig_.save("default", range);
            hsvRange_ = range;
        }
        return;
    }

    // 3. Chargement d'un preset explicite
    const std::string& name = opts_.loadCalib.empty() ? "default" : opts_.loadCalib;

    // 4. Premier lancement : calibration default inexistante → wizard obligatoire
    if (!calibConfig_.exists(name)) {
        std::cout << "\n╔══════════════════════════════════════════════╗\n"
                  << "║  AURA — Premier lancement détecté             ║\n"
                  << "║  Calibration requise pour votre peau/éclairage ║\n"
                  << "╚══════════════════════════════════════════════╝\n\n";
        Vision::Calibrator calib(camera_);
        Vision::HSVRange range;
        if (calib.runGuidedWizard(range)) {
            calibConfig_.save(name, range);
            hsvRange_ = range;
        } else {
            std::cerr << "[AuraRunner] Wizard ignoré, valeurs par défaut utilisées.\n";
        }
        return;
    }

    // 5. Charger le preset existant
    calibConfig_.load(name, hsvRange_);
}

void AuraRunner::setupMapping() {
    mapper_.loadDefault();
}

void AuraRunner::setupDebugUI() {
    cv::namedWindow("AURA Feed", cv::WINDOW_NORMAL);
    cv::namedWindow("AURA Mask", cv::WINDOW_NORMAL);
    cv::namedWindow("AURA Trackbars", cv::WINDOW_NORMAL);

    // Trackbars HSV pour ajustement en temps réel
    cv::createTrackbar("H_min", "AURA Trackbars", nullptr, 179, trackbarCb, this);
    cv::createTrackbar("H_max", "AURA Trackbars", nullptr, 179, trackbarCb, this);
    cv::createTrackbar("S_min", "AURA Trackbars", nullptr, 255, trackbarCb, this);
    cv::createTrackbar("S_max", "AURA Trackbars", nullptr, 255, trackbarCb, this);
    cv::createTrackbar("V_min", "AURA Trackbars", nullptr, 255, trackbarCb, this);
    cv::createTrackbar("V_max", "AURA Trackbars", nullptr, 255, trackbarCb, this);

    cv::setTrackbarPos("H_min", "AURA Trackbars", hsvRange_.H_min);
    cv::setTrackbarPos("H_max", "AURA Trackbars", hsvRange_.H_max);
    cv::setTrackbarPos("S_min", "AURA Trackbars", hsvRange_.S_min);
    cv::setTrackbarPos("S_max", "AURA Trackbars", hsvRange_.S_max);
    cv::setTrackbarPos("V_min", "AURA Trackbars", hsvRange_.V_min);
    cv::setTrackbarPos("V_max", "AURA Trackbars", hsvRange_.V_max);

    debugUICreated_ = true;
}

void AuraRunner::trackbarCb(int, void* ud) {
    static_cast<AuraRunner*>(ud)->updateRangeFromTrackbars();
}

void AuraRunner::updateRangeFromTrackbars() {
    hsvRange_.H_min = cv::getTrackbarPos("H_min", "AURA Trackbars");
    hsvRange_.H_max = cv::getTrackbarPos("H_max", "AURA Trackbars");
    hsvRange_.S_min = cv::getTrackbarPos("S_min", "AURA Trackbars");
    hsvRange_.S_max = cv::getTrackbarPos("S_max", "AURA Trackbars");
    hsvRange_.V_min = cv::getTrackbarPos("V_min", "AURA Trackbars");
    hsvRange_.V_max = cv::getTrackbarPos("V_max", "AURA Trackbars");
}

// --------------------------------------------------------------------------
// Boucle principale
// --------------------------------------------------------------------------

void AuraRunner::run() {
    if (!init()) return;

    std::cout << "\n=== AURA Gesture Control ===\n";
    std::cout << "Input : " << (opts_.inputEnabled ? "activé" : "désactivé (--no-input)") << "\n";
    std::cout << "Debug : " << (opts_.debug ? "activé (--debug)" : "désactivé") << "\n";
    if (!tracker_.usesBridge()) {
        std::cout << "\nINIT : " << Vision::HandTracker::kInitFrames
                  << " frames d'apprentissage du fond — retirez la main du cadre !\n";
    }
    std::cout << "Appuyez sur 'q' pour quitter.\n\n";

    while (true) {
        cv::Mat frame;
        if (tracker_.usesBridge()) {
            // Bridge mode : frame vide, la caméra appartient au processus Python
            processFrame(frame);
            // 'q' dans la fenêtre Python ferme le bridge ; on vérifie si le pipe est mort
            if (!tracker_.usesBridge()) break;
            if (cv::waitKey(1) == 'q') break;
        } else {
            if (!camera_.captureFrame(frame)) {
                std::cerr << "[AuraRunner] Impossible de lire la frame.\n";
                break;
            }
            processFrame(frame);
            int key = cv::waitKey(30);
            if ((key & 0xFF) == 'q') break;
            if (opts_.debug && (key & 0xFF) == 's') {
                calibConfig_.save("default", hsvRange_);
                std::cout << "[AuraRunner] Calibration sauvegardée.\n";
            }
        }
    }

    cv::destroyAllWindows();
    if (!tracker_.usesBridge()) camera_.release();
}

// --------------------------------------------------------------------------
// Traitement d'une frame
// --------------------------------------------------------------------------

void AuraRunner::processFrame(const cv::Mat& frame) {
    cv::Mat mask;
    Vision::DetectionResult result = tracker_.process(frame, hsvRange_, mask);

    const int frameW = frame.empty() ? 640 : frame.cols;
    const int frameH = frame.empty() ? 480 : frame.rows;

    if (tracker_.isReady()) activation_.update(result);
    const bool canAct = tracker_.isReady() && activation_.isActive();

    Core::GestureEvent event;
    if (canAct) {
        event = detector_.classify(result, frameW, frameH);
        handleMovement(result, frameW, frameH, event.type);
        handleDrag(event.type);
        handleTwoFingerScroll(event.type, result);
        // FIST et TWO_FINGERS sont gérés en dur (drag / scroll) — pas via le mapper
        if (event.type != Core::GestureType::NONE &&
            event.type != Core::GestureType::FIST &&
            event.type != Core::GestureType::TWO_FINGERS) {
            queue_.push(event);
            dispatchEvent(event, frameW, frameH);
        }
    } else {
        releaseDrag();
        twoFingerScrollActive_ = false;
        twoFingerHoldFrames_   = 0;
        scrollAccumulator_     = 0.f;
        if (activation_.isIdle()) {
            detector_    = Vision::GestureDetector{};
            lastGesture_ = Core::GestureType::NONE;
        }
    }

    if (opts_.debug && !frame.empty()) {
        cv::Mat display = frame.clone();
        renderDebugOverlay(display, canAct, event);
        showDebug(display, mask, result, event);
    }

    if (opts_.verbose && canAct) {
        std::cout << "\r["
                  << Core::gestureName(event.type) << "] "
                  << "doigts=" << result.fingerCount
                  << " aire=" << static_cast<int>(result.area)
                  << " pos=(" << static_cast<int>(result.smoothedPoint.x)
                  << "," << static_cast<int>(result.smoothedPoint.y) << ")  "
                  << std::flush;
    }
}

void AuraRunner::handleMovement(const Vision::DetectionResult& result,
                                 int frameW, int frameH,
                                 Core::GestureType gesture) {
    if (!opts_.inputEnabled || !controller_.available()) return;

    // Geler le curseur pendant les gestes d'action — seules les poses de navigation
    // et le drag laissent le curseur bouger.
    switch (gesture) {
        case Core::GestureType::OPEN_PALM:
        case Core::GestureType::POINT:
        case Core::GestureType::FIST:
        case Core::GestureType::NONE:
            break;
        default:
            lastHandPos_ = result.smoothedPoint;  // garder à jour sans bouger
            return;
    }

    if (opts_.absolute) {
        // Mode absolu : main mappe directement l'écran (comportement d'origine)
        controller_.moveMouse(static_cast<int>(result.smoothedPoint.x),
                              static_cast<int>(result.smoothedPoint.y),
                              frameW, frameH);
        lastHandPos_ = result.smoothedPoint;
        return;
    }

    // Mode relatif : delta de position × speed, avec zone morte
    const cv::Point2f& current = result.smoothedPoint;

    if (lastHandPos_.x < 0.f) {
        // Premier frame — initialiser sans bouger
        lastHandPos_   = current;
        virtualCursor_ = current;
        return;
    }

    float dx = current.x - lastHandPos_.x;
    float dy = current.y - lastHandPos_.y;
    lastHandPos_ = current;

    // Zone morte : ignorer les micro-tremblements
    if (std::abs(dx) < opts_.deadzone * frameW) dx = 0.f;
    if (std::abs(dy) < opts_.deadzone * frameH) dy = 0.f;
    if (dx == 0.f && dy == 0.f) return;

    // Accumuler le déplacement amplifié
    virtualCursor_.x = std::clamp(virtualCursor_.x + dx * opts_.speed,
                                   0.f, static_cast<float>(frameW - 1));
    virtualCursor_.y = std::clamp(virtualCursor_.y + dy * opts_.speed,
                                   0.f, static_cast<float>(frameH - 1));

    controller_.moveMouse(static_cast<int>(virtualCursor_.x),
                          static_cast<int>(virtualCursor_.y),
                          frameW, frameH);
}

void AuraRunner::handleDrag(Core::GestureType gesture) {
    if (!opts_.inputEnabled || !controller_.available()) return;
    if (gesture == Core::GestureType::FIST) {
        if (++fistFrameCount_ >= kDragActivateFrames && !isDragging_) {
            controller_.mouseDown(Input::MouseButton::Left);
            isDragging_ = true;
        }
    } else {
        releaseDrag();
    }
}

void AuraRunner::releaseDrag() {
    if (!isDragging_) return;
    if (opts_.inputEnabled && controller_.available())
        controller_.mouseUp(Input::MouseButton::Left);
    isDragging_     = false;
    fistFrameCount_ = 0;
}

void AuraRunner::handleTwoFingerScroll(Core::GestureType gesture,
                                        const Vision::DetectionResult& result) {
    if (!opts_.inputEnabled || !controller_.available()) return;

    if (gesture == Core::GestureType::TWO_FINGERS) {
        ++twoFingerHoldFrames_;
        if (twoFingerHoldFrames_ >= kScrollActivateFrames) {
            if (!twoFingerScrollActive_) {
                twoFingerScrollActive_ = true;
                lastScrollPosY_        = result.smoothedPoint.y;
                scrollAccumulator_     = 0.f;
            } else {
                // Mouvement vers le haut (y décroît) = scroll positif
                float delta = lastScrollPosY_ - result.smoothedPoint.y;
                lastScrollPosY_     = result.smoothedPoint.y;
                scrollAccumulator_ += delta / 10.f;  // 10px = 1 unité de scroll
                int steps = static_cast<int>(scrollAccumulator_);
                if (steps != 0) {
                    controller_.scroll(steps);
                    scrollAccumulator_ -= static_cast<float>(steps);
                }
            }
        }
    } else {
        twoFingerScrollActive_ = false;
        twoFingerHoldFrames_   = 0;
        scrollAccumulator_     = 0.f;
    }
}

void AuraRunner::renderDebugOverlay(cv::Mat& frame, bool canAct,
                                     const Core::GestureEvent& event) {
    if (!tracker_.isReady()) {
        cv::putText(frame, "INIT... retirez votre main du cadre",
                    {10, 40}, cv::FONT_HERSHEY_SIMPLEX, 0.75,
                    cv::Scalar(0, 165, 255), 2, cv::LINE_AA);
        return;
    }
    if (canAct) tracker_.drawSkeleton(frame);
    tracker_.drawDebug(frame);
    if (isDragging_) {
        cv::putText(frame, "DRAG",
                    {10, 65}, cv::FONT_HERSHEY_SIMPLEX, 0.85,
                    cv::Scalar(0, 80, 255), 2, cv::LINE_AA);
    } else if (twoFingerScrollActive_) {
        cv::putText(frame, "SCROLL",
                    {10, 65}, cv::FONT_HERSHEY_SIMPLEX, 0.85,
                    cv::Scalar(0, 200, 255), 2, cv::LINE_AA);
    } else if (canAct && event.type != Core::GestureType::NONE) {
        cv::putText(frame, Core::gestureName(event.type),
                    {10, 65}, cv::FONT_HERSHEY_SIMPLEX, 0.85,
                    cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
    }
    activation_.drawOverlay(frame);
}

// --------------------------------------------------------------------------
// Dispatch : applique le mapping geste → action
// --------------------------------------------------------------------------

void AuraRunner::dispatchEvent(const Core::GestureEvent& event, int frameW, int frameH) {
    if (!opts_.inputEnabled || !controller_.available()) return;

    // MOUSE_MOVE est toujours géré en dehors du mapping (voir processFrame)
    if (event.type == Core::GestureType::NONE) return;

    // Cooldown pour éviter de répéter la même action en boucle
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now - lastActionTime_).count();

    // Les swipes et actions one-shot nécessitent un cooldown
    bool isSwipe = (event.type == Core::GestureType::SWIPE_LEFT  ||
                    event.type == Core::GestureType::SWIPE_RIGHT ||
                    event.type == Core::GestureType::SWIPE_UP    ||
                    event.type == Core::GestureType::SWIPE_DOWN);

    bool sameGesture = (event.type == lastGesture_);
    if (sameGesture && !isSwipe && ms < kActionCooldownMs) return;

    lastGesture_    = event.type;
    lastActionTime_ = now;

    Input::Action action = mapper_.actionFor(event.type);
    if (action.type != Input::ActionType::NONE) {
        executeAction(action, event.position, frameW, frameH);
    }
}

void AuraRunner::executeAction(const Input::Action& action,
                                const cv::Point2f& normPos,
                                int frameW, int frameH) {
    auto param = [&](int i, const std::string& def = "") -> std::string {
        return (i < static_cast<int>(action.params.size())) ? action.params[i] : def;
    };
    auto paramInt = [&](int i, int def = 1) -> int {
        try { return std::stoi(param(i, std::to_string(def))); }
        catch (...) { return def; }
    };

    switch (action.type) {
        case Input::ActionType::MOUSE_MOVE:
            // Déjà géré en continu dans processFrame
            break;

        case Input::ActionType::MOUSE_CLICK: {
            std::string btn = param(0, "LEFT");
            Input::MouseButton mb = (btn == "RIGHT")  ? Input::MouseButton::Right :
                                    (btn == "MIDDLE") ? Input::MouseButton::Middle :
                                                        Input::MouseButton::Left;
            controller_.click(mb);
            if (opts_.verbose)
                std::cout << "\n[Action] MOUSE_CLICK " << btn << "\n";
            break;
        }

        case Input::ActionType::MOUSE_DOUBLE_CLICK: {
            std::string btn = param(0, "LEFT");
            Input::MouseButton mb = (btn == "RIGHT")  ? Input::MouseButton::Right :
                                    (btn == "MIDDLE") ? Input::MouseButton::Middle :
                                                        Input::MouseButton::Left;
            controller_.doubleClick(mb);
            if (opts_.verbose)
                std::cout << "\n[Action] MOUSE_DOUBLE_CLICK " << btn << "\n";
            break;
        }

        case Input::ActionType::MOUSE_DOWN: {
            std::string btn = param(0, "LEFT");
            Input::MouseButton mb = (btn == "RIGHT")  ? Input::MouseButton::Right :
                                    (btn == "MIDDLE") ? Input::MouseButton::Middle :
                                                        Input::MouseButton::Left;
            controller_.mouseDown(mb);
            break;
        }

        case Input::ActionType::MOUSE_UP: {
            std::string btn = param(0, "LEFT");
            Input::MouseButton mb = (btn == "RIGHT")  ? Input::MouseButton::Right :
                                    (btn == "MIDDLE") ? Input::MouseButton::Middle :
                                                        Input::MouseButton::Left;
            controller_.mouseUp(mb);
            break;
        }

        case Input::ActionType::SCROLL: {
            std::string dir = param(0, "UP");
            int amount = paramInt(1, 3);
            int dy = (dir == "DOWN") ? -amount : amount;
            int dx = (dir == "LEFT") ? -amount : (dir == "RIGHT") ? amount : 0;
            controller_.scroll(dy, dx);
            if (opts_.verbose)
                std::cout << "\n[Action] SCROLL " << dir << " " << amount << "\n";
            break;
        }

        case Input::ActionType::KEY_PRESS: {
            std::string key = param(0, "SPACE");
            controller_.pressKey(key);
            if (opts_.verbose)
                std::cout << "\n[Action] KEY_PRESS " << key << "\n";
            break;
        }

        case Input::ActionType::KEY_COMBO: {
            // Tous sauf le dernier = modificateurs (maintenus)
            // Dernier = touche principale
            if (action.params.empty()) break;
            int mods = static_cast<int>(action.params.size()) - 1;
            for (int i = 0; i < mods; ++i) controller_.keyDown(action.params[i]);
            controller_.pressKey(action.params.back());
            for (int i = mods - 1; i >= 0; --i) controller_.keyUp(action.params[i]);
            if (opts_.verbose) {
                std::cout << "\n[Action] KEY_COMBO";
                for (const auto& p : action.params) std::cout << " " << p;
                std::cout << "\n";
            }
            break;
        }

        case Input::ActionType::KEY_DOWN: {
            controller_.keyDown(param(0, "SPACE"));
            break;
        }

        case Input::ActionType::KEY_UP: {
            controller_.keyUp(param(0, "SPACE"));
            break;
        }

        default: break;
    }

    (void)normPos; (void)frameW; (void)frameH;
}

// --------------------------------------------------------------------------
// Debug UI
// --------------------------------------------------------------------------

void AuraRunner::showDebug(cv::Mat& frame, const cv::Mat& mask,
                            const Vision::DetectionResult& result,
                            const Core::GestureEvent& /*event*/) {
    // Infos de debug discrètes (aire, solidité)
    if (tracker_.isReady() && result.found) {
        float sol = (result.hullArea > 0.f)
                   ? result.area / result.hullArea * 100.f : 0.f;
        std::string info = "aire:" + std::to_string(static_cast<int>(result.area))
                         + " sol:" + std::to_string(static_cast<int>(sol)) + "%"
                         + " doigts:" + std::to_string(result.fingerCount);
        cv::putText(frame, info,
                    {10, frame.rows - 32}, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(180, 180, 0), 1, cv::LINE_AA);
    }

    cv::putText(frame, "'s'=save calib  'q'=quitter",
                {frame.cols - 240, frame.rows - 12},
                cv::FONT_HERSHEY_SIMPLEX, 0.45,
                cv::Scalar(120, 120, 120), 1, cv::LINE_AA);

    cv::imshow("AURA Feed", frame);
    cv::imshow("AURA Mask", mask);
}

} // namespace Aura::App
