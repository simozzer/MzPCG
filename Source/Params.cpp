// MzPCG - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Params.h"

#include <cmath>

namespace params
{

using S = pcg::Settings;

const std::vector<Spec>& specs()
{
    static const std::vector<Spec> table = {
        { "pattern", "Pattern", "PATTERN", "Which of the 32 patterns plays the gain - the PCF's: on each step the gain is struck to that step's level. Drag up or down, or use the wheel.",
          nullptr, &S::pattern, {}, 1.0f, 32.0f },
        { "mode",    "Mode",    "MODE",    "GATE: each step opens the sound, as a trance gate. DUCK: each step pushes the sound down and it comes back, as a sidechain pump.",
          nullptr, &S::mode, { "GATE", "DUCK" } },
        { "rate",    "Rate",    "RATE",    "How long a step is: an eighth, a sixteenth or a thirty-second note. The pattern's sixteen steps run that fast.",
          nullptr, &S::rate, { "1/8", "1/16", "1/32" } },
        { "depth",   "Depth",   "DEPTH",   "How far the gain goes: at 0 the sound is left alone, at 100 a gate shuts to silence and a duck pushes all the way down.",
          &S::depth },
        { "attack",  "Attack",  "ATK",     "How fast a step takes hold, 0.5 to 50 ms. Never so fast it clicks; slow, it swells.",
          &S::attack, nullptr, {}, 0.0f, 1.0f, Unit::Attack },
        { "hold",    "Hold",    "HOLD",    "How much of the step the gain holds where the step put it, before letting go: none of it, a pluck; all of it, a square gate.",
          &S::hold, nullptr, {}, 0.0f, 1.0f, Unit::Percent },
        { "decay",   "Decay",   "DEC",     "Then how fast it lets go, 5 ms to 1.5 s.",
          &S::decay, nullptr, {}, 0.0f, 1.0f, Unit::Decay },
        { "mix",     "Mix",     "MIX",     "How much of the sound goes through the gain: 0% is dry, 100% all of it.",
          &S::mix, nullptr, {}, 0.0f, 1.0f, Unit::Percent },
        { "level",   "Level",   "LEVEL",   "Output level, -24 to +12 dB, to make up what the gating takes away.",
          &S::level, nullptr, {}, -24.0f, 12.0f, Unit::Decibels },
    };

    return table;
}

const Spec& spec(const juce::String& id)
{
    for (const auto& p : specs())
        if (id == p.id) return p;
    jassertfalse;
    return specs().front();
}

namespace
{
    juce::String milliseconds(double seconds)
    {
        return seconds < 1.0 ? juce::String(seconds * 1000.0, seconds < 0.01 ? 1 : 0) + " ms"
                             : juce::String(seconds, 2) + " s";
    }

    juce::String text(Unit unit, float v)
    {
        switch (unit)
        {
            case Unit::Decibels: return (v > 0.0f ? "+" : "") + juce::String(v, 1) + " dB";
            case Unit::Percent:  return juce::String(juce::roundToInt(v * 100.0f)) + "%";
            case Unit::Attack:   return milliseconds(0.0005 * std::pow(100.0, v));
            case Unit::Decay:    return milliseconds(0.005 * std::pow(300.0, v));
            default:             return juce::String(juce::roundToInt(v * 100.0f));
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const S defaults;

    for (const auto& p : specs())
    {
        const juce::ParameterID id { p.id, 1 };

        if (p.f != nullptr)
        {
            const Unit unit = p.unit;
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                id, p.name, juce::NormalisableRange<float>(p.min, p.max, 0.0f), defaults.*(p.f),
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [unit] (float v, int) { return text(unit, v); })));
        }
        else if (! p.choices.isEmpty())
        {
            layout.add(std::make_unique<juce::AudioParameterChoice>(id, p.name, p.choices, defaults.*(p.i)));
        }
        else
        {
            // a number, shown from min - the pattern, 1 to 32, held as 0 to 31
            layout.add(std::make_unique<juce::AudioParameterInt>(
                id, p.name, (int) p.min, (int) p.max, defaults.*(p.i) + (int) p.min));
        }
    }

    return layout;
}

Reader::Reader(juce::AudioProcessorValueTreeState& state)
{
    for (const auto& p : specs())
    {
        auto* v = state.getRawParameterValue(p.id);
        jassert(v != nullptr);
        values.push_back(v);
    }
}

pcg::Settings Reader::read() const
{
    S s;
    const auto& table = specs();

    for (size_t n = 0; n < table.size(); ++n)
    {
        const float v = values[n]->load(std::memory_order_relaxed);
        const auto& p = table[n];

        if (p.f != nullptr)                s.*(p.f) = v;
        else if (! p.choices.isEmpty())    s.*(p.i) = juce::roundToInt(v);
        else                               s.*(p.i) = juce::roundToInt(v) - (int) p.min;
    }

    return s;
}

} // namespace params
