#pragma once
#include "Aura/Vision/Camera.hpp"
#include "Aura/Vision/Types.hpp"
#include <string>

namespace Aura::Vision {

// Wizard de calibration HSV couleur peau.
// Séparé de Camera pour respecter le principe de responsabilité unique.
class Calibrator {
public:
    explicit Calibrator(Camera& camera);

    // Wizard guidé 3 étapes (poses) — retourne true si succès
    bool runGuidedWizard(HSVRange& outRange,
                         int framesPerStep = 30,
                         int delayMs = 30);

    // Calibration automatique (YCrCb + fallback mouvement)
    bool runAuto(HSVRange& outRange,
                 int frames = 40,
                 int delayMs = 30);

private:
    Camera& camera_;

    struct SkinSample { double H, S, V; };

    // Détecte la peau dans une frame via YCrCb, retourne le nombre de pixels trouvés
    static int detectSkin(const cv::Mat& frame, cv::Mat& mask);

    // Construit HSVRange à partir d'un ensemble de samples
    static HSVRange buildRange(const std::vector<SkinSample>& samples,
                               int hPad = 12, int sPad = 50, int vPad = 50);

    // Collecte un sample HSV sur la plus grande région de skinMask
    static bool collectSample(const cv::Mat& frame, const cv::Mat& skinMask,
                               SkinSample& out);
};

} // namespace Aura::Vision
