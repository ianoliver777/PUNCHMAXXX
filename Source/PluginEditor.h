#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace pmx
{
// Look & feel: brushed-metal knobs with red value arc, glowing power icons, dark menus.
class PunchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PunchLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
};

// Invisible hit-zone over a graphic that's already in the background art.
class HitButton : public juce::Button
{
public:
    explicit HitButton (const juce::String& name, float cornerFrac = 0.25f) : juce::Button (name), corner (cornerFrac) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    float corner;
};

class PowerButton : public juce::ToggleButton
{
public:
    void paintButton (juce::Graphics&, bool over, bool down) override;
};
} // namespace pmx

//==============================================================================
class PunchmaxxxAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit PunchmaxxxAudioProcessorEditor (PunchmaxxxAudioProcessor&);
    ~PunchmaxxxAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int artW = 1774, artH = 887;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<SliderAttachment> attach;
        juce::String paramId;
        juce::Point<float> centre; float radius; float valueY;
    };

    void timerCallback() override;
    void setupKnob (Knob&, const char* id, float cx, float cy, float r, float valueY, bool bipolar = false);
    juce::Rectangle<int> toScreen (juce::Rectangle<float> art) const;
    void repaintArt (juce::Rectangle<float> art) { repaint (toScreen (art).expanded (2)); }

    void showPresetMenu();
    void showSettingsMenu();
    void showOversampleMenu();
    void savePreset();
    void saveAsPreset();
    void openPresetFile();
    void exportPresetFile();
    void deleteCurrentPreset();

    void drawGraph (juce::Graphics&);
    void drawMeter (juce::Graphics&, float x, float level, float peak);

    PunchmaxxxAudioProcessor& proc;
    pmx::PunchLookAndFeel lnf;
    juce::Image background, scaledBackground;

    std::array<Knob, 10> knobs;
    pmx::PowerButton compPower, transPower, maxPower;
    std::unique_ptr<ButtonAttachment> compPowerAtt, transPowerAtt, maxPowerAtt;
    pmx::HitButton prevBtn { "prev", 0.5f }, nextBtn { "next", 0.5f }, presetNameBtn { "preset", 0.1f },
                   saveBtn { "save" }, saveAsBtn { "saveAs" }, settingsBtn { "settings" },
                   osBtn { "oversample", 0.3f };

    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> nameDialog;

    // meters / graph state
    float inDisp = -60.0f, outDisp = -60.0f, inPeak = -60.0f, outPeak = -60.0f;
    int inPeakHold = 0, outPeakHold = 0;
    std::vector<PunchmaxxxAudioProcessor::GraphPoint> history;
    int historyPos = 0;
    juce::String lastPresetText;
    int osLast = -1;

    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PunchmaxxxAudioProcessorEditor)
};
