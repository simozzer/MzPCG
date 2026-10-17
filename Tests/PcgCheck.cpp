// MzPCG - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

//
// The engine, checked on its own: steps on the host's grid at every RATE, the shape of a
// gate and of a duck, DEPTH, HOLD, that it never clicks, that a stopped transport leaves the
// sound alone, MIX and LEVEL, the block size making no difference, and that no setting at
// any rate blows up. The clock itself is MzPCF's, held to the sample by its own checks.
// Links no JUCE.
//
//     PcgCheck                 run the checks
//     PcgCheck --wav <dir>     and also write a few demo phrases there to listen to
//

#define _CRT_SECURE_NO_WARNINGS   // fopen, for the demo renders

#include "Pcg.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace pcg;

namespace
{

int failures = 0;

void check(bool ok, const char* what, const std::string& detail)
{
    std::printf("  %s  %-48s %s\n", ok ? "ok  " : "FAIL", what, detail.c_str());
    if (! ok) ++failures;
}

std::string fmt(const char* f, double a, double b = 0)
{
    char buf[128];
    std::snprintf(buf, sizeof buf, f, a, b);
    return buf;
}

constexpr double pi = 3.14159265358979323846;

//
// A host: plays from `ppq` at `bpm`, handing the engine its position at the top of each
// block, as the plugin does.
//
struct Host
{
    double rate = 48000.0, bpm = 120.0, ppq = 0.0;
    bool playing = true;
    int block = 512;
    long long played = 0;

    template <class F>
    void run(Pcg& p, float* l, float* r, int n, F&& perSample)
    {
        for (int at = 0; at < n; )
        {
            const int len = block == 1 ? 1 : std::min(block, n - at);
            p.setHostClock(playing, ppq + (double) played * bpm / 60.0 / rate, bpm);
            p.process(l + at, r ? r + at : nullptr, len);
            if (block == 1) perSample(at);
            if (playing) played += len;
            at += len;
        }
    }

