#pragma once

#include <string>

// Global high score backed by a tiny Cloudflare Worker + KV
// (https://trussbuster-score.tettou771.workers.dev). On web the requests go
// through the browser's fetch; on native it's a no-op (stays "---").
//
// Async + lock-free: fetch()/submit() kick off a request; the result lands in a
// JS global that poll() copies into these fields each frame.
struct WorldScore {
    int         score = 0;
    std::string initials = "---";
    bool        loaded = false;

    void fetch();                                   // GET the world record
    void submit(int s, const std::string& initials);// POST (server keeps the max)
    void poll();                                    // copy any async result in
};

WorldScore& worldScore();
