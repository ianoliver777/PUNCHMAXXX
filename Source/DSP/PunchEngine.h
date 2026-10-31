#pragma once
#include <juce_dsp/juce_dsp.h>
#include "../Parameters.h"

namespace pmx
{
/** The wet signal path of PUNCHMAXXX.

    lookahead delay -> split at FOCUS (LR4) -> transient / punch / body gains
    -> 2x oversampled drive + saturation -> tone shelves -> stereo width

    Detection uses the classic "differential envelope" approach: a fast-attack
    follower vs a slow-attack follower finds the hit, a slow-release follower vs
    a fast-release follower finds the tail. No threshold to set, so it reacts the
    same on quiet and loud drums. */
class PunchEngine
{
public:
    struct Settings
    {
        float transient = 0.0f;   // -100..100
        float punch = 0.0f;       // 0..100
        float body = 50.0f;       // 0..100 (50 = untouched)
        Mode mode = Mode::Punch;
        Shape shape = Shape::Smooth;
        float focusHz = 200.0f;
        float drive = 0.0f;       // 0..100
        float saturation = 0.0f;  // 0..100
        float lowDb = 0.0f, highDb = 0.0f;
        float width = 50.0f;      // 0..100 (50 = untouched)
    };

    void prepare (double sampleRate, int maxBlock, int channels)
    {
        sr = sampleRate;
        numCh = juce::jlimit (1, 2, channels);
        lookahead = juce::jmax (1, juce::roundToInt (sr * 0.0015));

        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, (juce::uint32) numCh };

        delay.prepare (spec);
        delay.setMaximumDelayInSamples (lookahead + 4);
        delay.setDelay ((float) lookahead);

        xover.prepare (spec);
        xover.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        xover.setCutoffFrequency (settings.focusHz);

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) numCh, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        oversampler->initProcessing ((size_t) maxBlock);

        for (auto& f : lowShelf)  { f.reset(); }
        for (auto& f : highShelf) { f.reset(); }
        updateTone (true);