    void run(Pcg& p, float* l, float* r, int n) { run(p, l, r, n, [] (int) {}); }
};

std::vector<float> noise(size_t n, unsigned seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(-0.5f, 0.5f);
    std::vector<float> v(n);
    for (auto& x : v) x = u(rng);
    return v;
}

// The gain on every sample, played a sample at a time.
std::vector<float> gains(Host h, const Settings& s, int n, std::vector<int>* struck = nullptr)
{
    Pcg p;
    p.prepare(h.rate);
    p.setSettings(s);
    h.block = 1;
    std::vector<float> x((size_t) n, 1.0f), g((size_t) n);
    int last = -2;
    h.run(p, x.data(), nullptr, n, [&] (int i)
    {
        g[(size_t) i] = p.gain();
        if (struck != nullptr && p.step() != last && p.step() >= 0) struck->push_back(i);
        last = p.step();
    });
    return g;
}

//============================================================================== the checks

void gridCheck()
{
    std::printf("the grid\n");

    // 120 bpm at 48 kHz: a beat is 24000 samples
    Settings s;
    s.pattern = 2;   // every step
    for (int rate : { Rate::Eighth, Rate::Sixteenth, Rate::ThirtySecond })
    {
        s.rate = rate;
        const int every = 24000 / stepsPerQuarter(rate);
        std::vector<int> struck;
        gains(Host {}, s, 24000 * 4 + 1, &struck);
        bool onGrid = ! struck.empty();
        for (size_t k = 0; k < struck.size() && onGrid; ++k) onGrid = struck[k] == (int) k * every;
        const char* names[] = { "1/8: a step every 12000 samples", "1/16: every 6000", "1/32: every 3000" };
        check(onGrid && (int) struck.size() == 24000 * 4 / every + 1, names[rate], fmt("%.0f steps", (double) struck.size()));
    }

    check(level(0, 0) == pcf::level(0, 0) && level(13, 5) == pcf::level(13, 5) && patterns == 32,
          "the PCF's 32 patterns", "");
}

void shapeCheck()
{
    std::printf("the gate and the duck\n");

    // pattern 1, every beat; a step is 6000 samples
    Settings s;
    s.pattern = 0;
    s.attack = 0.0f;   // 0.5 ms
    s.hold = 0.5f;
    s.decay = 0.0f;    // 5 ms

    auto g = gains(Host {}, s, 24000);
    check(g[1500] > 0.98f && g[2900] > 0.98f, "GATE: open on the beat, for HOLD of its step", fmt("%.3f, then %.3f", g[1500], g[2900]));
    check(g[4500] < 0.01f && g[9000] < 0.001f, "and shut after, until the next", fmt("%.4f, then %.5f", g[4500], g[9000]));
    check(g[24000 - 1] < 0.001f && gains(Host {}, s, 24100)[24050] > 0.8f, "which opens it again", "");

    s.mode = Mode::Duck;
    g = gains(Host {}, s, 24000);
    check(g[1500] < 0.02f && g[9000] > 0.999f, "DUCK: pushed down on the beat, and back up after",
          fmt("%.3f, then %.4f", g[1500], g[9000]));

    s.mode = Mode::Gate;
    s.depth = 0.5f;
    g = gains(Host {}, s, 24000);
    check(std::fabs(g[9000] - 0.5f) < 0.01f, "DEPTH at half shuts it half way", fmt("%.3f", g[9000]));

    s.depth = 1.0f;
    s.hold = 1.0f;
    s.pattern = 2;   // every step
    g = gains(Host {}, s, 24000);
    float least = 1.0f;
    for (size_t i = 600; i < g.size(); ++i) least = std::min(least, g[i]);
    check(least > 0.98f, "HOLD all of a step, every step: open throughout", fmt("never below %.3f", least));

    // a step at level 8 of 15: open that far
    s.pattern = 5;   // f8f8...
    s.hold = 1.0f;
    g = gains(Host {}, s, 12000);
    check(std::fabs(g[9000] - 8.0f / 15.0f) < 0.01f, "a step opens it as far as its level", fmt("%.3f of 0.533", g[9000]));
}

void clickCheck()
{
    std::printf("no clicks\n");

    // the fastest attack and decay, the hardest pattern: the gain moves at most so far a sample
    Settings s;
    s.pattern = 1;   // f0f0...
    s.attack = 0.0f;
    s.hold = 0.0f;
    s.decay = 0.0f;
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        Host h;
        h.rate = rate;
        const auto g = gains(h, s, (int) rate);
        float most = 0;
        for (size_t i = 1; i < g.size(); ++i) most = std::max(most, std::fabs(g[i] - g[i - 1]));
        const float bound = 1.0f / (0.0005f * (float) rate) + 0.002f;
        check(most <= bound, "the gain never jumps", fmt("at %.0f Hz, at most %.3f a sample", rate, most));
    }
}

void stoppedCheck()
{
    std::printf("stopped\n");

    for (int mode : { Mode::Gate, Mode::Duck })
    {
        Settings s;
        s.mode = mode;
        s.pattern = 0;
        Pcg p;
        p.prepare(48000.0);
        p.setSettings(s);
        std::vector<float> x(48000, 1.0f);
        p.setHostClock(true, 0.25, 120.0);   // mid-beat, gate shut / duck clear
        p.process(x.data(), nullptr, 4000);
        p.setHostClock(false, 0.0, 120.0);
        std::fill(x.begin(), x.end(), 1.0f);
        p.process(x.data(), nullptr, (int) x.size());
        check(std::fabs(x.back() - 1.0f) < 1.0e-4f && p.step() == -1,
              mode == Mode::Gate ? "GATE, stopped: the sound passes" : "DUCK, stopped: the sound passes", fmt("gain %.5f", x.back()));
    }
}

