#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <vector>
#include <algorithm>

// All PUNCHMAXXX processing is pure gain-based (no filters in the signal path),
// so it introduces ZERO phase shift. The only filters are the linear-phase FIR
// oversamplers, and total latency is kept constant and reported to the host.

namespace pmx
{
inline float dbToGain (float db) noexcept { return std::exp (db * 0.11512925465f); } // ln(10)/20
inline float gainToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-6f)); }
inline float timeCoef (float ms, double fs) noexcept
{
    return (float) std::exp (-1.0 / (std::max (0.001, (double) ms) * 0.001 * fs));
}

//==============================================================================
// Feed-forward, stereo-linked, soft-knee compressor with gentle auto make-up.
class Compressor
{
public:
    void prepare (double newFs) { fs = newFs; reset(); }
    void reset()                { env = 0.0f; lastGr = 0.0f; }

    void setParams (float threshDb, float ratio_, float attackMs, float releaseMs) noexcept
    {
        thresh = threshDb;
        ratio  = std::max (1.0f, ratio_);
        atk    = timeCoef (attackMs, fs);
        rel    = timeCoef (releaseMs, fs);
        // ~40% of the static gain reduction at 0 dBFS is given back: keeps drums
        // fat without massive level jumps when you pull the threshold down.
        makeup = -0.4f * thresh * (1.0f - 1.0f / ratio);
    }

    // returns linear gain to apply (amount 0..1 = section on/off crossfade)
    inline float process (float linkedPeak, float amount) noexcept
    {
        const float x = gainToDb (linkedPeak);
        const float d = x - thresh;
        float y;
        if (2.0f * d < -knee)                 y = x;
        else if (2.0f * std::abs (d) <= knee) { const float t = d + knee * 0.5f; y = x + (1.0f / ratio - 1.0f) * t * t / (2.0f * knee); }
        else                                  y = thresh + d / ratio;

        const float gr = y - x; // <= 0
        env = gr < env ? atk * env + (1.0f - atk) * gr
                       : rel * env + (1.0f - rel) * gr;
        lastGr = env * amount;
        return dbToGain ((env + makeup) * amount);
    }

    float getGrDb() const noexcept { return lastGr; }

private:
    double fs = 44100.0;
    float thresh = 0.0f, ratio = 4.0f, atk = 0.0f, rel = 0.0f, makeup = 0.0f;
    float knee = 6.0f, env = 0.0f, lastGr = 0.0f;
};

//==============================================================================
// Differential-envelope transient shaper (level independent, no lookahead,
// no filters -> no phase smear). Attack/Sustain in -1..+1.
class TransientShaper
{
public:
    void prepare (double newFs)
    {
        fs = newFs;
        fastA = timeCoef (0.3f, fs);   fastR = timeCoef (60.0f, fs);
        slowA = timeCoef (25.0f, fs);
        susR  = timeCoef (350.0f, fs);
        smooth = timeCoef (0.25f, fs);
        reset();
    }
    void reset() { eFast = eSlow = eSus = 1.0e-6f; g = 0.0f; }

    inline float process (float linkedPeak, float attackAmt, float sustainAmt, float amount) noexcept
    {
        const float x = linkedPeak;
        eFast = x > eFast ? fastA * eFast + (1.0f - fastA) * x : fastR * eFast + (1.0f - fastR) * x;
        eSlow = x > eSlow ? slowA * eSlow + (1.0f - slowA) * x : fastR * eSlow + (1.0f - fastR) * x;
        eSus  = x > eSus  ? fastA * eSus  + (1.0f - fastA) * x : susR  * eSus  + (1.0f - susR)  * x;

        const float dFast = gainToDb (eFast);
        const float atkDiff = juce::jlimit (0.0f, 30.0f, dFast - gainToDb (eSlow));
        const float susDiff = juce::jlimit (0.0f, 30.0f, gainToDb (eSus) - dFast);

        // don't pump up the noise floor on silence
        const float gate = juce::jlimit (0.0f, 1.0f, (dFast + 70.0f) / 20.0f);

        float target = attackAmt  * (attackAmt  > 0.0f ? 0.9f : 1.2f) * atkDiff
                     + sustainAmt * (sustainAmt > 0.0f ? 0.6f : 0.9f) * susDiff * gate;
        target = juce::jlimit (-24.0f, 15.0f, target) * amount;

        g = smooth * g + (1.0f - smooth) * target;
        return dbToGain (g);
    }

private:
    double fs = 44100.0;
    float fastA = 0, fastR = 0, slowA = 0, susR = 0, smooth = 0;
    float eFast = 1.0e-6f, eSlow = 1.0e-6f, eSus = 1.0e-6f, g = 0.0f;
};

//==============================================================================
// Lookahead brick-wall limiter. Sliding-minimum + box-filter gain smoothing
// guarantees the ceiling with a smooth (click-free, distortion-free) gain curve.
class Limiter
{
public:
    void prepare (int lookaheadSamples, double newFs, int numCh)
    {
        L  = std::max (1, lookaheadSamples);
        fs = newFs;
        delay.assign ((size_t) numCh, std::vector<float> ((size_t) L, 0.0f));
        dqIdx.assign ((size_t) L + 2, 0);
        dqVal.assign ((size_t) L + 2, 1.0f);
        box.assign ((size_t) L, 1.0f);
        relC = timeCoef (80.0f, fs);
        reset();
    }

