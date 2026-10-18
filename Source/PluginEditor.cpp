// MzPCG - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "PluginEditor.h"

namespace
{
    juce::RangedAudioParameter& parameter(MzPcgProcessor& p, const char* id)
    {
        auto* param = p.state.getParameter(id);
        jassert(param != nullptr);
        return *param;
    }

    void drawLegend(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r)
    {
        g.setColour(colours::light);
        g.setFont(fonts::bold(9.5f));
        g.drawText(text, r, juce::Justification::centred);
    }
}

//============================================================================== Knob

Knob::Knob(juce::AudioProcessorValueTreeState& state, const params::Spec& spec)
    : legend(spec.legend)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity(180);
    slider.setPopupDisplayEnabled(true, true, nullptr);
    slider.setTooltip(juce::String(spec.name).toUpperCase() + "\n" + spec.hint);
    slider.setColour(juce::Slider::rotarySliderFillColourId, colours::knobCap);
    slider.setColour(juce::Slider::thumbColourId, colours::pointer);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, colours::light);
    addAndMakeVisible(slider);

    // the attachment sets the range, and reads the value out as the host would
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, spec.id, slider);

    if (auto* p = state.getParameter(spec.id))
        slider.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop(12);
    const int side = juce::jmin(r.getWidth(), r.getHeight());
    slider.setBounds(r.withSizeKeepingCentre(side, side).withY(r.getY()));
}

void Knob::paint(juce::Graphics& g)
{
    drawLegend(g, legend, getLocalBounds().removeFromTop(12));
}

//============================================================================== LedNumber

LedNumber::LedNumber(juce::RangedAudioParameter& p, int d) : param(p), digits(d)
{
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
}

int LedNumber::value() const
{
    return juce::roundToInt(param.convertFrom0to1(param.getValue()));
}

void LedNumber::put(int v)
{
    const auto range = param.getNormalisableRange();
    v = juce::jlimit((int) range.start, (int) range.end, v);
    if (v != value()) param.setValueNotifyingHost(param.convertTo0to1((float) v));
    repaint();
}

void LedNumber::mouseDown(const juce::MouseEvent&)
{
    dragFrom = value();
    param.beginChangeGesture();
}

void LedNumber::mouseDrag(const juce::MouseEvent& e) { put(dragFrom - e.getDistanceFromDragStartY() / 4); }
void LedNumber::mouseUp(const juce::MouseEvent&)     { param.endChangeGesture(); }

void LedNumber::mouseDoubleClick(const juce::MouseEvent&)
{
    param.beginChangeGesture();
    put(juce::roundToInt(param.convertFrom0to1(param.getDefaultValue())));
    param.endChangeGesture();
}

void LedNumber::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    wheel += w.deltaY * 8.0f;
    const int whole = (int) wheel;
    if (whole == 0) return;
    wheel -= (float) whole;
    param.beginChangeGesture();
    put(value() + whole);
    param.endChangeGesture();
}

void LedNumber::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour(colours::display);
    g.fillRoundedRectangle(r, 2.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(r.reduced(0.5f), 2.0f, 1.0f);

    g.setFont(fonts::digits(r.getHeight() * 0.78f));
    g.setColour(colours::ledOff.withAlpha(0.4f));   // the unlit segments, there but not read
    g.drawText(juce::String::repeatedString("8", digits), r.reduced(4.0f, 0.0f), juce::Justification::centredRight);
    g.setColour(colours::ledRed);
    g.drawText(juce::String(value()), r.reduced(4.0f, 0.0f), juce::Justification::centredRight);
}

//============================================================================== Switch

Switch::Switch(juce::RangedAudioParameter& p, const juce::String& l) : param(p), legend(l) {}

void Switch::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    pressed = true;

    const int positions = param.getNumSteps();
    const int now = juce::roundToInt(param.getValue() * (float) (positions - 1));
    param.beginChangeGesture();
    param.setValueNotifyingHost((float) ((now + 1) % positions) / (float) (positions - 1));
    param.endChangeGesture();
    repaint();
}

void Switch::mouseUp(const juce::MouseEvent&)
{
    pressed = false;
    repaint();
}