void mixLevelCheck()
{
    std::printf("DEPTH, MIX and LEVEL\n");

    const auto in = noise(48000, 3);
    for (const char* what : { "depth", "mix" })
    {
        Settings s;
        s.pattern = 0;
        if (std::strcmp(what, "depth") == 0) s.depth = 0.0f;
        else                                 s.mix = 0.0f;
        Pcg p;
        p.prepare(48000.0);
        p.setSettings(s);
        auto y = in;
        Host h;
        h.run(p, y.data(), nullptr, (int) y.size());
        double worst = 0;
        for (size_t i = 0; i < y.size(); ++i) worst = std::max(worst, (double) std::fabs(y[i] - in[i]));
        check(worst < 1.0e-6, std::strcmp(what, "depth") == 0 ? "DEPTH at 0 leaves the sound untouched" : "MIX at dry leaves the sound untouched",
              fmt("worst %.2g", worst));
    }

    Settings s;
    s.mix = 0.0f;
    s.level = 6.0f;
    Pcg p;
    p.prepare(48000.0);
    p.setSettings(s);
    auto y = in;
    p.process(y.data(), nullptr, (int) y.size());
    check(std::fabs(y[1000] / in[1000] - 1.9953) < 1.0e-3, "LEVEL +6 dB", fmt("x%.4f", y[1000] / in[1000]));
}

void blockCheck()
{
    std::printf("blocks\n");

    Settings s;
    s.pattern = 8;
    s.hold = 0.3f;
    s.decay = 0.4f;
    const auto in = noise(44100 * 3, 7);
    std::vector<std::vector<float>> outs;
    for (int block : { 1, 37, 512, 4096 })
    {
        Host h;
        h.rate = 44100.0;
        h.bpm = 133.0;
        h.block = block;
        Pcg p;
        p.prepare(h.rate);
        p.setSettings(s);
        auto l = in, r = in;
        h.run(p, l.data(), r.data(), (int) l.size());
        outs.push_back(l);
    }
    double worst = 0;
    for (size_t o = 1; o < outs.size(); ++o)
        for (size_t i = 0; i < in.size(); ++i)
            worst = std::max(worst, (double) std::fabs(outs[o][i] - outs[0][i]));
    check(worst < 1.0e-6, "the same at block sizes 1, 37, 512 and 4096", fmt("worst difference %.2g", worst));
}

void stabilityCheck()
{
    std::printf("stability\n");

    std::mt19937 rng(101);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    bool finite = true;
    double loudest = 0;

    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Pcg p;
        p.prepare(rate);
        Host h;
        h.rate = rate;
        h.bpm = 40.0 + 260.0 * u(rng);
        h.block = 256;
        const int chunk = (int) (rate * 0.25);
        for (int round = 0; round < 40; ++round)
        {
            Settings s;
            s.pattern = (int) (u(rng) * patterns) % patterns;
            s.mode = u(rng) < 0.5f ? Mode::Gate : Mode::Duck;
            s.rate = (int) (u(rng) * 3.0f) % 3;
            s.depth = u(rng); s.attack = u(rng); s.hold = u(rng); s.decay = u(rng);
            s.mix = u(rng); s.level = -24.0f + 36.0f * u(rng);
            p.setSettings(s);
            h.playing = round % 7 != 3;
            auto l = noise((size_t) chunk, (unsigned) round), r = noise((size_t) chunk, (unsigned) round + 99);
            h.run(p, l.data(), r.data(), chunk);
            for (int i = 0; i < chunk; ++i)
            {
                finite = finite && std::isfinite(l[(size_t) i]) && std::isfinite(r[(size_t) i]);
                loudest = std::max(loudest, (double) std::max(std::fabs(l[(size_t) i]), std::fabs(r[(size_t) i])));
            }
        }
    }
    check(finite, "no NaN or infinity, any setting, any rate", "");
    check(loudest <= 0.5 * 3.99, "never louder than the input and LEVEL", fmt("peak %.2f from noise at 0.5", loudest));
}

