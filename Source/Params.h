#pragma once
#include <JuceHeader.h>

namespace pmx
{
namespace ids
{
    inline constexpr const char* compOn       = "compOn";
    inline constexpr const char* thresh       = "thresh";
    inline constexpr const char* ratio        = "ratio";
    inline constexpr const char* compAttack   = "compAttack";
    inline constexpr const char* compRelease  = "compRelease";
    inline constexpr const char* transOn      = "transOn";
    inline constexpr const char* transAttack  = "transAttack";
    inline constexpr const char* transSustain = "transSustain";
    inline constexpr const char* maxOn        = "maxOn";
    inline constexpr const char* ceiling      = "ceiling";
    inline constexpr const char* drive        = "drive";
    inline constexpr const char* mix          = "mix";
    inline constexpr const char* oversample   = "oversample";
    inline constexpr const char* output       = "output";

    inline constexpr const char* all[] = { compOn, thresh, ratio, compAttack, compRelease,
                                           transOn, transAttack, transSustain,
                                           maxOn, ceiling, drive, mix, oversample, output };
}

inline juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float step = 0.0f)
{
    juce::NormalisableRange<float> r (lo, hi, step);
    r.setSkewForCentre (centre);
    return r;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto attr = [] (const String& suffix) { return AudioParameterFloatAttributes().withLabel (suffix); };

    auto dbText   = [] (float v, int) { return String (v, 1) + " dB"; };
    auto msText   = [] (float v, int) { return v < 100.0f ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms"; };
    auto pctText  = [] (float v, int) { return String (roundToInt (v)) + " %"; };
    auto ratText  = [] (float v, int) { return String (v, 1) + " : 1"; };

    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { ids::compOn, 1 }, "Compressor On", true));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::thresh, 1 }, "Threshold", NormalisableRange<float> (-48.0f, 0.0f, 0.1f), 0.0f,
                                                        attr ("dB").withStringFromValueFunction (dbText)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::ratio, 1 }, "Ratio", skewed (1.0f, 20.0f, 4.0f, 0.1f), 4.0f,
                                                        attr (":1").withStringFromValueFunction (ratText)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::compAttack, 1 }, "Comp Attack", skewed (0.1f, 100.0f, 10.0f, 0.1f), 10.0f,
                                                        attr ("ms").withStringFromValueFunction (msText)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::compRelease, 1 }, "Comp Release", skewed (10.0f, 1000.0f, 100.0f, 1.0f), 100.0f,
                                                        attr ("ms").withStringFromValueFunction (msText)));

    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { ids::transOn, 1 }, "Transient On", true));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::transAttack, 1 }, "Transient Attack", NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
                                                        attr ("%").withStringFromValueFunction (pctText)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::transSustain, 1 }, "Transient Sustain", NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
                                                        attr ("%").withStringFromValueFunction (pctText)));

    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { ids::maxOn, 1 }, "Maximizer On", true));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::ceiling, 1 }, "Ceiling", NormalisableRange<float> (-12.0f, 0.0f, 0.1f), 0.0f,
                                                        attr ("dB").withStringFromValueFunction (dbText)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::drive, 1 }, "Drive", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f,
                                                        attr ("%").withStringFromValueFunction (pctText)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
                                                        attr ("%").withStringFromValueFunction (pctText)));

    p.push_back (std::make_unique<AudioParameterChoice>(ParameterID { ids::oversample, 1 }, "Oversample", StringArray { "OFF", "2X", "4X" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                        attr ("dB").withStringFromValueFunction (dbText)));
    return { p.begin(), p.end() };
}
} // namespace pmx
