// MzPCG - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Look.h"
#include "PluginProcessor.h"

//
// A knob with its legend above it, on a parameter.
//
class Knob final : public juce::Component
{
public:
    Knob(juce::AudioProcessorValueTreeState&, const params::Spec&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::String legend;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//
// A red seven-segment style readout of a whole-number parameter, set by dragging up and
// down or the mouse wheel; double-click for its default.
//
class LedNumber final : public juce::Component, public juce::SettableTooltipClient
{
public:
    LedNumber(juce::RangedAudioParameter&, int digits);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    int value() const;

private:
    void put(int v);

    juce::RangedAudioParameter& param;
    int digits, dragFrom = 0;
    float wheel = 0.0f;
};

//
// A switch, as a pad that steps through its positions, named on it.
//
class Switch final : public juce::Component, public juce::SettableTooltipClient
{
public:
    Switch(juce::RangedAudioParameter&, const juce::String& legend);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& param;
    juce::String legend;
    bool pressed = false;
};

//
// What the gain does over the pattern's sixteen steps: each step's level as a bar, and over
// them the gain the engine makes of them with the knobs as they are - worked out by playing
// the engine itself, whenever a knob moves. The step playing is lit, and a meter at the side
// shows the gain as it is.
//
class GainDisplay final : public juce::Component, public juce::SettableTooltipClient
{
public:
    GainDisplay();

    void show(const pcg::Settings&, int step, float gain);
    void paint(juce::Graphics&) override;

private:
    void simulate();

    pcg::Settings settings;
    bool simulated = false;
    int step = -1;
    float gain = 1.0f;
    std::vector<float> curve;   // the gain across the sixteen steps
};

class MzPcgEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MzPcgEditor(MzPcgProcessor&);
    ~MzPcgEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MzPcgProcessor& processor;
    params::Reader reader;
    Look look;
    juce::TooltipWindow tooltips { this, 600 };

    LedNumber pattern;
    GainDisplay display;
    Switch mode, rate;
    std::vector<std::unique_ptr<Knob>> gainKnobs, outputKnobs;

    juce::Rectangle<int> face, pcgArea, outArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MzPcgEditor)
};