void Switch::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    drawLegend(g, legend, area.removeFromTop(12));
    area.removeFromTop(2);

    auto r = area.withSizeKeepingCentre(juce::jmin(area.getWidth(), 46), 26).toFloat().reduced(1.0f);
    if (pressed) r = r.translated(0.0f, 1.0f);

    g.setGradientFill(juce::ColourGradient(colours::pad.brighter(pressed ? 0.0f : 0.15f), r.getX(), r.getY(),
                                           colours::pad.darker(0.3f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawRoundedRectangle(r, 3.0f, 1.0f);

    g.setColour(colours::light);
    g.setFont(fonts::bold(11.0f));
    g.drawText(param.getCurrentValueAsText(), r.toNearestInt(), juce::Justification::centred);
}

//============================================================================== GainDisplay

GainDisplay::GainDisplay()
{
    setTooltip("The pattern's sixteen steps, each as tall as its level, and over them the gain they "
               "make with the knobs as they are (drawn at 120 bpm). The step playing is lit while your "
               "DAW plays; the meter at the right is the gain now.");
}

void GainDisplay::show(const pcg::Settings& s, int st, float g)
{
    const bool changed = ! simulated || s.pattern != settings.pattern || s.mode != settings.mode || s.rate != settings.rate
                      || s.depth != settings.depth || s.attack != settings.attack || s.hold != settings.hold
                      || s.decay != settings.decay;
    settings = s;
    if (changed) simulate();
    if (changed || st != step || std::abs(g - gain) > 0.002f)
    {
        step = st;
        gain = g;
        repaint();
    }
}

void GainDisplay::simulate()
{
    // The engine itself, played twice round the pattern at 120 bpm, so the first step shows
    // what the last left it; the second time round is drawn.
    constexpr double rate = 4000.0, bpm = 120.0;
    pcg::Pcg engine;
    engine.prepare(rate);
    pcg::Settings s = settings;
    s.mix = 1.0f;
    s.level = 0.0f;
    engine.setSettings(s);

    const int perStep = (int) (rate * 60.0 / bpm / pcg::stepsPerQuarter(s.rate));
    const int total = perStep * pcg::steps * 2;
    std::vector<float> x((size_t) total, 1.0f);
    for (int at = 0; at < total; at += 64)
    {
        const int n = juce::jmin(64, total - at);
        engine.setHostClock(true, (double) at / rate * bpm / 60.0, bpm);
        engine.process(x.data() + at, nullptr, n);
    }

    curve.assign(x.begin() + total / 2, x.end());
    simulated = true;
}

void GainDisplay::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour(colours::display);
    g.fillRoundedRectangle(r, 2.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(r.reduced(0.5f), 2.0f, 1.0f);

    auto inner = r.reduced(5.0f);
    const auto meter = inner.removeFromRight(8.0f);
    inner.removeFromRight(5.0f);

    // the steps' levels
    const float w = inner.getWidth() / (float) pcg::steps;
    for (int s = 0; s < pcg::steps; ++s)
    {
        const float level = pcg::level(settings.pattern, s);
        const float h = juce::jmax(1.5f, inner.getHeight() * level);
        const juce::Rectangle<float> bar(inner.getX() + w * (float) s + 1.0f, inner.getBottom() - h, w - 2.0f, h);
        g.setColour(s == step ? colours::ledRed.withAlpha(0.6f) : colours::ledRed.withAlpha(s % 4 == 0 ? 0.28f : 0.18f));
        g.fillRect(bar);
    }

    // the gain they make
    if (! curve.empty())
    {
        juce::Path p;
        const int points = juce::jmax(2, (int) inner.getWidth());
        for (int i = 0; i < points; ++i)
        {
            const size_t k = (size_t) ((double) i / (points - 1) * (double) (curve.size() - 1));
            const float x = inner.getX() + (float) i;
            const float y = inner.getBottom() - inner.getHeight() * juce::jlimit(0.0f, 1.0f, curve[k]);
            if (i == 0) p.startNewSubPath(x, y);
            else        p.lineTo(x, y);
        }
        g.setColour(colours::pointer);
        g.strokePath(p, juce::PathStrokeType(1.8f));
    }

    // the gain now
    g.setColour(juce::Colours::black);
    g.fillRect(meter);
    const float m = meter.getHeight() * juce::jlimit(0.0f, 1.0f, gain);
    g.setColour(colours::ledRed);
    g.fillRect(meter.withTop(meter.getBottom() - m));
}

//============================================================================== the editor

MzPcgEditor::MzPcgEditor(MzPcgProcessor& p)
    : AudioProcessorEditor(p),
      processor(p),
      reader(p.state),
      pattern(parameter(p, "pattern"), 2),
      mode(parameter(p, "mode"), "MODE"),
      rate(parameter(p, "rate"), "RATE")
{
    setLookAndFeel(&look);

    pattern.setTooltip(juce::String("PATTERN\n") + params::spec("pattern").hint);
    mode.setTooltip(juce::String("MODE\n") + params::spec("mode").hint);
    rate.setTooltip(juce::String("RATE\n") + params::spec("rate").hint);
    addAndMakeVisible(pattern);
    addAndMakeVisible(display);

    //
    // Where to come back to: the version this is, and the page the next one will be on.
    // Nothing here checks for updates by itself - the plugin never goes online unasked.
    // It takes no keyboard focus, or a host's space bar could open it rather than play.
    //
    projectLink.setButtonText("MzPCG " JucePlugin_VersionString "  -  github.com/simozzer/MzPCG");
    projectLink.setURL(juce::URL("https://github.com/simozzer/MzPCG"));
    projectLink.setFont(fonts::bold(10.5f), false, juce::Justification::centredRight);
    projectLink.setColour(juce::HyperlinkButton::textColourId, colours::light.withAlpha(0.6f));
    projectLink.setTooltip("The project's page: the latest version and what's new in it, the source, "
                           "and where to report a problem. Opens in your browser.");
    projectLink.setWantsKeyboardFocus(false);
    addAndMakeVisible(projectLink);
    addAndMakeVisible(mode);
    addAndMakeVisible(rate);

    for (const char* id : { "depth", "attack", "hold", "decay" })
        gainKnobs.push_back(std::make_unique<Knob>(p.state, params::spec(id)));
    for (const char* id : { "mix", "level" })
        outputKnobs.push_back(std::make_unique<Knob>(p.state, params::spec(id)));
    for (auto& k : gainKnobs) addAndMakeVisible(*k);
    for (auto& k : outputKnobs) addAndMakeVisible(*k);

    setSize(840, 170);
    timerCallback();   // opened showing the pattern, not after the first tick
    startTimerHz(30);
}

MzPcgEditor::~MzPcgEditor()
{
    setLookAndFeel(nullptr);
}

void MzPcgEditor::timerCallback()
{
    display.show(reader.read(), processor.step(), processor.gain());
    pattern.repaint();
    mode.repaint();
    rate.repaint();
}

void MzPcgEditor::resized()
{
    face = getLocalBounds().reduced(10);
    projectLink.setBounds(face.reduced(14, 8).removeFromTop(28).removeFromRight(300));   // the title's row

    auto r = face.reduced(10, 8);
    r.removeFromTop(30);   // the title

    outArea = r.removeFromRight(130);
    r.removeFromRight(8);
    pcgArea = r;

    // the PCG: the pattern number and the gain it makes, the switches, then the four knobs
    auto row = pcgArea.reduced(8, 6);
    row.removeFromTop(18);   // the unit's name
    const int y = row.getY(), h = row.getHeight();

    pattern.setBounds(row.getX(), y + 16, 44, 32);
    display.setBounds(row.getX() + 52, y + 4, 230, h - 8);
    mode.setBounds(row.getX() + 290, y + 4, 54, 46);
    rate.setBounds(row.getX() + 348, y + 4, 54, 46);

    int x = row.getX() + 410;
    for (auto& k : gainKnobs)
    {
        k->setBounds(x, y, 54, h);
        x += 56;
    }

    auto out = outArea.reduced(8, 6);
    out.removeFromTop(18);
    for (auto& k : outputKnobs)
        k->setBounds(out.removeFromLeft(56).withHeight(out.getHeight()));
}

void MzPcgEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::rail);
    drawFace(g, face.toFloat(), colours::rail.brighter(0.15f));

    // the title
    auto title = face.reduced(14, 8).removeFromTop(28);
    g.setColour(colours::light);
    g.setFont(fonts::bold(22.0f));
    g.drawText("MzPCG", title, juce::Justification::centredLeft);
    g.setColour(colours::light.withAlpha(0.6f));
    g.setFont(fonts::bold(10.5f));
    g.drawText("PATTERN CONTROLLED GAIN", title.withTrimmedLeft(82), juce::Justification::centredLeft);

    // the two units, as the rack draws its effects
    for (auto [area, name] : { std::pair { pcgArea, "PCG" }, std::pair { outArea, "OUTPUT" } })
    {
        drawFace(g, area.toFloat(), colours::mixer);

        auto tag = area.reduced(8, 6).removeFromTop(15).withWidth(60);
        g.setColour(colours::mixerEdge);
        g.fillRoundedRectangle(tag.toFloat(), 2.0f);
        g.setColour(colours::light);
        g.setFont(fonts::bold(10.5f));
        g.drawText(name, tag, juce::Justification::centred);
    }

    g.setColour(colours::light.withAlpha(0.75f));
    g.setFont(fonts::bold(9.0f));
    g.drawText("PATTERN", pattern.getBounds().withY(pattern.getY() - 13).withHeight(12).withWidth(60),
               juce::Justification::centredLeft);
}
