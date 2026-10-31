#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "FactoryPresets.h"

namespace pmx
{
/** INIT + 20 factory presets + user presets (.pmxx files on disk).
    Tracks which preset is loaded and whether it's been tweaked since. */
class PresetManager : private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr const char* fileExtension = ".pmxx";
    static juce::String initName() { return juce::String::fromUTF8 ("INIT \xe2\x80\x93 Default"); }
    static juce::String displayName (const juce::String& n) { return n.replace (" - ", juce::String::fromUTF8 (" \xe2\x80\x93 ")); }

    struct Entry
    {
        enum class Kind { Init, Factory, User } kind;
        juce::String name;
        int factoryIndex = -1;
        juce::File file;
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState& state);
    ~PresetManager() override;

    void setUserFolder (const juce::File& f) { userFolder = f; rescan(); }
    juce::File getUserFolder() const { return userFolder; }
    static juce::File defaultUserFolder();

    void rescan();
    const std::vector<Entry>& getEntries() const { return entries; }

    int getCurrentIndex() const;
    juce::String getCurrentName() const { return currentName; }
    bool isCurrentUser() const { return currentKind == Entry::Kind::User; }
    bool isCurrentFactory() const { return currentKind == Entry::Kind::Factory; }
    bool isModified() const { return modified.load(); }

    void loadEntry (int index);
    void loadInit();
    void loadNext() { step (1); }
    void loadPrevious() { step (-1); }

    /** Saves the current settings as a user preset (overwrites if the name exists). */
    bool saveUserPreset (const juce::String& name);
    bool userPresetExists (const juce::String& name) const;
    bool deleteCurrentUserPreset();
    /** Imports a .pmxx file from anywhere: copies it into the user folder and loads it. */
    bool importAndLoad (const juce::File& file);

    /** Store/restore which preset is active inside the plugin state (for project recall). */
    void writeToState (juce::ValueTree& state) const;
    void readFromState (const juce::ValueTree& state);

    /** Wrap bulk parameter changes (state restore, preset load) so they don't flag "modified". */
    struct ScopedLoad
    {
        explicit ScopedLoad (PresetManager& m) : pm (m) { ++pm.loadingDepth; }
        ~ScopedLoad() { --pm.loadingDepth; }
        PresetManager& pm;
    };

    /** Bumps whenever the current preset / modified flag / list changes (cheap UI polling). */
    int getChangeCounter() const { return changeCounter.load(); }

private:
    void parameterChanged (const juce::String&, float) override;
    void applyValues (const std::map<juce::String, float>& values);
    void step (int delta);
    void setCurrent (Entry::Kind kind, const juce::String& name);
    static juce::String sanitise (const juce::String& name);

    juce::AudioProcessorValueTreeState& apvts;
    juce::File userFolder;
    std::vector<Entry> entries;
    juce::String currentName;
    Entry::Kind currentKind = Entry::Kind::Init;
    std::atomic<bool> modified { false };
    std::atomic<int> loadingDepth { 0 };
    std::atomic<int> changeCounter { 0 };
};
} // namespace pmx
