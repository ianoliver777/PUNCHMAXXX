#include "PresetManager.h"
#include "FactoryPresets.h"
#include "Params.h"

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s)
{
    for (auto* id : pmx::ids::all)
        apvts.addParameterListener (id, this);
    refreshUserPresets();
}

PresetManager::~PresetManager()
{
    for (auto* id : pmx::ids::all)
        apvts.removeParameterListener (id, this);
}

juce::File PresetManager::getUserPresetFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               #if JUCE_MAC
                .getChildFile ("Application Support")
               #endif
                .getChildFile ("TRIPLE 7 AUDIO").getChildFile ("PUNCHMAXXX").getChildFile ("Presets");
    if (! dir.exists()) dir.createDirectory();
    return dir;
}

void PresetManager::refreshUserPresets()
{
    userFiles = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension);
    std::sort (userFiles.begin(), userFiles.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0; });
}

int PresetManager::getNumFactory() const { return pmx::numFactoryPresets; }
int PresetManager::getNumTotal() const   { return getNumFactory() + userFiles.size(); }

juce::String PresetManager::getNameAt (int index) const
{
    if (index < 0) return {};
    if (index < getNumFactory()) return pmx::factoryPresets[index].name;
    index -= getNumFactory();
    return juce::isPositiveAndBelow (index, userFiles.size()) ? userFiles[index].getFileNameWithoutExtension() : juce::String();
}

int PresetManager::findIndexForName (const juce::String& name) const
{
    for (int i = getNumTotal(); --i >= 0;)        // user presets win over factory ones with same name
        if (getNameAt (i) == name) return i;
    return -1;
}

void PresetManager::applyValues (const std::function<float (const juce::String&, float)>& getValue)
{
    loading = true;
    for (auto* id : pmx::ids::all)
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            const float current = p->convertFrom0to1 (p->getValue());
            const float v = getValue (id, current);
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    loading = false;
    dirty = false;
}

void PresetManager::loadIndex (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumTotal())) return;

    if (index < getNumFactory())
    {
        const auto& f = pmx::factoryPresets[index];
        applyValues ([&f] (const juce::String& id, float) -> float
        {
            using namespace pmx::ids;
            if (id == compOn)       return f.compOn ? 1.0f : 0.0f;
            if (id == thresh)       return f.thresh;
            if (id == ratio)        return f.ratio;
            if (id == compAttack)   return f.attack;
            if (id == compRelease)  return f.release;
            if (id == transOn)      return f.transOn ? 1.0f : 0.0f;
            if (id == transAttack)  return f.tAttack;
            if (id == transSustain) return f.tSustain;
            if (id == maxOn)        return f.maxOn ? 1.0f : 0.0f;
            if (id == ceiling)      return f.ceiling;
            if (id == drive)        return f.drive;
            if (id == mix)          return f.mix;
            if (id == oversample)   return (float) f.oversample;
            if (id == output)       return f.output;
            return 0.0f;
        });
        currentName = f.name;
        currentIndex = index;
    }
    else
    {
        loadFromFile (userFiles[index - getNumFactory()]);
    }
}

void PresetManager::loadNext()     { loadIndex ((currentIndex + 1) % getNumTotal()); }
void PresetManager::loadPrevious() { loadIndex ((currentIndex - 1 + getNumTotal()) % getNumTotal()); }

bool PresetManager::loadFromFile (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("PUNCHMAXXX_PRESET")) return false;

    applyValues ([&xml] (const juce::String& id, float current) -> float
    {
        if (auto* e = xml->getChildByAttribute ("id", id))
            return (float) e->getDoubleAttribute ("value", current);
        return current;
    });

    // if the file lives outside the preset folder, copy it in so it shows up in the list
    auto folder = getUserPresetFolder();
    if (! file.isAChildOf (folder))
        file.copyFileTo (folder.getChildFile (file.getFileName()));

    refreshUserPresets();
    currentName = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
    const int idx = findIndexForName (file.getFileNameWithoutExtension());
    currentIndex = idx >= 0 ? idx : currentIndex;
    return true;
}

bool PresetManager::saveUserPreset (const juce::String& rawName)
{
    auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;

    juce::XmlElement xml ("PUNCHMAXXX_PRESET");
    xml.setAttribute ("name", name);
    xml.setAttribute ("author", "TRIPLE 7 AUDIO");
    xml.setAttribute ("version", 1);
    for (auto* id : pmx::ids::all)
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", id);
            e->setAttribute ("value", (double) p->convertFrom0to1 (p->getValue()));
        }

    auto file = getUserPresetFolder().getChildFile (name + extension);
    if (! xml.writeTo (file)) return false;

    refreshUserPresets();
    currentName = name;
    currentIndex = findIndexForName (name);
    dirty = false;
    return true;
}

bool PresetManager::deleteUserPreset (int index)
{
    if (index < getNumFactory() || index >= getNumTotal()) return false;
    const bool ok = userFiles[index - getNumFactory()].deleteFile();
    refreshUserPresets();
    if (ok && index == currentIndex) { currentIndex = 0; currentName = "INIT"; dirty = true; }
    return ok;
}

void PresetManager::writeToState (juce::ValueTree& state) const
{
    state.setProperty ("presetName", currentName, nullptr);
    state.setProperty ("presetDirty", dirty.load(), nullptr);
}

void PresetManager::readFromState (const juce::ValueTree& state)
{
    refreshUserPresets();
    currentName = state.getProperty ("presetName", "INIT").toString();
    const int idx = findIndexForName (currentName);
    currentIndex = idx >= 0 ? idx : 0;
    dirty = (bool) state.getProperty ("presetDirty", false);
}

void PresetManager::parameterChanged (const juce::String&, float)
{
    if (! loading) dirty = true;
}
