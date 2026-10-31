#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/Widgets.h"

class PunchmaxxxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PunchmaxxxEditor (PunchmaxxxProcessor&);
    ~PunchmaxxxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Renders the UI at 1:1 (used by the headless test to check the layout against the art). */
    juce::Image renderSnapshot();
    /** Pumps the meters/display once (normally driven by the 30 Hz timer). */
    void tick() { timerCallback(); }

private:
    class Canvas : public juce::Component
    {
    public:
        explicit Canvas (PunchmaxxxProcessor& p);
        void paint (juce::Graphics&) override;
    private:
        PunchmaxxxProcessor& proc;
        juce::Image background;
    };

    void timerCallback() override;
    void updateBubble();
    void refreshPresetBox();
    void showPresetList();
    void showMainMenu();
    void openSaveDialog();
    void doSave (const juce::String& name);
    void confirmDelete();
    void importPreset();
    void openPresetFolder();

    PunchmaxxxProcessor& proc;
    pmx::PunchLookAndFeel lnf;
    Canvas canvas;

    std::vector<std::unique_ptr<pmx::KnobOverlay>> knobs;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> knobAttachments;
    std::unique_ptr<pmx::ChoiceGroup> modeGroup, shapeGroup;

    pmx::HotspotButton importBtn { "Load a preset file" }, folderBtn { "Open your presets folder" },
                       prevBtn { "Previous preset" }, nextBtn { "Next preset" },
                       saveBtn { "Save preset" }, initBtn { "Reset to INIT" }, menuBtn { "Menu" };
    pmx::PresetNameBox presetBox;
    pmx::ScopeDisplay scope;
    pmx::LedMeter meter;
    pmx::ValueBubble bubble;
    juce::TooltipWindow tooltips { this, 900 };

    std::unique_ptr<juce::AlertWindow> dialog;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastPresetCounter = -1;
    double lastTick = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PunchmaxxxEditor)
};
