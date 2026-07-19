#include "Aura/App/AuraRunner.hpp"
#include "Aura/Config/ProfileManager.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstdlib>

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
    if (!tracker_.usesBridge() && !camera_.isOpened()) {
        std::cerr << "[AuraRunner] Caméra inaccessible (device " << opts_.cameraDevice << ")\n";
        return false;
    }
    if (!tracker_.usesBridge()) setupCalibration();
    setupMapping();
    if (opts_.debug && !tracker_.usesBridge()) setupDebugUI();
    if (tracker_.usesBridge())
        std::cout << "[AuraRunner] Mode MediaPipe bridge — jusqu'à 2 mains actives\n";
    return true;
}

void AuraRunner::setupCalibration() {
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
    if (opts_.autoCalib) {
        Vision::Calibrator calib(camera_);
        Vision::HSVRange range;
        if (calib.runAuto(range)) {
            calibConfig_.save("default", range);
            hsvRange_ = range;
        }
        return;
    }
    const std::string& name = opts_.loadCalib.empty() ? "default" : opts_.loadCalib;
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
    calibConfig_.load(name, hsvRange_);
}

void AuraRunner::setupMapping() {
    Config::ProfileManager pm;
    pm.seedFromDir("config/");  // copie tous les profils bundlés si absents

    const std::string name = opts_.profile.empty() ? "default" : opts_.profile;
    loadProfile(pm, name);

    // Auto-switch : initialiser le switcher si demandé
    if (opts_.autoProfile) {
        const char* home = std::getenv("HOME");
        std::filesystem::path cfgPath =
            (home ? std::filesystem::path(home) : std::filesystem::path("."))
            / ".aura" / "auto_profile.txt";
        AutoProfileSwitcher::createTemplateIfMissing(cfgPath);
        autoSwitcher_ = std::make_unique<AutoProfileSwitcher>(cfgPath);
        std::cout << "[AuraRunner] Auto-switch profil activé\n";
    }
}

void AuraRunner::loadProfile(const Config::ProfileManager& pm, const std::string& name) {
    if (!pm.exists(name)) {
        if (name != "default")
            std::cerr << "[AuraRunner] Profil '" << name << "' introuvable — fallback default\n";
        mapper_.loadDefault();
        activeProfile_ = "default";
        return;
    }
    mapper_.load(pm.path(name));
    activeProfile_ = name;
    std::cout << "[AuraRunner] Profil '" << name << "' chargé\n";
}