        dcCoefOs = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 8.0 / (sr * 2.0));
        scHpCoef = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 40.0 / sr);

        driveSm.reset (sr * 2.0, 0.03);
        satSm.reset (sr * 2.0, 0.03);
        widthSm.reset (sr, 0.03);
        driveSm.setCurrentAndTargetValue (settings.drive * 0.01f);
        satSm.setCurrentAndTargetValue (settings.saturation * 0.01f);
        widthSm.setCurrentAndTargetValue (settings.width / 50.0f);

        gainTrace.assign ((size_t) maxBlock, 0.0f);
        updateTimings();
        reset();
    }

    void reset()
    {
        delay.reset();
        xover.reset();
        if (oversampler) oversampler->reset();
        for (auto& f : lowShelf)  f.reset();
        for (auto& f : highShelf) f.reset();
        eFA = eSA = eFR = eSR = 0.0f;
        gHiDb = gLoDb = 0.0f;
        scPrevIn = scPrevOut = 0.0f;
        satDcIn[0] = satDcIn[1] = satDcOut[0] = satDcOut[1] = 0.0f;
    }

    void setSettings (const Settings& s)
    {
        const bool timingChanged = s.mode != settings.mode || s.shape != settings.shape;
        const bool toneChanged = std::abs (s.lowDb - settings.lowDb) > 0.01f || std::abs (s.highDb - settings.highDb) > 0.01f;
        settings = s;
        if (timingChanged) updateTimings();
        if (toneChanged) updateTone (false);
        xover.setCutoffFrequency (juce::jlimit (50.0f, 800.0f, s.focusHz));
        driveSm.setTargetValue (s.drive * 0.01f);
        satSm.setTargetValue (s.saturation * 0.01f);
        widthSm.setTargetValue (s.width / 50.0f);
    }

    int getLatencySamples() const
    {
        return lookahead + (oversampler ? (int) std::lround (oversampler->getLatencyInSamples()) : 0);
    }

    /** Per-sample combined transient+body gain in dB from the last process() call (for the display). */
    const float* getGainTrace() const { return gainTrace.data(); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        const int chans = juce::jmin (numCh, buffer.getNumChannels());
        if (n == 0 || chans == 0) return;
        if ((int) gainTrace.size() < n) gainTrace.resize ((size_t) n);

        const float T = settings.transient * 0.01f;
        const float P = settings.punch * 0.01f;
        const float B = (settings.body - 50.0f) / 50.0f;
        const float bodyScale = B >= 0.0f ? 12.0f : 24.0f;

        float* ch0 = buffer.getWritePointer (0);
        float* ch1 = chans > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            // ---- detection on the un-delayed signal (this is our lookahead) ----
            const float mono = ch1 ? 0.5f * (ch0[i] + ch1[i]) : ch0[i];
            const float hp = scHpCoef * (scPrevOut + mono - scPrevIn);
            scPrevIn = mono; scPrevOut = hp;
            const float lvl = std::abs (hp);

            follow (eFA, lvl, aFastAtt, aAttRel);
            follow (eSA, lvl, aSlowAtt, aAttRel);
            follow (eFR, lvl, aFastAtt, aSusFastRel);
            follow (eSR, lvl, aFastAtt, aSusSlowRel);

            constexpr float eps = 1.0e-7f;
            const float attDb = juce::jmax (0.0f, 20.0f * std::log10 ((eFA + eps) / (eSA + eps)));
            const float susDb = juce::jmax (0.0f, 20.0f * std::log10 ((eSR + eps) / (eFR + eps)));

            // ignore the noise floor so we never pump up hiss between hits
            const float levelDb = 20.0f * std::log10 (eSR + eps);
            const float gate = smoothstep (juce::jlimit (0.0f, 1.0f, (levelDb + 66.0f) / 20.0f));

            const float fa = shapeCurve (juce::jlimit (0.0f, 1.0f, attDb / attRange));
            const float xs = juce::jlimit (0.0f, 1.0f, susDb / 24.0f);

            const float attGain = T * 18.0f * fa * gate;
            const float punchGain = P * 12.0f * punchBoost * fa * gate;
            const float susGain = B * bodyScale * xs * gate;

            const float targetHi = juce::jlimit (-36.0f, 24.0f, attGain + susGain);
            const float targetLo = juce::jlimit (-36.0f, 24.0f, attGain + punchGain + susGain);
            gHiDb += (targetHi - gHiDb) * (1.0f - (targetHi > gHiDb ? aGainUp : aGainDown));
            gLoDb += (targetLo - gLoDb) * (1.0f - (targetLo > gLoDb ? aGainUp : aGainDown));

            const float gHi = juce::Decibels::decibelsToGain (gHiDb);
            const float gLo = juce::Decibels::decibelsToGain (gLoDb);
            gainTrace[(size_t) i] = 0.5f * (gHiDb + gLoDb);

            // ---- apply to the delayed signal, split into low/high bands ----
            for (int c = 0; c < chans; ++c)
            {
                float* d = c == 0 ? ch0 : ch1;
                delay.pushSample (c, d[i]);
                const float x = delay.popSample (c);
                // complementary split: when both gains match this is exactly x (no crossover phase smear,
                // so parallel MIX never comb-filters)
                const float lo = xover.processSample (c, x);
                d[i] = x * gHi + lo * (gLo - gHi);
            }
        }

        // ---- drive + saturation, 2x oversampled ----
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) chans, (size_t) n);
        auto up = oversampler->processSamplesUp (block);
        const int un = (int) up.getNumSamples();
        for (int i = 0; i < un; ++i)
        {
            const float d = driveSm.getNextValue();
            const float s = satSm.getNextValue();
            if (d < 1.0e-4f && s < 1.0e-4f) continue;

            const float g = juce::Decibels::decibelsToGain (d * 18.0f);
            const float makeup = 1.0f / std::sqrt (g);
            const float driveMix = juce::jmin (1.0f, d * 4.0f);
            const float k = 1.0f + 2.0f * s;

            for (int c = 0; c < chans; ++c)
            {
                float& x = up.getChannelPointer ((size_t) c)[i];
                float y = x + driveMix * (softClip (x * g) * makeup - x);
                const float v = y + 0.15f * s * y * y;           // even harmonics (asymmetric)
                const float contrib = s * (std::tanh (k * v) / k - y);
                // DC-block only what the saturator adds, so the clean path stays untouched
                const float hp = contrib - satDcIn[c] + dcCoefOs * satDcOut[c];
                satDcIn[c] = contrib; satDcOut[c] = hp;
                x = y + hp;
            }
        }
        oversampler->processSamplesDown (block);

        // ---- tone shelves (exact identity at 0 dB), width ----
        for (int c = 0; c < chans; ++c)
        {
            float* d = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                d[i] = highShelf[(size_t) c].processSample (lowShelf[(size_t) c].processSample (d[i]));
        }

        if (chans == 2)
        {
            for (int i = 0; i < n; ++i)
            {
                const float w = widthSm.getNextValue();
                const float m = 0.5f * (ch0[i] + ch1[i]);
                const float s = 0.5f * (ch0[i] - ch1[i]) * w;
                ch0[i] = m + s;
                ch1[i] = m - s;
            }
        }
        else
        {
            widthSm.skip (n);
        }
    }

