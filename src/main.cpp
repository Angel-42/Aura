#include "Aura/App/AuraRunner.hpp"
#include "Aura/Config/ProfileManager.hpp"
#include <iostream>
#include <string>
#include <vector>

static void printHelp(const std::string& prog) {
    std::cout
        << "Usage: " << prog << " [options]\n\n"
        << "Options:\n"
        << "  --help              Affiche cette aide\n"
        << "  --no-input          Désactive la simulation clavier/souris\n"
        << "  --verbose           Affiche le geste détecté en temps réel\n"
        << "  --debug             Fenêtres OpenCV + trackbars HSV\n"
        << "  --camera <id>       Index de la caméra (défaut: 0)\n"
        << "  --load-calib <nom>  Charge le preset de calibration\n"
        << "  --save-calib <nom>  Lance le wizard et sauvegarde sous <nom>\n"
        << "  --auto-calib        Calibration automatique\n\n"
        << "Profils de mapping :\n"
        << "  --profile <nom>     Charge ~/.aura/profiles/<nom>.txt (défaut: default)\n"
        << "  --list-profiles     Liste les profils disponibles\n\n"
        << "Sensibilité curseur :\n"
        << "  --speed <x>         Multiplicateur de vitesse (défaut: 1.5, ex: 2.0)\n"
        << "  --deadzone <x>      Zone morte en fraction de frame (défaut: 0.02)\n"
        << "  --absolute          Mode absolu : main mappe directement l'écran\n\n"
        << "Raccourcis en mode --debug :\n"
        << "  s  Sauvegarder la calibration courante\n"
        << "  q  Quitter\n";
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);

    // --list-profiles ne nécessite pas de lancer le runner
    for (const auto& a : args) {
        if (a == "--list-profiles") {
            Aura::Config::ProfileManager pm;
            pm.initDefault("config/default_mapping.txt");
            auto profiles = pm.list();
            if (profiles.empty()) {
                std::cout << "Aucun profil dans " << pm.profilesDir() << "\n";
            } else {
                std::cout << "Profils disponibles (" << pm.profilesDir() << ") :\n";
                for (const auto& p : profiles)
                    std::cout << "  " << p << "\n";
            }
            return 0;
        }
    }

    Aura::App::RunnerOptions opts;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& a = args[i];
        if (a == "--help" || a == "-h") { printHelp(argv[0]); return 0; }
        if (a == "--no-input")    { opts.inputEnabled = false; continue; }
        if (a == "--verbose")     { opts.verbose      = true;  continue; }
        if (a == "--debug")       { opts.debug        = true;  continue; }
        if (a == "--auto-calib")  { opts.autoCalib    = true;  continue; }
        if (a == "--absolute")    { opts.absolute     = true;  continue; }
        if (a == "--camera"    && i + 1 < args.size()) { opts.cameraDevice = std::stoi(args[++i]); continue; }
        if (a == "--speed"     && i + 1 < args.size()) { opts.speed        = std::stof(args[++i]); continue; }
        if (a == "--deadzone"  && i + 1 < args.size()) { opts.deadzone     = std::stof(args[++i]); continue; }
        if (a == "--load-calib"&& i + 1 < args.size()) { opts.loadCalib    = args[++i]; continue; }
        if (a == "--save-calib"&& i + 1 < args.size()) { opts.saveCalib    = args[++i]; continue; }
        if (a == "--profile"   && i + 1 < args.size()) { opts.profile      = args[++i]; continue; }
        std::cerr << "[main] Option inconnue : " << a << "\n";
    }

    Aura::App::AuraRunner runner(std::move(opts));
    runner.run();
    return 0;
}
