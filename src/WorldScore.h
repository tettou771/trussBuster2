#pragma once

#include <string>
#include <vector>

struct WorldEntry { int score = 0; std::string initials = "---"; };

// World leaderboard backed by a tiny Cloudflare Worker + KV
// (https://trussbuster-score.tettou771.workers.dev). On web the requests go
// through the browser's fetch; on native it's a no-op (stays empty).
//
// Async + lock-free: fetch()/submit() kick off a request; the result lands in a
// JS global that poll() copies into these fields each frame.
struct WorldScore {
    int                     score = 0;          // #1 score
    std::string             initials = "---";   // #1 initials
    std::vector<WorldEntry> board;              // top N, descending
    bool                    loaded = false;

    void fetch();                                   // GET the board
    void submit(int s, const std::string& initials);// POST (signed)
    void poll();                                    // copy any async result in
    void promptSubmit(int s);                       // mobile web: prompt() then POST
};

WorldScore& worldScore();
