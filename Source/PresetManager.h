#pragma once
#include <JuceHeader.h>

// Factory + user presets for PUNCHMAXXX.
// User presets are XML files (*.pmx) in:
//   Windows: %APPDATA%\TRIPLE 7 AUDIO\PUNCHMAXXX\Presets
//   macOS:   ~/Library/Application Support/TRIPLE 7 AUDIO/PUNCHMAXXX/Presets  (via userApplicationDataDirectory)
class PresetManager : private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr const char* extension = ".pmx";

    explicit PresetManager (juce::AudioProcessorValueTreeState&);
    ~PresetManager() override;

    static juce::File getUserPresetFolder();

    // the full browsable list = factory presets, then user presets (alphabetical)
    void refreshUserPresets();
    int getNumFactory() const;
    int getNumTotal() const;
    juce::String getNameAt (int index) const;
    bool isFactory (int index) const { return index < getNumFactory(); }
    const juce::Array<juce::File>& getUserFiles() const { return userFiles; }

    void loadIndex (int index);
    void loadNext();
    void loadPrevious();
    bool loadFromFile (const juce::File&);
    bool saveUserPreset (const juce::String& name);          // creates / overwrites user preset
    bool deleteUserPreset (int index);

    juce::String getCurrentName() const { return currentName; }
    int getCurrentIndex() const { return currentIndex; }
    bool isDirty() const { return dirty.load(); }
    bool currentIsUser() const { return currentIndex >= getNumFactory(); }

    // project save / recall
    void writeToState (juce::ValueTree& state) const;
    void readFromState (const juce::ValueTree& state);

private:
    void parameterChanged (const juce::String&, float) override;
    void applyValues (const std::function<float (const juce::String&, float)>& getValue);
    int findIndexForName (const juce::String&) const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::Array<juce::File> userFiles;
    juce::String currentName { "INIT" };
    int currentIndex = 0;
    std::atomic<bool> dirty { false };
    std::atomic<bool> loading { false };
};