private:
    static void follow (float& env, float x, float aAtt, float aRel) noexcept
    {
        const float a = x > env ? aAtt : aRel;
        env = x + a * (env - x);
    }

    static float smoothstep (float x) noexcept { return x * x * (3.0f - 2.0f * x); }

    static float softClip (float u) noexcept
    {
        constexpr float n = 2.5f;
        return u / std::pow (1.0f + std::pow (std::abs (u), n), 1.0f / n);
    }

    float shapeCurve (float x) const noexcept
    {
        switch (settings.shape)
        {
            case Shape::Soft:   return 1.0f - (1.0f - x) * (1.0f - x);
            case Shape::Smooth: return smoothstep (x);
            case Shape::Hard:   return juce::jmin (1.0f, x * 4.0f);
            case Shape::Exp:    return x * x;
        }
        return x;
    }

    float coef (float ms) const { return (float) std::exp (-1.0 / (juce::jmax (0.01f, ms) * 0.001 * sr)); }

    void updateTimings()
    {
        // fastAtt, slowAtt, attRel, susFastRel, susSlowRel, attRange(dB), punchBoost
        struct T { float fa, sa, ar, sfr, ssr, range, punch; };
        static constexpr T table[] = {
            { 0.10f,  6.0f, 30.0f, 15.0f, 140.0f, 15.0f, 1.00f },  // Tight
            { 0.20f, 12.0f, 50.0f, 25.0f, 260.0f, 18.0f, 1.25f },  // Punch
            { 0.50f, 22.0f, 80.0f, 40.0f, 480.0f, 20.0f, 1.10f },  // Fat
            { 0.30f, 10.0f, 50.0f, 30.0f, 300.0f, 18.0f, 1.00f },  // Clean
        };
        const auto& t = table[(int) settings.mode];
        aFastAtt = coef (t.fa);
        aSlowAtt = coef (t.sa);
        aAttRel = coef (t.ar);
        aSusFastRel = coef (t.sfr);
        aSusSlowRel = coef (t.ssr);
        attRange = t.range;
        punchBoost = t.punch;

        // how fast the applied gain can move: Hard snaps, Soft glides
        static constexpr float upMs[]   = { 2.5f, 1.2f, 0.15f, 0.8f };
        static constexpr float downMs[] = { 8.0f, 5.0f, 1.5f,  4.0f };
        const float cleanSlow = settings.mode == Mode::Clean ? 1.6f : 1.0f;
        aGainUp = coef (upMs[(int) settings.shape] * cleanSlow);
        aGainDown = coef (downMs[(int) settings.shape] * cleanSlow);
    }

    void updateTone (bool force)
    {
        juce::ignoreUnused (force);
        auto lo = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 120.0, 0.7071, juce::Decibels::decibelsToGain (settings.lowDb));
        auto hi = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 7000.0, 0.7071, juce::Decibels::decibelsToGain (settings.highDb));
        for (auto& f : lowShelf)  f.coefficients = lo;
        for (auto& f : highShelf) f.coefficients = hi;
    }

    Settings settings;
    double sr = 44100.0;
    int numCh = 2, lookahead = 64;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> delay { 4096 };
    juce::dsp::LinkwitzRileyFilter<float> xover;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::array<juce::dsp::IIR::Filter<float>, 2> lowShelf, highShelf;

    juce::SmoothedValue<float> driveSm, satSm, widthSm;

    float aFastAtt = 0, aSlowAtt = 0, aAttRel = 0, aSusFastRel = 0, aSusSlowRel = 0;
    float attRange = 18.0f, punchBoost = 1.0f, aGainUp = 0, aGainDown = 0;
    float eFA = 0, eSA = 0, eFR = 0, eSR = 0, gHiDb = 0, gLoDb = 0;
    float scHpCoef = 0, scPrevIn = 0, scPrevOut = 0;
    float dcCoefOs = 0, satDcIn[2] {}, satDcOut[2] {};
    std::vector<float> gainTrace;
};
} // namespace pmx
