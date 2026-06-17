#include "WorldScore.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

WorldScore& worldScore() { static WorldScore w; return w; }

#ifdef __EMSCRIPTEN__

// fetch/submit stash the latest {score, initials} into a JS global; poll()
// copies it back. Avoids passing strings back through wasm (no malloc dance).
EM_JS(void, js_wsFetch, (), {
    fetch("https://trussbuster-score.tettou771.workers.dev/")
        .then(function(r) { return r.json(); })
        .then(function(d) { globalThis.__tbWS = { score: (d.score | 0), initials: (d.initials || "---") }; })
        .catch(function(e) {});
});

EM_JS(void, js_wsSubmit, (int s, const char* ini), {
    fetch("https://trussbuster-score.tettou771.workers.dev/", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ score: s, initials: UTF8ToString(ini) })
    }).then(function(r) { return r.json(); })
      .then(function(d) { globalThis.__tbWS = { score: (d.score | 0), initials: (d.initials || "---") }; })
      .catch(function(e) {});
});

EM_JS(int, js_wsScore, (), {
    return (globalThis.__tbWS !== undefined ? globalThis.__tbWS.score : -1) | 0;
});

EM_JS(void, js_wsInitials, (char* buf, int cap), {
    var s = (globalThis.__tbWS !== undefined ? globalThis.__tbWS.initials : "---");
    stringToUTF8(s, buf, cap);
});

void WorldScore::fetch() { js_wsFetch(); }
void WorldScore::submit(int s, const std::string& ini) { js_wsSubmit(s, ini.c_str()); }
void WorldScore::poll() {
    int s = js_wsScore();
    if (s >= 0) {
        score = s;
        char b[8] = {0};
        js_wsInitials(b, 8);
        initials = b;
        loaded = true;
    }
}

#else   // native: no global leaderboard

void WorldScore::fetch() {}
void WorldScore::submit(int, const std::string&) {}
void WorldScore::poll() {}

#endif
