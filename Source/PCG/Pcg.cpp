// MzPCG - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Pcg.h"

#include <algorithm>
#include <cmath>

namespace pcg
{

namespace
{
    // the one-pole coefficient that gets 63% of the way in `seconds`
    float follow(double seconds, double rate) { return 1.0f - (float) std::exp(-1.0 / (seconds * rate)); }
}

void Pcg::prepare(double sampleRate)
{
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    clock.prepare(fs);
    smoothCoef = follow(0.010, fs);
    reset();
}

void Pcg::reset()
{
    clock.reset();
    lastStruck = -1;
    holdLeft = 0.0;
    settled = false;

    // at rest the sound passes: the gate open, nothing ducked
    raw = shaped = target.mode == Mode::Gate ? 1.0f : 0.0f;
}

void Pcg::setHostClock(bool isPlaying, double ppq, double bpm)
{
    clock.set(isPlaying, ppq, bpm, stepsPerQuarter(target.rate));
    if (! clock.isPlaying()) lastStruck = -1;
}

void Pcg::process(float* left, float* right, int n)
{
    const Settings& s = target;
    const bool gate = s.mode == Mode::Gate;

    const float attackCoef = follow(0.0005 * std::pow(100.0, std::clamp(s.attack, 0.0f, 1.0f)), fs);   // 0.5 to 50 ms
    const float decayCoef = (float) std::exp(-1.0 / (0.005 * std::pow(300.0, std::clamp(s.decay, 0.0f, 1.0f)) * fs));   // 5 ms to 1.5 s
    const double holdFor = std::clamp(s.hold, 0.0f, 1.0f) * clock.samplesPerStep();
    const float rest = gate ? 1.0f : 0.0f;

    const float depthT = std::clamp(s.depth, 0.0f, 1.0f);
    const float wetT = std::clamp(s.mix, 0.0f, 1.0f);
    const float outT = std::pow(10.0f, std::clamp(s.level, -24.0f, 12.0f) / 20.0f);
    if (! settled)
    {
        depth = depthT; wet = wetT; out = outT;
        settled = true;
    }

    float* sides[2] = { left, right };
    float g = 1.0f;

    for (int i = 0; i < n; ++i)
    {
        long long st = 0;
        if (clock.stepAt(i, st))
        {
            const int k = (int) (((st % steps) + steps) % steps);
            raw = level(s.pattern, k);
            holdLeft = holdFor;
            lastStruck.store(k, std::memory_order_relaxed);
        }

        if (! clock.isPlaying())  raw = rest;          // stopped: the sound passes
        else if (holdLeft > 0.0)  holdLeft -= 1.0;
        else                      raw *= decayCoef;

        shaped += (raw - shaped) * attackCoef;

        depth += (depthT - depth) * smoothCoef;
        wet   += (wetT - wet) * smoothCoef;
        out   += (outT - out) * smoothCoef;

        // GATE: the envelope is how open the sound is; DUCK: how far down it is pushed
        g = gate ? 1.0f - depth * (1.0f - shaped) : 1.0f - depth * shaped;
        const float k = (1.0f - wet + wet * g) * out;

        for (auto* side : sides)
            if (side != nullptr) side[i] *= k;
    }

    gainNow.store(g, std::memory_order_relaxed);
}

} // namespace pcg
