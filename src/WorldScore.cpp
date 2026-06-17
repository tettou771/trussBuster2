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
#include <cstdlib>
#endif

WorldScore& worldScore() { static WorldScore w; return w; }

#ifdef __EMSCRIPTEN__

// fetch/submit stash the whole response object into globalThis.__tbWS; poll()
// copies it back. POSTs carry token = sha256(salt:score:initials) so the Worker
// can reject casual fakes.
EM_JS(void, js_wsFetch, (), {
    fetch("https://trussbuster-score.tettou771.workers.dev/")
        .then(function(r) { return r.json(); })
        .then(function(d) { globalThis.__tbWS = d; })
        .catch(function(e) {});
});

EM_JS(void, js_wsPost, (int s, const char* iniC, const char* saltC), {
    var ini = UTF8ToString(iniC), salt = UTF8ToString(saltC);
    crypto.subtle.digest("SHA-256", new TextEncoder().encode(salt + ":" + s + ":" + ini))
      .then(function(buf) {
        var token = Array.from(new Uint8Array(buf)).map(function(b) {
            return b.toString(16).padStart(2, "0"); }).join("");
        return fetch("https://trussbuster-score.tettou771.workers.dev/", {
            method: "POST", headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ score: s, initials: ini, token: token }) });
      }).then(function(r) { return r.json(); })
        .then(function(d) { if (d && d.board) globalThis.__tbWS = d; })
        .catch(function(e) {});
});

// mobile: prompt() (a tap gesture is required so iOS shows the keyboard), then
// clean + sign + POST.
EM_JS(void, js_wsPrompt, (int s, const char* saltC), {
    var raw = prompt("NEW WORLD RECORD!\nEnter your initials (3):", "");
    var ini = (raw || "").toUpperCase().replace(/[^A-Z0-9]/g, "").slice(0, 3);
    if (ini.length === 0) ini = "---";
    var salt = UTF8ToString(saltC);
    crypto.subtle.digest("SHA-256", new TextEncoder().encode(salt + ":" + s + ":" + ini))
      .then(function(buf) {
        var token = Array.from(new Uint8Array(buf)).map(function(b) {
            return b.toString(16).padStart(2, "0"); }).join("");
        return fetch("https://trussbuster-score.tettou771.workers.dev/", {
            method: "POST", headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ score: s, initials: ini, token: token }) });
      }).then(function(r) { return r.json(); })
        .then(function(d) { if (d && d.board) globalThis.__tbWS = d; })
        .catch(function(e) {});
});

// Share the run: native share sheet on mobile, X (Twitter) post intent on
// desktop. Called from a click/tap so the browser permits it.
EM_JS(void, js_share, (int score, int wave), {
    var url = "https://tettou771.github.io/demo-trussBuster2/";
    var text = "I reached Wave " + wave + " with a score of " + score +
               " in TRUSS BUSTER ENDLESS! Can you beat it?";
    if (navigator.share) {
        navigator.share({ title: "TRUSS BUSTER ENDLESS", text: text, url: url }).catch(function(e) {});
    } else {
        var x = "https://twitter.com/intent/tweet?text=" + encodeURIComponent(text) +
                "&url=" + encodeURIComponent(url);
        window.open(x, "_blank");
    }
});

EM_JS(int, js_wsScore, (), {
    return (globalThis.__tbWS !== undefined ? globalThis.__tbWS.score : -1) | 0;
});
EM_JS(void, js_wsInitials, (char* buf, int cap), {
    stringToUTF8((globalThis.__tbWS !== undefined ? globalThis.__tbWS.initials : "---") || "---", buf, cap);
});
// board serialised as "score,INI|score,INI|..."
EM_JS(void, js_wsBoard, (char* buf, int cap), {
    var b = (globalThis.__tbWS && globalThis.__tbWS.board) ? globalThis.__tbWS.board : [];
    var s = b.map(function(e) { return (e.score | 0) + "," + (e.initials || "---"); }).join("|");
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
void webShare(int score, int wave) { js_share(score, wave); }

void WorldScore::poll() {
    int s = js_wsScore();
    if (s < 0) return;                 // nothing fetched yet
    score = s;
    char b[8] = {0};
    js_wsInitials(b, 8);
    initials = b;
    // parse the board string
    char buf[512] = {0};
    js_wsBoard(buf, sizeof(buf));
    board.clear();
    std::string str(buf), entry;
    auto flush = [&]() {
        if (entry.empty()) return;
        size_t comma = entry.find(',');
        if (comma != std::string::npos) {
            WorldEntry e;
            e.score = atoi(entry.substr(0, comma).c_str());
            e.initials = entry.substr(comma + 1);
            board.push_back(e);
        }
        entry.clear();
    };
    for (char c : str) { if (c == '|') flush(); else entry += c; }
    flush();
    loaded = true;
}

#else   // native: no global leaderboard

void WorldScore::fetch() {}
void WorldScore::submit(int, const std::string&) {}
void WorldScore::promptSubmit(int) {}
void WorldScore::poll() {}
void webShare(int, int) {}

#endif
