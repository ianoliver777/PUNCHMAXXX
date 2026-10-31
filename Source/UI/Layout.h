#pragma once
#include <juce_graphics/juce_graphics.h>

// Every coordinate here is in the 1200 x 800 space of the UI concept art.
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
    { "transient",  311.5f, 458.5f, 76.4f, 5.2f, 66.0f, 28.0f, 3.6f, true  },
    { "punch",      519.5f, 458.5f, 75.6f, 5.0f, 66.0f, 28.0f, 3.6f, false },
    { "body",       724.0f, 458.5f, 76.3f, 5.3f, 66.0f, 28.0f, 3.6f, true  },
    { "drive",      915.0f, 279.0f, 50.1f, 4.3f, 45.0f, 14.0f, 3.0f, false },
    { "saturation", 915.0f, 435.5f, 47.5f, 5.0f, 44.0f, 14.0f, 3.0f, false },
    { "output",    1080.5f, 485.5f, 0.0f,  0.0f, 36.0f,  7.0f, 2.6f, true  },
    { "focus",      396.5f, 672.5f, 0.0f,  0.0f, 37.0f,  4.0f, 2.6f, false },
    { "toneLow",    584.5f, 661.5f, 0.0f,  0.0f, 36.0f,  6.0f, 2.6f, true  },
    { "toneHigh",   719.5f, 661.5f, 0.0f,  0.0f, 36.0f,  6.0f, 2.6f, true  },
    { "width",      887.5f, 661.5f, 43.6f, 3.6f, 41.0f,  8.0f, 2.6f, true  },
    { "mix",       1067.5f, 661.5f, 42.9f, 4.0f, 41.0f,  8.0f, 2.6f, false },
};

inline constexpr float startAngle = -0.75f * juce::MathConstants<float>::pi;  // 7:30
inline constexpr float endAngle   =  0.75f * juce::MathConstants<float>::pi;  // 4:30

inline const juce::Rectangle<int> modeButtons[] = {
    { 62, 264, 116, 47 }, { 62, 324, 116, 48 }, { 62, 388, 116, 49 }, { 62, 453, 116, 49 } };
inline const juce::Point<float> modeLed { 78.5f, 237.5f };

inline const juce::Rectangle<int> shapeButtons[] = {
    { 60, 631, 48, 45 }, { 115, 631, 48, 45 }, { 169, 631, 47, 45 }, { 222, 631, 48, 45 } };

// preset bar
inline const juce::Rectangle<int> btnImport  { 437, 142, 58, 38 };
inline const juce::Rectangle<int> btnFolder  { 502, 142, 40, 38 };
inline const juce::Rectangle<int> presetBox  { 543, 142, 262, 38 };
inline const juce::Rectangle<int> btnPrev    { 812, 142, 40, 38 };
inline const juce::Rectangle<int> btnNext    { 859, 142, 41, 38 };
inline const juce::Rectangle<int> btnSave    { 912, 142, 91, 38 };
inline const juce::Rectangle<int> btnInit    { 1012, 142, 93, 38 };
inline const juce::Rectangle<int> btnMenu    { 1112, 142, 58, 38 };

inline const juce::Rectangle<int> display { 201, 208, 610, 146 };

// output meter: two LED columns + the dB scale printed next to them
inline const juce::Rectangle<int> meterL { 1050, 253, 17, 169 };
inline const juce::Rectangle<int> meterR { 1077, 253, 17, 169 };
struct MeterTick { float db, y; };
inline const MeterTick meterScale[] = { { 6, 256 }, { 0, 282 }, { -6, 308 }, { -12, 335 }, { -18, 361 }, { -24, 386 }, { -36, 412 }, { -60, 422 } };

inline const juce::Point<float> focusReadout { 398.5f, 621.0f };
inline const juce::Point<float> focusMinLabel { 329.0f, 699.5f };
} // namespace pmx::layout
