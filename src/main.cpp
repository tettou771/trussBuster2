// =============================================================================
// main.cpp - TRUSS BUSTER entry point
// =============================================================================

#include "tcApp.h"
#include <cstdlib>

int main() {
    tc::WindowSettings settings;
    // TB_WIN_W / TB_WIN_H override the size (responsive-layout testing)
    int w = std::getenv("TB_WIN_W") ? atoi(std::getenv("TB_WIN_W")) : 1280;
    int h = std::getenv("TB_WIN_H") ? atoi(std::getenv("TB_WIN_H")) : 800;
    settings.setSize(w, h);
    settings.setTitle("TRUSS BUSTER");

    return TC_RUN_APP(tcApp, settings);
}