void AuraRunner::setupDebugUI() {
    cv::namedWindow("AURA Feed", cv::WINDOW_NORMAL);
    cv::namedWindow("AURA Mask", cv::WINDOW_NORMAL);
    cv::namedWindow("AURA Trackbars", cv::WINDOW_NORMAL);

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
    std::cout << "Input  : " << (opts_.inputEnabled ? "activé" : "désactivé (--no-input)") << "\n";
    std::cout << "Debug  : " << (opts_.debug ? "activé (--debug)" : "désactivé") << "\n";
    std::cout << "Mains  : jusqu'à 2 simultanées" << (tracker_.usesBridge() ? " [MediaPipe]" : " [CV mono]") << "\n";
    if (!tracker_.usesBridge()) {
        std::cout << "\nINIT : " << Vision::HandTracker::kInitFrames
                  << " frames d'apprentissage du fond — retirez la main du cadre !\n";
    }
    std::cout << "Appuyez sur 'q' pour quitter.\n\n";

    while (!stopRequested_.load(std::memory_order_relaxed)) {
        // Auto-switch profil selon l'app active
        if (autoSwitcher_) {
            std::string newProfile;
            if (autoSwitcher_->pollChanged(newProfile)) {
                Config::ProfileManager pm;
                loadProfile(pm, newProfile);
                std::cout << "[AutoProfile] → '" << newProfile << "'\n";
            }
        }

        cv::Mat frame;
        if (tracker_.usesBridge()) {
            processFrame(frame);
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
// Traitement d'une frame — itère sur toutes les mains détectées
// --------------------------------------------------------------------------

void AuraRunner::processFrame(const cv::Mat& frame) {
    cv::Mat mask;
    auto results = tracker_.process(frame, hsvRange_, mask);

    const int frameW = frame.empty() ? 640 : frame.cols;
    const int frameH = frame.empty() ? 480 : frame.rows;

    // Marquer quelles mains ont été vues cette frame
    std::array<bool, 2> seen = {false, false};

    for (const auto& result : results) {
        int idx = (result.side == Core::HandSide::RIGHT) ? 1 : 0;
        seen[idx] = true;
        processHand(handStates_[idx], result, frameW, frameH);
    }

    // Relâcher les ressources des mains disparues
    for (int i = 0; i < 2; ++i) {
        if (!seen[i]) {
            releaseDrag(handStates_[i]);
            handStates_[i].twoFingerScrollActive = false;
            handStates_[i].twoFingerHoldFrames   = 0;
            handStates_[i].scrollAccumulator     = 0.f;
        }
    }

    if (opts_.debug && !frame.empty()) {
        cv::Mat display = frame.clone();
        renderDebugOverlay(display, results);
        showDebug(display, mask, results);
    }

    if (opts_.verbose && !results.empty()) {
        std::cout << "\r";
        for (const auto& r : results) {
            std::string side = (r.side == Core::HandSide::LEFT)  ? "L"
                             : (r.side == Core::HandSide::RIGHT) ? "R" : "?";
            int idx = (r.side == Core::HandSide::RIGHT) ? 1 : 0;
            Core::GestureType g = handStates_[idx].lastGesture;
            std::cout << "[" << side << ":" << Core::gestureName(g) << " "
                      << "f" << r.fingerCount << "] ";
        }
        std::cout << "     " << std::flush;
    }
}

void AuraRunner::processHand(HandState& hs, const Vision::DetectionResult& result,
                              int frameW, int frameH) {
    if (tracker_.isReady()) hs.guard.update(result);
    const bool canAct = tracker_.isReady() && hs.guard.isActive();

    Core::GestureEvent event;
    if (canAct) {
        event      = hs.detector.classify(result, frameW, frameH);
        event.side = result.side;

        handleMovement(hs, result, frameW, frameH, event.type);
        handleDrag(hs, event.type);
        handleTwoFingerScroll(hs, event.type, result);

        hs.lastGesture = event.type;

        if (event.type != Core::GestureType::NONE &&
            event.type != Core::GestureType::FIST &&
            event.type != Core::GestureType::TWO_FINGERS) {
            queue_.push(event);
            dispatchEvent(hs, event, frameW, frameH);
        }
    } else {
        releaseDrag(hs);
        hs.twoFingerScrollActive = false;
        hs.twoFingerHoldFrames   = 0;
        hs.scrollAccumulator     = 0.f;
        if (hs.guard.isIdle()) {
            hs.detector    = Vision::GestureDetector{};
            hs.lastGesture = Core::GestureType::NONE;
        }
    }
}

// --------------------------------------------------------------------------
// Mouvement curseur
// --------------------------------------------------------------------------

void AuraRunner::handleMovement(HandState& hs, const Vision::DetectionResult& result,
                                 int frameW, int frameH, Core::GestureType gesture) {
    if (!opts_.inputEnabled || !controller_.available()) return;

    // POINT  → navigation principale (index levé = mapping absolu du bout du doigt)
    // FIST   → drag (relatif, curseur suit la main)
    // tout le reste → curseur gelé pendant les gestes d'action
    if (gesture != Core::GestureType::POINT &&
        gesture != Core::GestureType::FIST) {
        // Mémoriser la position pour reprendre proprement si on revient en POINT
        if (result.found) hs.lastHandPos = result.smoothedPoint;
        return;
    }

    // ── POINT avec MediaPipe : mapping absolu du bout de l'index ─────────────
    if (gesture == Core::GestureType::POINT &&
        tracker_.usesBridge() && result.landmarks.found) {

        const auto& tip = result.landmarks.tip(1);  // index finger TIP
        cv::Point2f rawTip = {tip.x * static_cast<float>(frameW),
                              tip.y * static_cast<float>(frameH)};

        // Kalman fort : filtre les micro-tremblements du bout du doigt
        cv::Point2f kTip = hs.tipSmoother.update(rawTip);

        // EMA additionnelle pour une trajectoire parfaitement fluide
        if (hs.smoothedCursor.x < 0.f) {
            hs.smoothedCursor = kTip;
            virtualCursor_    = kTip;
        }
        hs.smoothedCursor.x = hs.smoothedCursor.x * kTipEma + kTip.x * (1.f - kTipEma);
        hs.smoothedCursor.y = hs.smoothedCursor.y * kTipEma + kTip.y * (1.f - kTipEma);
        virtualCursor_ = hs.smoothedCursor;

        controller_.moveMouse(static_cast<int>(hs.smoothedCursor.x),
                              static_cast<int>(hs.smoothedCursor.y),
                              frameW, frameH);
        hs.lastHandPos = kTip;
        return;
    }

    // ── Fallback relatif : FIST (drag) ou POINT sans MediaPipe ──────────────
    const cv::Point2f& current = opts_.absolute ? result.smoothedPoint
                                                 : result.smoothedPoint;
    if (opts_.absolute) {
        controller_.moveMouse(static_cast<int>(current.x),
                              static_cast<int>(current.y),
                              frameW, frameH);
        hs.lastHandPos    = current;
        hs.smoothedCursor = current;
        virtualCursor_    = current;
        return;
    }

    if (hs.lastHandPos.x < 0.f) {
        hs.lastHandPos    = current;
        virtualCursor_    = current;
        hs.smoothedCursor = current;
        return;
    }

    float dx = current.x - hs.lastHandPos.x;
    float dy = current.y - hs.lastHandPos.y;
    hs.lastHandPos = current;

    if (std::abs(dx) < opts_.deadzone * frameW) dx = 0.f;
    if (std::abs(dy) < opts_.deadzone * frameH) dy = 0.f;
    if (dx == 0.f && dy == 0.f) return;

    virtualCursor_.x = std::clamp(virtualCursor_.x + dx * opts_.speed,
                                   0.f, static_cast<float>(frameW - 1));
    virtualCursor_.y = std::clamp(virtualCursor_.y + dy * opts_.speed,
                                   0.f, static_cast<float>(frameH - 1));

    // EMA légère sur le mode relatif aussi
    if (hs.smoothedCursor.x < 0.f) hs.smoothedCursor = virtualCursor_;
    hs.smoothedCursor.x = hs.smoothedCursor.x * 0.5f + virtualCursor_.x * 0.5f;
    hs.smoothedCursor.y = hs.smoothedCursor.y * 0.5f + virtualCursor_.y * 0.5f;

    controller_.moveMouse(static_cast<int>(hs.smoothedCursor.x),
                          static_cast<int>(hs.smoothedCursor.y),
                          frameW, frameH);
}

// --------------------------------------------------------------------------
// Drag
// --------------------------------------------------------------------------

void AuraRunner::handleDrag(HandState& hs, Core::GestureType gesture) {
    if (!opts_.inputEnabled || !controller_.available()) return;
    if (gesture == Core::GestureType::FIST) {
        if (++hs.fistFrameCount >= kDragActivateFrames && !hs.isDragging) {
            controller_.mouseDown(Input::MouseButton::Left);
            hs.isDragging = true;
        }
    } else {
        releaseDrag(hs);
    }
}

void AuraRunner::releaseDrag(HandState& hs) {
    if (!hs.isDragging) return;
    if (opts_.inputEnabled && controller_.available())
        controller_.mouseUp(Input::MouseButton::Left);
    hs.isDragging     = false;
    hs.fistFrameCount = 0;
}

// --------------------------------------------------------------------------
// Scroll continu (TWO_FINGERS maintenu)
// --------------------------------------------------------------------------

void AuraRunner::handleTwoFingerScroll(HandState& hs, Core::GestureType gesture,
                                        const Vision::DetectionResult& result) {
    if (!opts_.inputEnabled || !controller_.available()) return;

    if (gesture == Core::GestureType::TWO_FINGERS) {
        ++hs.twoFingerHoldFrames;
        if (hs.twoFingerHoldFrames >= kScrollActivateFrames) {
            if (!hs.twoFingerScrollActive) {
                hs.twoFingerScrollActive = true;
                hs.lastScrollPosY        = result.smoothedPoint.y;
                hs.scrollAccumulator     = 0.f;
            } else {
                float delta = hs.lastScrollPosY - result.smoothedPoint.y;
                hs.lastScrollPosY     = result.smoothedPoint.y;
                hs.scrollAccumulator += delta / 10.f;
                int steps = static_cast<int>(hs.scrollAccumulator);
                if (steps != 0) {
                    controller_.scroll(steps);
                    hs.scrollAccumulator -= static_cast<float>(steps);
                }
            }
        }
    } else {
        hs.twoFingerScrollActive = false;
        hs.twoFingerHoldFrames   = 0;
        hs.scrollAccumulator     = 0.f;
    }
}

// --------------------------------------------------------------------------
// Debug overlay
// --------------------------------------------------------------------------

void AuraRunner::renderDebugOverlay(cv::Mat& frame,
                                     const std::vector<Vision::DetectionResult>& results) {
    if (!tracker_.isReady()) {
        cv::putText(frame, "INIT... retirez votre main du cadre",
                    {10, 40}, cv::FONT_HERSHEY_SIMPLEX, 0.75,
                    cv::Scalar(0, 165, 255), 2, cv::LINE_AA);
        return;
    }

    // Skeleton de toutes les mains détectées
    tracker_.drawSkeleton(frame);
    tracker_.drawDebug(frame);

    // État per-main : DRAG / SCROLL / geste actif
    int yOffset = 65;
    for (int i = 0; i < 2; ++i) {
        const HandState& hs = handStates_[i];
        if (!hs.guard.isActive()) continue;

        std::string prefix = (i == 0) ? "[L] " : "[R] ";
        cv::Scalar  color  = (i == 0) ? cv::Scalar(0, 210, 255) : cv::Scalar(255, 80, 50);

        std::string msg;
        if (hs.isDragging)              msg = prefix + "DRAG";
        else if (hs.twoFingerScrollActive) msg = prefix + "SCROLL";
        else if (hs.lastGesture != Core::GestureType::NONE)
            msg = prefix + Core::gestureName(hs.lastGesture);

        if (!msg.empty()) {
            cv::putText(frame, msg, {10, yOffset}, cv::FONT_HERSHEY_SIMPLEX,
                        0.8, color, 2, cv::LINE_AA);
            yOffset += 28;
        }
    }

    // Overlay d'activation : on choisit la main la plus avancée
    ActivationGuard* bestGuard = nullptr;
    for (int i = 0; i < 2; ++i) {
        if (!bestGuard || handStates_[i].guard.isActive())
            bestGuard = &handStates_[i].guard;
    }
    if (bestGuard) bestGuard->drawOverlay(frame);
}

// --------------------------------------------------------------------------
// Dispatch geste → action
// --------------------------------------------------------------------------

void AuraRunner::dispatchEvent(HandState& hs, const Core::GestureEvent& event,
                                int frameW, int frameH) {
    if (!opts_.inputEnabled || !controller_.available()) return;
    if (event.type == Core::GestureType::NONE) return;

    auto now = std::chrono::steady_clock::now();
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now - hs.lastActionTime).count();

    bool isSwipe = (event.type == Core::GestureType::SWIPE_LEFT  ||
                    event.type == Core::GestureType::SWIPE_RIGHT ||
                    event.type == Core::GestureType::SWIPE_UP    ||
                    event.type == Core::GestureType::SWIPE_DOWN);

    if (event.type == hs.lastGesture && !isSwipe && ms < kActionCooldownMs) return;

    hs.lastActionTime = now;

    Input::Action action = mapper_.actionFor(event.type, event.side);
    if (action.type != Input::ActionType::NONE)
        executeAction(action, event.position, frameW, frameH);
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
            break;

        case Input::ActionType::MOUSE_CLICK: {
            std::string btn = param(0, "LEFT");
            Input::MouseButton mb = (btn == "RIGHT")  ? Input::MouseButton::Right :
                                    (btn == "MIDDLE") ? Input::MouseButton::Middle :
                                                        Input::MouseButton::Left;
            controller_.click(mb);
            if (opts_.verbose) std::cout << "\n[Action] MOUSE_CLICK " << btn << "\n";
            break;
        }

        case Input::ActionType::MOUSE_DOUBLE_CLICK: {
            std::string btn = param(0, "LEFT");
            Input::MouseButton mb = (btn == "RIGHT")  ? Input::MouseButton::Right :
                                    (btn == "MIDDLE") ? Input::MouseButton::Middle :
                                                        Input::MouseButton::Left;
            controller_.doubleClick(mb);
            if (opts_.verbose) std::cout << "\n[Action] MOUSE_DOUBLE_CLICK " << btn << "\n";
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
            if (opts_.verbose) std::cout << "\n[Action] SCROLL " << dir << " " << amount << "\n";
            break;
        }

        case Input::ActionType::KEY_PRESS: {
            std::string key = param(0, "SPACE");
            controller_.pressKey(key);
            if (opts_.verbose) std::cout << "\n[Action] KEY_PRESS " << key << "\n";
            break;
        }

        case Input::ActionType::KEY_COMBO: {
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

        case Input::ActionType::KEY_DOWN:
            controller_.keyDown(param(0, "SPACE"));
            break;

        case Input::ActionType::KEY_UP:
            controller_.keyUp(param(0, "SPACE"));
            break;

        default: break;
    }

    (void)normPos; (void)frameW; (void)frameH;
}

// --------------------------------------------------------------------------
// Debug UI
// --------------------------------------------------------------------------

void AuraRunner::showDebug(cv::Mat& frame, const cv::Mat& mask,
                            const std::vector<Vision::DetectionResult>& results) {
    int y = frame.rows - 52;
    for (const auto& r : results) {
        if (!r.found) continue;
        float sol = (r.hullArea > 0.f) ? r.area / r.hullArea * 100.f : 0.f;
        std::string side = (r.side == Core::HandSide::LEFT)  ? "L"
                         : (r.side == Core::HandSide::RIGHT) ? "R" : "?";
        std::string info = "[" + side + "] aire:" + std::to_string(static_cast<int>(r.area))
                         + " sol:" + std::to_string(static_cast<int>(sol)) + "%"
                         + " f:" + std::to_string(r.fingerCount);
        cv::putText(frame, info, {10, y}, cv::FONT_HERSHEY_SIMPLEX, 0.45,
                    cv::Scalar(180, 180, 0), 1, cv::LINE_AA);
        y += 18;
    }

    cv::putText(frame, "'s'=save calib  'q'=quitter",
                {frame.cols - 240, frame.rows - 12},
                cv::FONT_HERSHEY_SIMPLEX, 0.45,
                cv::Scalar(120, 120, 120), 1, cv::LINE_AA);

    cv::imshow("AURA Feed", frame);
    cv::imshow("AURA Mask", mask);
}

} // namespace Aura::App
