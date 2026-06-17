#include "WorldScore.h"

// Signing salt for score POSTs. Kept out of the public repo (Secret.h is
// gitignored); without it the build still works but the server rejects submits.
#if __has_include("Secret.h")
#include "Secret.h"
#else
#define TB_SCORE_SALT "changeme"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

WorldScore& worldScore() { static WorldScore w; return w; }

#ifdef __EMSCRIPTEN__

// fetch/submit stash the latest {score, initials} into a JS global; poll()
// copies it back. POSTs carry token = sha256(salt:score:initials) so the Worker
// can reject casual fakes.
EM_JS(void, js_wsFetch, (), {
    fetch("https://trussbuster-score.tettou771.workers.dev/")
        .then(function(r) { return r.json(); })
        .then(function(d) { globalThis.__tbWS = { score: (d.score | 0), initials: (d.initials || "---") }; })
        .catch(function(e) {});
});

// shared POST helper: sign then send. `cleaned` initials are already A-Z0-9<=3.
EM_JS(void, js_wsPost, (int s, const char* iniC, const char* saltC), {
    var ini = UTF8ToString(iniC);
    var salt = UTF8ToString(saltC);
    var enc = new TextEncoder().encode(salt + ":" + s + ":" + ini);
    crypto.subtle.digest("SHA-256", enc).then(function(buf) {
        var token = Array.from(new Uint8Array(buf)).map(function(b) {
            return b.toString(16).padStart(2, "0"); }).join("");
        return fetch("https://trussbuster-score.tettou771.workers.dev/", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ score: s, initials: ini, token: token })
        });
    }).then(function(r) { return r.json(); })
      .then(function(d) { if (d && d.score !== undefined) globalThis.__tbWS = { score: (d.score | 0), initials: (d.initials || "---") }; })
      .catch(function(e) {});
});

// mobile: prompt() (a tap gesture is required so iOS shows the keyboard), then
// clean + sign + POST.
EM_JS(void, js_wsPrompt, (int s, const char* saltC), {
    var raw = prompt("NEW WORLD RECORD!\nEnter your initials (3):", "");
    var ini = (raw || "").toUpperCase().replace(/[^A-Z0-9]/g, "").slice(0, 3);
    if (ini.length === 0) ini = "---";
    var salt = UTF8ToString(saltC);
    var enc = new TextEncoder().encode(salt + ":" + s + ":" + ini);
    crypto.subtle.digest("SHA-256", enc).then(function(buf) {
        var token = Array.from(new Uint8Array(buf)).map(function(b) {
            return b.toString(16).padStart(2, "0"); }).join("");
        return fetch("https://trussbuster-score.tettou771.workers.dev/", {
            method: "POST", headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ score: s, initials: ini, token: token })
        });
    }).then(function(r) { return r.json(); })
      .then(function(d) { if (d && d.score !== undefined) globalThis.__tbWS = { score: (d.score | 0), initials: (d.initials || "---") }; })
      .catch(function(e) {});
});

EM_JS(int, js_wsScore, (), {
    return (globalThis.__tbWS !== undefined ? globalThis.__tbWS.score : -1) | 0;
});

EM_JS(void, js_wsInitials, (char* buf, int cap), {
    var s = (globalThis.__tbWS !== undefined ? globalThis.__tbWS.initials : "---");
    stringToUTF8(s, buf, cap);
});

static std::string clean3(const std::string& in) {
    std::string out;
    for (char c : in) {
        if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            out += c;
            if (out.size() == 3) break;
        }
    }
    return out.empty() ? "---" : out;
}

void WorldScore::fetch() { js_wsFetch(); }
void WorldScore::submit(int s, const std::string& ini) {
    js_wsPost(s, clean3(ini).c_str(), TB_SCORE_SALT);
}
void WorldScore::promptSubmit(int s) { js_wsPrompt(s, TB_SCORE_SALT); }
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
void WorldScore::promptSubmit(int) {}
void WorldScore::poll() {}

#endif
