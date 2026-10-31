#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace pmx
{
namespace ID
{
    inline constexpr const char* transient  = "transient";
    inline constexpr const char* punch      = "punch";
    inline constexpr const char* body       = "body";
    inline constexpr const char* mode       = "mode";
    inline constexpr const char* shape      = "shape";
    inline constexpr const char* focus      = "focus";
    inline constexpr const char* drive      = "drive";
    inline constexpr const char* saturation = "saturation";
    inline constexpr const char* toneLow    = "toneLow";
    inline constexpr const char* toneHigh   = "toneHigh";
    inline constexpr const char* width      = "width";
    inline constexpr const char* mix        = "mix";
    inline constexpr const char* output     = "output";
}

enum class Mode  { Tight = 0, Punch, Fat, Clean };
enum class Shape { Soft = 0, Smooth, Hard, Exp };

inline const juce::StringArray& allParamIDs()
{
    static const juce::StringArray ids { ID::transient, ID::punch, ID::body, ID::mode, ID::shape, ID::focus,
                                         ID::drive, ID::saturation, ID::toneLow, ID::toneHigh,
                                         ID::width, ID::mix, ID::output };
    return ids;
}

inline constexpr float kOutputFloorDb = -60.0f; // treated as -inf

// Focus: 50..800 Hz, log, 200 Hz at the 12 o'clock position (matches the dial labels)
inline juce::NormalisableRange<float> focusRange()
{
    return { 50.0f, 800.0f,
             [] (float, float, float x) { return 50.0f * std::pow (16.0f, x); },
             [] (float, float, float hz) { return std::log (juce::jlimit (50.0f, 800.0f, hz) / 50.0f) / std::log (16.0f); },
             [] (float, float, float hz) { return juce::jlimit (50.0f, 800.0f, hz); } };
}

// Output: -inf .. +6 dB with 0 dB at 12 o'clock
inline juce::NormalisableRange<float> outputRange()
{
    return { kOutputFloorDb, 6.0f,
             [] (float, float, float x)
             {
                 if (x <= 0.0f) return kOutputFloorDb;
                 if (x <= 0.5f) return juce::jmax (kOutputFloorDb, 40.0f * std::log10 (x / 0.5f));
                 return 12.0f * (x - 0.5f);
             },
             [] (float, float, float db)
             {
                 if (db <= kOutputFloorDb) return 0.0f;
                 if (db <= 0.0f) return 0.5f * std::pow (10.0f, db / 40.0f);
                 return juce::jmin (1.0f, 0.5f + db / 12.0f);
             },
             [] (float, float, float db) { return juce::jlimit (kOutputFloorDb, 6.0f, db); } };
}

inline juce::String formatSigned (float v, int decimals = 0)
{
    auto s = juce::String (v, decimals);
    return v > 0.0f ? "+" + s : s;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto pct = A().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)); });
    auto bip = A().withStringFromValueFunction ([] (float v, int) { return formatSigned ((float) roundToInt (v)); });

    l.add (std::make_unique<P> (ParameterID { ID::transient, 1 }, "Transient", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, bip));
    l.add (std::make_unique<P> (ParameterID { ID::punch, 1 },     "Punch",     NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct));
    l.add (std::make_unique<P> (ParameterID { ID::body, 1 },      "Body",      NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pct));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::mode, 1 },  "Mode",  StringArray { "Tight", "Punch", "Fat", "Clean" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::shape, 1 }, "Shape", StringArray { "Soft", "Smooth", "Hard", "Exp" }, 1));
    l.add (std::make_unique<P> (ParameterID { ID::focus, 1 }, "Focus", focusRange(), 200.0f,
                                A().withLabel ("Hz").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " Hz"; })));
    l.add (std::make_unique<P> (ParameterID { ID::drive, 1 },      "Drive",      NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct));
    l.add (std::make_unique<P> (ParameterID { ID::saturation, 1 }, "Saturation", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct));
    auto db = A().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return formatSigned (v, 1) + " dB"; });
    l.add (std::make_unique<P> (ParameterID { ID::toneLow, 1 },  "Tone Low",  NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, db));
    l.add (std::make_unique<P> (ParameterID { ID::toneHigh, 1 }, "Tone High", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, db));
    l.add (std::make_unique<P> (ParameterID { ID::width, 1 }, "Width", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
                                A().withStringFromValueFunction ([] (float v, int)
                                {
                                    if (v < 0.5f) return String ("MONO");
                                    return String (roundToInt (v * 2.0f)) + "%";
                                })));
    l.add (std::make_unique<P> (ParameterID { ID::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                A().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; })));
    l.add (std::make_unique<P> (ParameterID { ID::output, 1 }, "Output", outputRange(), 0.0f,
                                A().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
                                {
                                    if (v <= kOutputFloorDb + 0.01f) return String ("-inf dB");
                                    return formatSigned (v, 1) + " dB";
                                })));
    return l;
}
} // namespace pmx
