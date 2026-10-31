#include "PresetManager.h"

namespace pmx
{
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    currentName = initName();
    for (auto& id : allParamIDs())
        apvts.addParameterListener (id, this);
    userFolder = defaultUserFolder();
    rescan();
}

PresetManager::~PresetManager()
{
    for (auto& id : allParamIDs())
        apvts.removeParameterListener (id, this);
}

juce::File PresetManager::defaultUserFolder()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    return base.getChildFile ("barnacle boi").getChildFile ("PUNCHMAXXX").getChildFile ("Presets");
}

void PresetManager::rescan()
{
    entries.clear();
    entries.push_back ({ Entry::Kind::Init, initName() });

    const auto& fp = factoryPresets();
    for (int i = 0; i < (int) fp.size(); ++i)
        entries.push_back ({ Entry::Kind::Factory, fp[(size_t) i].name, i });

    if (userFolder.isDirectory())
    {
        auto files = userFolder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
        for (auto& f : files)
            entries.push_back ({ Entry::Kind::User, f.getFileNameWithoutExtension(), -1, f });
    }
    ++changeCounter;
}

int PresetManager::getCurrentIndex() const
{
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].kind == currentKind && entries[(size_t) i].name == currentName)
            return i;
    return -1;
}

void PresetManager::setCurrent (Entry::Kind kind, const juce::String& name)
{
    currentKind = kind;
    currentName = name;
    modified = false;
    ++changeCounter;
}

void PresetManager::applyValues (const std::map<juce::String, float>& values)
{
    ScopedLoad sl (*this);
    for (auto& id : allParamIDs())
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            auto it = values.find (id);
            const float norm = it != values.end() ? p->convertTo0to1 (it->second) : p->getDefaultValue();
            p->beginChangeGesture();
            p->setValueNotifyingHost (norm);
            p->endChangeGesture();
        }
    }
}

void PresetManager::loadInit()
{
    applyValues ({});
    setCurrent (Entry::Kind::Init, initName());
}

void PresetManager::loadEntry (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) entries.size()))
        return;

    const auto e = entries[(size_t) index];
    switch (e.kind)
    {
        case Entry::Kind::Init:
            loadInit();
            break;

        case Entry::Kind::Factory:
            applyValues (toValueMap (factoryPresets()[(size_t) e.factoryIndex]));
            setCurrent (Entry::Kind::Factory, e.name);
            break;

        case Entry::Kind::User:
        {
            auto xml = juce::XmlDocument::parse (e.file);
            if (xml == nullptr)
                return;
            std::map<juce::String, float> values;
            for (auto* child : xml->getChildWithTagNameIterator ("PARAM"))
                values[child->getStringAttribute ("id")] = (float) child->getDoubleAttribute ("value");
            applyValues (values);
            setCurrent (Entry::Kind::User, e.name);
            break;
        }
    }
}

void PresetManager::step (int delta)
{
    const int count = (int) entries.size();
    if (count == 0) return;
    int idx = getCurrentIndex();
    if (idx < 0) idx = 0;
    loadEntry ((idx + delta + count) % count);
}

juce::String PresetManager::sanitise (const juce::String& name)
{
    return juce::File::createLegalFileName (name.trim()).substring (0, 64).trim();
}

bool PresetManager::userPresetExists (const juce::String& name) const
{
    return userFolder.getChildFile (sanitise (name) + fileExtension).existsAsFile();
}

bool PresetManager::saveUserPreset (const juce::String& rawName)
{
    const auto name = sanitise (rawName);
    if (name.isEmpty())
        return false;

    if (! userFolder.createDirectory())
        return false;

    juce::XmlElement xml ("PUNCHMAXXX_PRESET");
    xml.setAttribute ("name", name);
    xml.setAttribute ("version", 1);
    for (auto& id : allParamIDs())
    {
        auto* child = xml.createNewChildElement ("PARAM");
        child->setAttribute ("id", id);
        child->setAttribute ("value", (double) apvts.getRawParameterValue (id)->load());
    }

    const auto file = userFolder.getChildFile (name + fileExtension);
    if (! xml.writeTo (file))
        return false;

    rescan();
    setCurrent (Entry::Kind::User, name);
    return true;
}

bool PresetManager::deleteCurrentUserPreset()
{
    if (currentKind != Entry::Kind::User)
        return false;
    const auto file = userFolder.getChildFile (currentName + fileExtension);
    if (! file.deleteFile())
        return false;
    rescan();
    // keep the sound, just mark it as no longer tied to a saved preset
    currentKind = Entry::Kind::Init;
    currentName = initName();
    modified = true;
    ++changeCounter;
    return true;
}

bool PresetManager::importAndLoad (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("PUNCHMAXXX_PRESET"))
        return false;

    userFolder.createDirectory();
    const auto name = sanitise (xml->getStringAttribute ("name", file.getFileNameWithoutExtension()));
    const auto dest = userFolder.getChildFile (name + fileExtension);
    if (dest != file && ! file.copyFileTo (dest))
        return false;

    rescan();
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].kind == Entry::Kind::User && entries[(size_t) i].name == name)
        {
            loadEntry (i);
            return true;
        }
    return false;
}

void PresetManager::writeToState (juce::ValueTree& state) const
{
    state.setProperty ("presetName", currentName, nullptr);
    state.setProperty ("presetKind", (int) currentKind, nullptr);
    state.setProperty ("presetModified", modified.load(), nullptr);
}

void PresetManager::readFromState (const juce::ValueTree& state)
{
    currentName = state.getProperty ("presetName", initName()).toString();
    currentKind = (Entry::Kind) juce::jlimit (0, 2, (int) state.getProperty ("presetKind", 0));
    modified = (bool) state.getProperty ("presetModified", false);
    ++changeCounter;
}

void PresetManager::parameterChanged (const juce::String&, float)
{
    if (loadingDepth.load() == 0 && ! modified.exchange (true))
        ++changeCounter;
}
} // namespace pmx
