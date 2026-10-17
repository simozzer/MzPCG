// MzPCG - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

namespace pcg
{

namespace Mode { enum { Gate, Duck }; }
namespace Rate { enum { Eighth, Sixteenth, ThirtySecond }; }

// the steps to a beat at each RATE
inline int stepsPerQuarter(int rate) { return rate == Rate::Eighth ? 2 : rate == Rate::ThirtySecond ? 8 : 4; }

//
// The front panel, in panel units: 0 to 1 unless it says otherwise.
//
struct Settings
{
    int   pattern = 0;            // 0 to 31, MzPCF's patterns
    int   mode    = Mode::Gate;   // GATE: a step opens the sound; DUCK: a step pushes it down
    int   rate    = Rate::Sixteenth;
    float depth   = 1.0f;         // how far: 0 leaves the sound alone, 1 gates it to silence
    float attack  = 0.2f;         // how fast a step takes hold, 0.5 to 50 ms
    float hold    = 0.5f;         // how much of the step it holds there, none to all of it
    float decay   = 0.2f;         // then how fast it lets go, 5 ms to 1.5 s

    float mix     = 1.0f;         // 0 dry to 1 all through the gain
    float level   = 0.0f;         // output, dB, -24 to +12
};

} // namespace pcg
