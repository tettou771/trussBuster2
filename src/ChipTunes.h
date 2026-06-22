#pragma once

#include <TrussC.h>

using namespace std;
using namespace tc;

// MIDI note number -> frequency in Hz
inline float mtof(int midi) {
    return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

// All game audio, generated procedurally with ChipSound (no asset files).
// Access through jukebox() — sounds are built once in setup().
struct Jukebox {
    Sound fire;          // cannon shot
    Sound maxFire;       // perfectly-timed full-power shot
    Sound impact;        // ball hits something
    Sound bust;          // block knocked off the platform
    Sound goldBust;      // gold block busted
    Sound charge;        // gold charged the cannon (+shots)
    Sound explosion;     // a stuck-ball mine detonates
    Sound dryFire;       // tried to fire with no shots left
    Sound peakTick;      // gauge hits the top of its swing
    Sound snap;          // a breakable joint gives way
    Sound startJingle;   // game start
    Sound clearJingle;   // level clear
    Sound overJingle;    // game over
    Sound fanfare;       // all clear
    Sound bgm;           // looping background track

    void setup() {
        buildSfx();
        buildJingles();
        buildBgm();
    }

    // Throttled impact sound, intensity from approach speed
    void playImpact(float speed) {
        if (speed < 2.0f) return;
        float now = getElapsedTimef();
        if (now - lastImpact_ < 0.07f) return;
        lastImpact_ = now;
        impact.setVolume(clamp(speed / 18.0f, 0.15f, 0.8f));
        impact.play();
    }

private:
    float lastImpact_ = 0.0f;

    static ChipSoundNote note(Wave w, float hz, float dur, float vol,
                              float a = 0.005f, float d = 0.03f, float s = 0.6f, float r = 0.04f) {
        ChipSoundNote n(w, hz, dur, vol);
        n.attack = a; n.decay = d; n.sustain = s; n.release = r;
        return n;
    }

    void buildSfx() {
        {   // fire: descending square zap over a noise burst
            ChipSoundBundle b;
            b.add(note(Wave::Noise, 0, 0.13f, 0.45f, 0.002f, 0.04f, 0.4f, 0.06f), 0.0f);
            b.add(note(Wave::Square, 660, 0.045f, 0.30f), 0.0f);
            b.add(note(Wave::Square, 440, 0.045f, 0.28f), 0.045f);
            b.add(note(Wave::Square, 290, 0.06f, 0.24f), 0.09f);
            fire = b.build();
        }
        {   // impact: short noise thunk
            ChipSoundBundle b;
            b.add(note(Wave::Noise, 0, 0.06f, 0.35f, 0.001f, 0.02f, 0.4f, 0.03f), 0.0f);
            b.add(note(Wave::Triangle, 95, 0.06f, 0.35f, 0.001f, 0.02f, 0.5f, 0.03f), 0.0f);
            impact = b.build();
        }
        {   // bust: classic coin (B5 -> E6)
            ChipSoundBundle b;
            b.add(note(Wave::Square, mtof(83), 0.05f, 0.30f), 0.0f);
            b.add(note(Wave::Square, mtof(88), 0.16f, 0.30f, 0.002f, 0.04f, 0.5f, 0.08f), 0.05f);
            bust = b.build();
        }
        {   // gold bust: longer sparkle arpeggio
            ChipSoundBundle b;
            int seq[] = {88, 91, 95, 100};
            for (int i = 0; i < 4; i++)
                b.add(note(Wave::Square, mtof(seq[i]), 0.07f, 0.28f), i * 0.055f);
            b.add(note(Wave::Square, mtof(103), 0.25f, 0.26f, 0.002f, 0.05f, 0.5f, 0.12f), 0.22f);
            goldBust = b.build();
        }
        {   // charge: bright rising power-up sparkle (gold refills shots)
            ChipSoundBundle b;
            int seq[] = {76, 81, 84, 88, 91};
            for (int i = 0; i < 5; i++)
                b.add(note(Wave::Square, mtof(seq[i]), 0.06f, 0.24f), i * 0.045f);
            charge = b.build();
        }
        {   // explosion: deep boom — long noise burst + sub-bass drop
            ChipSoundBundle b;
            b.add(note(Wave::Noise, 0, 0.40f, 0.70f, 0.001f, 0.10f, 0.45f, 0.20f), 0.0f);
            b.add(note(Wave::Triangle, 70, 0.40f, 0.60f, 0.001f, 0.12f, 0.4f, 0.22f), 0.0f);
            b.add(note(Wave::Triangle, 45, 0.45f, 0.55f, 0.001f, 0.14f, 0.4f, 0.25f), 0.04f);
            b.add(note(Wave::Square, 180, 0.10f, 0.30f), 0.0f);
            explosion = b.build();
        }
        {   // dry fire: low buzz
            ChipSoundBundle b;
            b.add(note(Wave::Square, 110, 0.12f, 0.25f, 0.002f, 0.03f, 0.5f, 0.05f), 0.0f);
            dryFire = b.build();
        }
        {   // max-power shot: deep boom + sub bass + harsh sweep + sparkle
            ChipSoundBundle b;
            b.add(note(Wave::Noise, 0, 0.30f, 0.60f, 0.002f, 0.08f, 0.45f, 0.14f), 0.0f);
            b.add(note(Wave::Triangle, 55, 0.32f, 0.55f, 0.002f, 0.10f, 0.5f, 0.15f), 0.0f);
            b.add(note(Wave::Square, 880, 0.05f, 0.34f), 0.0f);
            b.add(note(Wave::Square, 440, 0.05f, 0.32f), 0.05f);
            b.add(note(Wave::Square, 220, 0.07f, 0.30f), 0.10f);
            b.add(note(Wave::Square, 110, 0.12f, 0.26f), 0.16f);
            b.add(note(Wave::Square, mtof(100), 0.09f, 0.18f), 0.05f);   // sparkle on top
            b.add(note(Wave::Square, mtof(103), 0.12f, 0.16f), 0.11f);
            maxFire = b.build();
        }
        {   // joint snap: dry crack — noise click + falling square
            ChipSoundBundle b;
            b.add(note(Wave::Noise, 0, 0.05f, 0.5f, 0.001f, 0.015f, 0.3f, 0.02f), 0.0f);
            b.add(note(Wave::Square, 980, 0.03f, 0.30f), 0.0f);
            b.add(note(Wave::Square, 560, 0.04f, 0.26f), 0.03f);
            b.add(note(Wave::Triangle, 140, 0.07f, 0.30f, 0.001f, 0.02f, 0.5f, 0.04f), 0.02f);
            snap = b.build();
        }
        {   // gauge peak tick: tiny high blip for ear-timing
            ChipSoundBundle b;
            b.add(note(Wave::Square, 1320, 0.025f, 0.13f, 0.001f, 0.01f, 0.4f, 0.01f), 0.0f);
            peakTick = b.build();
        }
    }

    void buildJingles() {
        {   // start: quick ascending A major-ish lift
            ChipSoundBundle b;
            int seq[] = {69, 73, 76, 81};
            for (int i = 0; i < 4; i++)
                b.add(note(Wave::Square, mtof(seq[i]), 0.09f, 0.28f), i * 0.07f);
            startJingle = b.build();
        }
        {   // level clear: C E G C' + chord
            ChipSoundBundle b;
            int seq[] = {72, 76, 79, 84};
            for (int i = 0; i < 4; i++)
                b.add(note(Wave::Square, mtof(seq[i]), 0.10f, 0.26f), i * 0.10f);
            b.add(note(Wave::Square, mtof(84), 0.55f, 0.20f, 0.005f, 0.08f, 0.6f, 0.25f), 0.42f);
            b.add(note(Wave::Square, mtof(88), 0.55f, 0.18f, 0.005f, 0.08f, 0.6f, 0.25f), 0.42f);
            b.add(note(Wave::Triangle, mtof(60), 0.55f, 0.30f, 0.005f, 0.08f, 0.6f, 0.25f), 0.42f);
            clearJingle = b.build();
        }
        {   // game over: slow descending minor
            ChipSoundBundle b;
            int seq[] = {76, 72, 69, 64};
            for (int i = 0; i < 4; i++)
                b.add(note(Wave::Triangle, mtof(seq[i]), 0.26f, 0.34f, 0.01f, 0.06f, 0.6f, 0.12f), i * 0.24f);
            b.add(note(Wave::Triangle, mtof(57), 0.7f, 0.34f, 0.01f, 0.1f, 0.5f, 0.3f), 0.96f);
            b.add(note(Wave::Noise, 0, 0.3f, 0.12f, 0.02f, 0.1f, 0.3f, 0.15f), 0.96f);
            overJingle = b.build();
        }
        {   // all clear: triumphant fanfare
            ChipSoundBundle b;
            int   seq[]  = {72, 72, 72, 76, 79, 79, 76, 79, 84};
            float when[] = {0.0f, 0.12f, 0.24f, 0.36f, 0.60f, 0.72f, 0.84f, 0.96f, 1.20f};
            float dur[]  = {0.10f, 0.10f, 0.10f, 0.20f, 0.10f, 0.10f, 0.10f, 0.20f, 0.8f};
            for (int i = 0; i < 9; i++)
                b.add(note(Wave::Square, mtof(seq[i]), dur[i], 0.26f), when[i]);
            // harmony on the final note
            b.add(note(Wave::Square, mtof(88), 0.8f, 0.18f, 0.005f, 0.1f, 0.6f, 0.3f), 1.20f);
            b.add(note(Wave::Triangle, mtof(60), 0.8f, 0.32f, 0.005f, 0.1f, 0.6f, 0.3f), 1.20f);
            fanfare = b.build();
        }
    }

    void buildBgm() {
        // 8-bar loop in A minor, 132 BPM, 8th-note steps (64 steps)
        const float step = 60.0f / 132.0f / 2.0f;
        ChipSoundBundle b;

        // lead (square), 0 = rest
        int lead[64] = {
            69, 0, 76, 0, 81, 0, 79, 76,   // Am
            74, 0, 77, 0, 81, 0, 79, 76,   // Dm
            72, 0, 76, 0, 79, 0, 81, 79,   // C
            76, 74, 72, 74, 76, 0, 0, 0,   // E
            69, 0, 76, 0, 81, 0, 83, 84,   // Am
            74, 0, 77, 0, 81, 0, 84, 81,   // Dm
            72, 0, 76, 0, 79, 0, 88, 86,   // C
            88, 86, 84, 83, 81, 0, 76, 0,  // E run-down
        };
        for (int i = 0; i < 64; i++) {
            if (lead[i] == 0) continue;
            b.add(note(Wave::Square, mtof(lead[i]), step * 0.92f, 0.15f,
                       0.004f, 0.03f, 0.55f, 0.03f), i * step);
        }

        // bass (triangle): root/octave pump, quarter notes
        int roots[8] = {45, 50, 48, 52, 45, 50, 48, 40};  // A D C E | A D C E,
        for (int bar = 0; bar < 8; bar++) {
            for (int q = 0; q < 4; q++) {
                int midi = roots[bar] + (q % 2) * 12;
                b.add(note(Wave::Triangle, mtof(midi), step * 1.7f, 0.30f,
                           0.005f, 0.04f, 0.7f, 0.05f), (bar * 8 + q * 2) * step);
            }
        }

        // hats (noise): every 8th, accent on beats
        for (int i = 0; i < 64; i++) {
            float vol = (i % 4 == 0) ? 0.06f : 0.03f;
            b.add(note(Wave::Noise, 0, 0.025f, vol, 0.001f, 0.01f, 0.3f, 0.01f), i * step);
        }

        // Pad the buffer out to a full 8 bars (64 steps). build() sizes the buffer
        // to the last note's tail (~63.7 steps here), so without this silent marker
        // the sample-accurate loop restarts a fraction early — the missing trailing
        // rest. This zero-volume note ends exactly on the downbeat (64 * step).
        b.add(note(Wave::Square, 440.0f, step * 0.5f, 0.0f), 64.0f * step - step * 0.5f);

        bgm = b.build();
        bgm.setLoop(true);
        bgm.setVolume(0.5f);
    }
};

inline Jukebox& jukebox() {
    static Jukebox j;
    return j;
}
