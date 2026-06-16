#pragma once

#include <TrussC.h>
#include <cstdlib>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

using namespace tc;

// True on phones/tablets: native iOS/Android (compile time) or a mobile
// browser when running as wasm (runtime user-agent + touch check; iPad
// reports as Mac so maxTouchPoints is also checked).
inline bool isMobileDevice() {
    if (Platform::isMobile()) return true;
#ifdef __EMSCRIPTEN__
    static int cached = -1;
    if (cached < 0) {
        cached = emscripten_run_script_int(
            "((/Android|iPhone|iPad|iPod/i.test(navigator.userAgent))||"
            "(navigator.platform==='MacIntel'&&navigator.maxTouchPoints>1))?1:0");
    }
    if (cached == 1) return true;
#endif
    // dev override: test the touch UI on desktop
    return std::getenv("TB_FORCE_TOUCH") != nullptr;
}
