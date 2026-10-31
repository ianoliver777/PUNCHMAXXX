#pragma once
#include <juce_graphics/juce_graphics.h>

// Every coordinate here is in a 1200 x 800 logical space (the 1536 x 1024 art scaled by 0.78125).
namespace pmx::layout
{
inline constexpr int width = 1200, height = 800;

struct KnobSpec
{
    const char* paramID;
    float cx, cy;
    float arcR, arcThick;      // arcThick == 0 -> pointer-only knob (no value arc in the art)
    float ptrOuter, ptrInner, ptrWidth;
    bool bipolar;              // arc grows from 12 o'clock instead of from the minimum
};

inline const KnobSpec knobs[] = {
    { "transient",   312.9f, 453.3f, 70.7f, 4.7f, 60.2f, 23.4f, 3.4f, true  },
    { "punch",       521.4f, 453.5f, 70.7f, 4.7f, 60.2f, 23.4f, 3.4f, false },
    { "body",        726.0f, 453.1f, 70.7f, 4.7f, 60.2f, 23.4f, 3.4f, true  },
    { "drive",       915.8f, 275.4f, 46.5f, 3.8f, 41.4f, 13.3f, 2.8f, false },
    { "saturation",  916.2f, 436.5f, 46.9f, 3.9f, 41.4f, 13.3f, 2.8f, false },
    { "output",     1082.3f, 485.5f,  0.0f, 0.0f, 37.1f,  8.2f, 2.5f, true  },
    { "focus",       398.4f, 677.7f,  0.0f, 0.0f, 37.1f,  9.4f, 2.5f, false },
    { "toneLow",     584.5f, 661.3f,  0.0f, 0.0f, 37.1f, 21.9f, 2.5f, true  },
    { "toneHigh",    720.8f, 661.3f,  0.0f, 0.0f, 36.3f, 21.9f, 2.5f, true  },
    { "width",       888.0f, 663.7f, 42.1f, 3.3f, 38.7f, 15.6f, 2.3f, true  },
    { "mix",        1068.3f, 664.0f, 42.1f, 3.3f, 38.3f, 15.6f, 2.3f, false },
};

inline constexpr float startAngle = -0.75f * juce::MathConstants<float>::pi;  // 7:30
inline constexpr float endAngle   =  0.75f * juce::MathConstants<float>::pi;  // 4:30

inline const juce::Rectangle<int> modeButtons[] = {
    { 64, 263, 114, 47 }, { 64, 326, 114, 47 }, { 64, 388, 114, 47 }, { 64, 454, 114, 46 } };
inline const juce::Point<float> modeLed { 78.4f, 238.0f };

inline const juce::Rectangle<int> shapeButtons[] = {
    { 59, 630, 48, 47 }, { 114, 630, 47, 47 }, { 168, 630, 46, 47 }, { 220, 630, 48, 47 } };

// preset bar
inline const juce::Rectangle<int> btnImport  { 435, 139, 60, 41 };
inline const juce::Rectangle<int> btnFolder  { 501, 139, 44, 41 };
inline const juce::Rectangle<int> presetBox  { 545, 139, 262, 41 };
inline const juce::Rectangle<int> btnPrev    { 813, 139, 41, 41 };
inline const juce::Rectangle<int> btnNext    { 859, 139, 41, 41 };
inline const juce::Rectangle<int> btnSave    { 912, 139, 93, 41 };
inline const juce::Rectangle<int> btnInit    { 1012, 139, 91, 41 };
inline const juce::Rectangle<int> btnMenu    { 1110, 139, 56, 41 };

inline const juce::Rectangle<int> display { 205, 216, 612, 142 };

// output meter: two LED columns + the dB scale printed next to them
inline const juce::Rectangle<int> meterL { 1048, 253, 19, 169 };
inline const juce::Rectangle<int> meterR { 1077, 253, 19, 169 };
struct MeterTick { float db, y; };
inline const MeterTick meterScale[] = { { 6, 257.0f }, { 0, 283.2f }, { -6, 308.6f }, { -12, 335.2f }, { -18, 360.5f }, { -24, 386.7f }, { -36, 412.1f }, { -60, 421.9f } };

inline const juce::Point<float> focusReadout { 398.4f, 621.1f };
inline const juce::Point<float> focusMinLabel { 326.6f, 700.0f };
} // namespace pmx::layout
