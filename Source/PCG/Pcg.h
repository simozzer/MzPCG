// MzPCG - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Settings.h"

#include "PCF/Pcf.h"   // MzPCF's patterns and clock, compiled from its repository
#include "PCF/StepClock.h"

#include <atomic>

namespace pcg
{

constexpr int patterns = pcf::patterns;
constexpr int steps = pcf::steps;

// A pattern's level on a step, 0 to 1: the PCF's, so a pattern number means the same in both.
inline float level(int pattern, int step) { return pcf::level(pattern, step); }

//
// The pattern-controlled gain. The PCF's idea, on the volume instead of a filter: on every
// step of the host's timeline - at RATE - an envelope is struck to that step's level in the
// pattern, holds for HOLD of the step, and lets go by DECAY, rounded by ATTACK so it never
// clicks. In GATE mode the envelope opens the sound, as a trance gate; in DUCK mode it
// pushes the sound down, and the sound comes back as it lets go, as a sidechain pump.
// DEPTH says how far either goes.
//
// With the transport stopped nothing is struck, and the sound passes untouched.
//
class Pcg
{
public:
    void prepare(double sampleRate);
    void reset();

    void setSettings(const Settings& s) { target = s; }

    // Where the host's timeline is at the top of the next block.
    void setHostClock(bool playing, double ppq, double bpm);

    // In place. right may be null, for a mono channel.
    void process(float* left, float* right, int n);

    // any thread: the step last struck, 0 to 15, for the panel's lights; -1 stopped
    int step() const { return lastStruck.load(std::memory_order_relaxed); }

    // the gain as it stands, before MIX and LEVEL - for the panel's meter, and the checks
    float gain() const { return gainNow.load(std::memory_order_relaxed); }

private:
    double fs = 48000.0;
    Settings target;
    pcf::StepClock clock;
    std::atomic<int> lastStruck { -1 };
    std::atomic<float> gainNow { 1.0f };

    // the envelope: struck to a level, held, let go; and rounded by the attack
    float raw = 0.0f, shaped = 0.0f;
    double holdLeft = 0.0;

    // the knobs, smoothed, so turning one does not zip
    float depth = 1.0f, wet = 1.0f, out = 1.0f;
    float smoothCoef = 0.0f;
    bool settled = false;
};

} // namespace pcg