void speedCheck()
{
    std::printf("speed\n");
    Pcg p;
    p.prepare(48000.0);
    Settings s;
    s.pattern = 2;
    p.setSettings(s);
    Host h;
    auto l = noise(48000 * 10, 1), r = noise(48000 * 10, 2);
    const auto t0 = std::chrono::steady_clock::now();
    h.run(p, l.data(), r.data(), (int) l.size());
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    check(10.0 / secs > 100.0, "stereo at 48 kHz, faster than 100x real time", fmt("%.0fx", 10.0 / secs));
}

//============================================================================== the demo renders

void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int rate)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) { std::printf("  could not write %s\n", path.c_str()); return; }

    const uint32_t frames = (uint32_t) l.size(), bytes = frames * 4;
    auto u32 = [f] (uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [f] (uint16_t v) { std::fwrite(&v, 2, 1, f); };

    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(2); u32((uint32_t) rate); u32((uint32_t) rate * 4); u16(4); u16(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    for (uint32_t i = 0; i < frames; ++i)
        for (float v : { l[i], r[i] })
        {
            const int16_t s = (int16_t) std::lrint(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
            std::fwrite(&s, 2, 1, f);
        }
    std::fclose(f);
    std::printf("  wrote %s\n", path.c_str());
}

// A held minor chord of detuned saws: a pad to gate.
std::vector<float> chord(int n, int rate, double detune)
{
    const double notes[] = { 130.81, 155.56, 196.00, 261.63 };
    std::vector<float> v((size_t) n, 0.0f);
    for (double f : notes)
    {
        const double inc = f * detune / rate;
        double ph = f / 1000.0;
        for (int i = 0; i < n; ++i)
        {
            v[(size_t) i] += (float) (0.12 * (2.0 * ph - 1.0));
            ph += inc;
            if (ph >= 1.0) ph -= 1.0;
        }
    }
    return v;
}

void demos(const std::string& dir)
{
    std::printf("demo renders\n");

    const int rate = 48000;
    const int twoBars = (int) (rate * 60.0 / 132.0 * 8.0);

    struct Demo { const char* name; int mode; std::vector<int> patternsPlayed; float hold, decay; };
    const Demo list[] = {
        { "pcg-trance-gate", Mode::Gate, { 2, 1, 8, 16, 29 }, 0.6f, 0.05f },
        { "pcg-pump",        Mode::Duck, { 0, 0, 22, 2, 18 }, 0.0f, 0.35f },
    };

    for (const auto& d : list)
    {
        const int n = twoBars * (int) d.patternsPlayed.size();
        auto l = chord(n, rate, 1.0), r = chord(n, rate, 1.004);

        Pcg p;
        p.prepare(rate);
        Host h;
        h.rate = rate;
        h.bpm = 132.0;
        for (size_t k = 0; k < d.patternsPlayed.size(); ++k)
        {
            Settings s;
            s.pattern = d.patternsPlayed[k];
            s.mode = d.mode;
            s.hold = d.hold;
            s.decay = d.decay;
            s.attack = 0.15f;
            p.setSettings(s);
            h.run(p, l.data() + k * twoBars, r.data() + k * twoBars, twoBars);
        }
        writeWav(dir + "/" + d.name + ".wav", l, r, rate);
    }
}

} // namespace

int main(int argc, char** argv)
{
    gridCheck();
    shapeCheck();
    clickCheck();
    stoppedCheck();
    mixLevelCheck();
    blockCheck();
    stabilityCheck();
    speedCheck();

    if (argc >= 3 && std::strcmp(argv[1], "--wav") == 0)
        demos(argv[2]);

    std::printf("\n%s\n", failures == 0 ? "all checks passed" : (std::to_string(failures) + " FAILED").c_str());
    return failures == 0 ? 0 : 1;
}
