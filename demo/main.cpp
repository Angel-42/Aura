#include "DemoApp.hpp"
#include "Aura/App/AuraRunner.hpp"
#include <iostream>
#include <thread>

static void printHelp(const char* prog) {
    std::cout
        << "Usage: " << prog << " [options]\n\n"
        << "Options:\n"
        << "  --help            Affiche cette aide\n"
        << "  --no-input        Désactive la simulation souris (overlay seulement)\n"
        << "  --profile <nom>   Profil de mapping (~/.aura/profiles/<nom>.txt)\n"
        << "  --camera <id>     Index caméra (défaut: 0)\n";
}

int main(int argc, char* argv[]) {
    Aura::App::RunnerOptions opts;
    opts.inputEnabled = true;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { printHelp(argv[0]); return 0; }
        if (a == "--no-input")   { opts.inputEnabled = false; continue; }
        if (a == "--profile"  && i + 1 < argc) { opts.profile      = argv[++i]; continue; }
        if (a == "--camera"   && i + 1 < argc) { opts.cameraDevice = std::stoi(argv[++i]); continue; }
        std::cerr << "[demo] Option inconnue : " << a << "\n";
    }

    Aura::App::AuraRunner runner(opts);

    // AuraRunner tourne dans un thread dédié — la fenêtre SFML doit être sur le thread principal
    std::thread runnerThread([&runner] { runner.run(); });

    {
        Aura::Demo::DemoApp demo(runner);
        demo.run();
    }

    runner.stop();
    runnerThread.join();
    return 0;
}