    void reset()
    {
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        std::fill (box.begin(), box.end(), 1.0f);
        boxSum = (double) L; boxPos = 0; dPos = 0;
        head = tail = 0; n = 0; r = 1.0f; lastGain = 1.0f; recount = 0;
        for (auto& h : hist) std::fill (std::begin (h), std::end (h), 0.0f);
    }

    int getLatency() const noexcept { return L; }

    // processes one frame in place. ceilingLin = linear ceiling, amount = on/off crossfade
    inline void process (float* frame, int numCh, float ceilingLin, float amount) noexcept
    {
        // sample peak + inter-sample (true) peak estimate between the previous two samples
        float peak = 0.0f;
        for (int c = 0; c < numCh && c < 2; ++c)
        {
            auto& h = hist[c];
            h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; h[3] = frame[c];
            peak = std::max (peak, std::abs (frame[c]));
            for (float t : { 0.25f, 0.5f, 0.75f })
            {
                // cubic Hermite between h[1] and h[2]
                const float c0 = h[1], c1 = 0.5f * (h[2] - h[0]);
                const float c2 = h[0] - 2.5f * h[1] + 2.0f * h[2] - 0.5f * h[3];
                const float c3 = 0.5f * (h[3] - h[0]) + 1.5f * (h[1] - h[2]);
                peak = std::max (peak, std::abs (((c3 * t + c2) * t + c1) * t + c0));
            }
        }
        const float gReq = peak > ceilingLin ? ceilingLin / peak : 1.0f;

        // sliding minimum over window [n-L, n]  (monotonic deque)
        const int cap = L + 2;
        while (head != tail && dqVal[(size_t) ((tail - 1 + cap) % cap)] >= gReq) tail = (tail - 1 + cap) % cap;
        dqIdx[(size_t) tail] = n; dqVal[(size_t) tail] = gReq; tail = (tail + 1) % cap;
        while (dqIdx[(size_t) head] < n - L) head = (head + 1) % cap;
        const float h = dqVal[(size_t) head];

        // instant attack / smooth release (r <= h always)
        r = h < r ? h : h + (r - h) * relC;

        // box filter of length L
        boxSum += (double) r - (double) box[(size_t) boxPos];
        box[(size_t) boxPos] = r;
        boxPos = (boxPos + 1) % L;
        if (++recount >= 4096) { recount = 0; boxSum = 0; for (auto v : box) boxSum += v; }
        const float gain = 1.0f + ((float) (boxSum / (double) L) - 1.0f) * amount;
        lastGain = gain;
        const float clampLevel = amount > 0.999f ? ceilingLin : 1.0e6f;

        // delay audio by L, apply gain
        for (int c = 0; c < numCh; ++c)
        {
            auto& d = delay[(size_t) c];
            const float delayed = d[(size_t) dPos];
            d[(size_t) dPos] = frame[c];
            frame[c] = juce::jlimit (-clampLevel, clampLevel, delayed * gain);
        }
        dPos = (dPos + 1) % L;   // read-before-write on a ring of length L = delay of exactly L
        ++n;
    }

    float getLastGain() const noexcept { return lastGain; }

private:
    int L = 64, boxPos = 0, dPos = 0, head = 0, tail = 0, recount = 0;
    long long n = 0;
    double fs = 44100.0, boxSum = 0;
    float relC = 0.0f, r = 1.0f, lastGain = 1.0f;
    std::vector<std::vector<float>> delay;
    std::vector<long long> dqIdx;
    std::vector<float> dqVal, box;
    float hist[2][4] {};
};

//==============================================================================
// Simple integer-sample multichannel delay (latency alignment).
class AlignDelay
{
public:
    void prepare (int numCh, int maxDelay)
    {
        buf.setSize (numCh, std::max (1, maxDelay) + 1);
        buf.clear(); pos = 0;
    }
    void setDelay (int d) { delaySamples = juce::jlimit (0, buf.getNumSamples() - 1, d); }
    void reset() { buf.clear(); pos = 0; }

    void process (juce::AudioBuffer<float>& b, int numSamples)
    {
        process (b.getArrayOfWritePointers(), b.getNumChannels(), numSamples);
    }

    void process (float* const* ch, int numChans, int numSamples)
    {
        if (delaySamples == 0) return;
        const int size = buf.getNumSamples();
        const int chans = std::min (numChans, buf.getNumChannels());
        for (int i = 0; i < numSamples; ++i)
        {
            const int readPos = (pos - delaySamples + size) % size;
            for (int c = 0; c < chans; ++c)
            {
                auto* d = buf.getWritePointer (c);
                d[pos] = ch[c][i];
                ch[c][i] = d[readPos];
            }
            pos = (pos + 1) % size;
        }
    }

private:
    juce::AudioBuffer<float> buf;
    int pos = 0, delaySamples = 0;
};
} // namespace pmx
