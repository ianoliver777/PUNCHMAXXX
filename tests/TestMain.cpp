// Headless checks for PUNCHMAXXX: project recall, presets, DSP behaviour, UI render.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::cout << "  FAIL: " << msg << "\n"; } else std::cout << "  ok:   " << msg << "\n"; } while (0)

static void setParam (PunchmaxxxProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}
static float getParam (PunchmaxxxProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

// kick-ish thump + snare-ish noise burst every 250 ms, with tails
static AudioBuffer<float> makeDrums (double sr, int len, int chans = 2)
{
    AudioBuffer<float> b (chans, len);
    b.clear();
    Random rng (7);
    const int period = (int) (sr * 0.25);
    for (int start = 0; start < len; start += period)
    {
        const bool kick = (start / period) % 2 == 0;
        for (int i = 0; i < period && start + i < len; ++i)
        {
            const float t = (float) i / (float) sr;
            float s;
            if (kick) s = 0.7f * std::sin (2.0f * MathConstants<float>::pi * (55.0f + 120.0f * std::exp (-t * 40.0f)) * t) * std::exp (-t * 9.0f);
            else      s = 0.5f * (rng.nextFloat() * 2.0f - 1.0f) * std::exp (-t * 14.0f) + 0.3f * std::sin (2.0f * MathConstants<float>::pi * 190.0f * t) * std::exp (-t * 25.0f);
            for (int c = 0; c < chans; ++c) b.setSample (c, start + i, s * (c == 0 ? 1.0f : 0.92f));
        }
    }
    return b;
}

static AudioBuffer<float> run (PunchmaxxxProcessor& p, const AudioBuffer<float>& input, int block = 512)
{
    AudioBuffer<float> out (input);
    MidiBuffer midi;
    for (int pos = 0; pos < out.getNumSamples(); pos += block)
    {
        const int n = jmin (block, out.getNumSamples() - pos);
        AudioBuffer<float> view (out.getArrayOfWritePointers(), out.getNumChannels(), pos, n);
        p.processBlock (view, midi);
    }
    return out;
}

// energy in [from, to) ms after each hit, averaged over hits (channel 0), with latency compensation
static float windowRms (const AudioBuffer<float>& b, double sr, float fromMs, float toMs, int latency)
{
    const int period = (int) (sr * 0.25);
    double acc = 0; int count = 0;
    for (int start = period * 2; start + period < b.getNumSamples(); start += period)   // skip warm-up
    {
        const int a = start + latency + (int) (fromMs * 0.001 * sr), z = start + latency + (int) (toMs * 0.001 * sr);
        for (int i = a; i < z && i < b.getNumSamples(); ++i) { acc += b.getSample (0, i) * b.getSample (0, i); ++count; }
    }
    return (float) std::sqrt (acc / jmax (1, count));
}

static void resetToInit (PunchmaxxxProcessor& p) { p.presets.loadInit(); }

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir = argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory();
    outDir.createDirectory();
    const File tmpPresets = File::getSpecialLocation (File::tempDirectory).getChildFile ("pmx_test_presets");
    tmpPresets.deleteRecursively();

    const double sr = 48000.0;
    const int len = (int) (sr * 2.0);
    const auto drums = makeDrums (sr, len);

    // ------------------------------------------------------------------
    std::cout << "[1] Project recall (save/close/reopen)\n";
    {
        PunchmaxxxProcessor a;
        a.presets.setUserFolder (tmpPresets);
        a.prepareToPlay (sr, 512);
        a.presets.loadEntry (5);   // a factory preset
        const auto loadedName = a.presets.getCurrentName();
        setParam (a, pmx::ID::transient, 37.0f);
        setParam (a, pmx::ID::focus, 333.0f);
        setParam (a, pmx::ID::output, -4.5f);
        setParam (a, pmx::ID::mode, 3.0f);
        setParam (a, pmx::ID::shape, 2.0f);
        a.savedEditorWidth = 900;
        CHECK (a.presets.isModified(), "tweaking a preset marks it modified");

        MemoryBlock state;
        a.getStateInformation (state);

        PunchmaxxxProcessor b;
        b.presets.setUserFolder (tmpPresets);
        b.setStateInformation (state.getData(), (int) state.getSize());
        bool allMatch = true;
        for (auto& id : pmx::allParamIDs())
            if (std::abs (getParam (a, id.toRawUTF8()) - getParam (b, id.toRawUTF8())) > 1.0e-3f)
            {
                allMatch = false;
                std::cout << "     mismatch " << id << ": " << getParam (a, id.toRawUTF8()) << " vs " << getParam (b, id.toRawUTF8()) << "\n";
            }
        CHECK (allMatch, "all 13 parameters restored exactly");
        CHECK (b.presets.getCurrentName() == loadedName, "preset name restored (" + loadedName.toStdString() + ")");
        CHECK (b.presets.isModified(), "modified flag restored");
        CHECK (b.savedEditorWidth.load() == 900, "window size restored");

        // a clean (unmodified) preset must come back unmodified too
        a.presets.loadEntry (2);
        a.getStateInformation (state);
        PunchmaxxxProcessor c;
        c.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (! c.presets.isModified() && c.presets.getCurrentName() == a.presets.getCurrentName(), "unmodified preset reopens clean");
    }

    // ------------------------------------------------------------------
    std::cout << "[2] Presets\n";
    {
        PunchmaxxxProcessor p;
        p.presets.setUserFolder (tmpPresets);
        int factory = 0;
        for (auto& e : p.presets.getEntries()) factory += e.kind == pmx::PresetManager::Entry::Kind::Factory ? 1 : 0;
        CHECK (factory == 20, "20 factory presets (" + std::to_string (factory) + ")");

        p.presets.loadEntry (3);
        setParam (p, pmx::ID::punch, 88.0f);
        CHECK (p.presets.saveUserPreset ("My Kick"), "save new user preset");
        CHECK (! p.presets.isModified() && p.presets.isCurrentUser(), "saved preset becomes current + clean");
        setParam (p, pmx::ID::punch, 12.0f);
        setParam (p, pmx::ID::punch, 64.0f);
        CHECK (p.presets.saveUserPreset ("My Kick"), "overwrite existing user preset");
        p.presets.loadInit();
        CHECK (std::abs (getParam (p, pmx::ID::punch)) < 1.0e-3f, "INIT resets to defaults");
        int idx = -1;
        for (int i = 0; i < (int) p.presets.getEntries().size(); ++i)
            if (p.presets.getEntries()[(size_t) i].name == "My Kick") idx = i;
        CHECK (idx > 20, "user preset listed after factory ones");
        p.presets.loadEntry (idx);
        CHECK (std::abs (getParam (p, pmx::ID::punch) - 64.0f) < 0.05f, "user preset reloads overwritten value");
        p.presets.loadNext();
        p.presets.loadPrevious();
        CHECK (p.presets.getCurrentName() == "My Kick", "next/prev navigation round-trips");
        CHECK (p.presets.deleteCurrentUserPreset() && ! p.presets.userPresetExists ("My Kick"), "delete user preset");
    }

    // ------------------------------------------------------------------
    std::cout << "[3] DSP\n";
    {
        PunchmaxxxProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        const int lat = p.getLatencySamples();
        CHECK (lat > 0 && lat < 300, "reports latency to host (" + std::to_string (lat) + " samples)");

        // neutral settings should be (almost) transparent
        resetToInit (p);
        p.prepareToPlay (sr, 512);
        auto out = run (p, drums);
        // compare below ~18 kHz (the 2x oversampler's anti-alias filter rolls off the last bit of air)
        auto lp = [] (const AudioBuffer<float>& x, int i) { return 0.25f * (x.getSample (0, i) + x.getSample (0, i - 1) + x.getSample (0, i - 2) + x.getSample (0, i - 3)); };
        double err = 0, ref = 0;
        for (int i = (int) sr / 2; i < len - lat; ++i)
        {
            const float d = lp (out, i + lat) - lp (drums, i);
            err += d * d; ref += lp (drums, i) * lp (drums, i);
        }
        const double nullDb = 10.0 * std::log10 (err / ref);
        CHECK (nullDb < -40.0, "neutral settings are transparent (null " + std::to_string ((int) nullDb) + " dB)");

        const float attIn = windowRms (drums, sr, 0, 6, 0), tailIn = windowRms (drums, sr, 60, 200, 0);

        resetToInit (p); setParam (p, pmx::ID::transient, 100.0f); p.prepareToPlay (sr, 512);
        out = run (p, drums);
        const float attUp = windowRms (out, sr, 0, 6, lat);
        CHECK (attUp > attIn * 1.6f, "TRANSIENT +100 boosts the hit (" + String (Decibels::gainToDecibels (attUp / attIn), 1).toStdString() + " dB)");

        resetToInit (p); setParam (p, pmx::ID::transient, -100.0f); p.prepareToPlay (sr, 512);
        out = run (p, drums);
        const float attDown = windowRms (out, sr, 0, 6, lat);
        CHECK (attDown < attIn * 0.7f, "TRANSIENT -100 softens the hit (" + String (Decibels::gainToDecibels (attDown / attIn), 1).toStdString() + " dB)");

        resetToInit (p); setParam (p, pmx::ID::body, 100.0f); p.prepareToPlay (sr, 512);
        out = run (p, drums);
        const float tailUp = windowRms (out, sr, 60, 200, lat);
        CHECK (tailUp > tailIn * 1.4f, "BODY 100 lifts the tail (" + String (Decibels::gainToDecibels (tailUp / tailIn), 1).toStdString() + " dB)");

        resetToInit (p); setParam (p, pmx::ID::body, 0.0f); p.prepareToPlay (sr, 512);
        out = run (p, drums);
        const float tailDown = windowRms (out, sr, 60, 200, lat);
        CHECK (tailDown < tailIn * 0.5f, "BODY 0 tightens the tail (" + String (Decibels::gainToDecibels (tailDown / tailIn), 1).toStdString() + " dB)");

        resetToInit (p); setParam (p, pmx::ID::mix, 0.0f); setParam (p, pmx::ID::transient, 100.0f); setParam (p, pmx::ID::drive, 80.0f);
        p.prepareToPlay (sr, 512);
        out = run (p, drums);
        err = 0;
        ref = 0;
        for (int i = (int) sr / 2; i < len - lat; ++i) { const float d = out.getSample (0, i + lat) - drums.getSample (0, i); err += d * d; ref += drums.getSample (0, i) * drums.getSample (0, i); }
        CHECK (10.0 * std::log10 (err / ref + 1e-20) < -60.0, "MIX 0 = dry, latency-aligned");

        resetToInit (p); setParam (p, pmx::ID::output, pmx::kOutputFloorDb); p.prepareToPlay (sr, 512);
        out = run (p, drums);
        CHECK (out.getMagnitude (len / 2, len / 2) < 1.0e-6f, "OUTPUT at -inf is silent");

        bool allFinite = true; float worst = 0;
        for (int i = 1; i <= 20; ++i)
        {
            p.presets.loadEntry (i);
            p.prepareToPlay (sr, 512);
            out = run (p, drums, 333);
            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < len; ++s)
                    if (! std::isfinite (out.getSample (c, s))) allFinite = false;
            worst = jmax (worst, out.getMagnitude (0, len));
            std::cout << "     " << p.presets.getCurrentName() << ": peak " << Decibels::gainToDecibels (out.getMagnitude (0, len))
                      << " dBFS (in " << Decibels::gainToDecibels (drums.getMagnitude (0, len)) << ")\n";
        }
        CHECK (allFinite, "all 20 factory presets produce finite audio");
        CHECK (worst < 4.0f, "factory presets stay in a sane range (max peak " + String (Decibels::gainToDecibels (worst), 1).toStdString() + " dBFS)");

        // mono instance
        PunchmaxxxProcessor m;
        AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (AudioChannelSet::mono());
        mono.outputBuses.add (AudioChannelSet::mono());
        CHECK (m.setBusesLayout (mono), "mono layout supported");
        m.prepareToPlay (sr, 512);
        m.presets.loadEntry (13);
        out = run (m, makeDrums (sr, len, 1));
        CHECK (std::isfinite (out.getMagnitude (0, len)), "mono processing works");
    }

    // ------------------------------------------------------------------
    std::cout << "[4] UI render\n";
    {
        PunchmaxxxProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        std::unique_ptr<PunchmaxxxEditor> ed (dynamic_cast<PunchmaxxxEditor*> (p.createEditor()));
        CHECK (ed != nullptr, "editor opens");
        ed->renderSnapshot();
        auto snap = ed->renderSnapshot();
        PNGImageFormat png;
        auto save = [&] (const Image& img, const String& name)
        {
            FileOutputStream os (outDir.getChildFile (name));
            os.setPosition (0); os.truncate();
            png.writeImageToStream (img, os);
        };
        save (snap, "ui_init.png");

        p.presets.loadEntry (17);   // Riddim - Neck Snapper
        auto drumsLong = makeDrums (sr, (int) sr * 2);
        run (p, drumsLong);
        for (int i = 0; i < 3; ++i) { Thread::sleep (40); ed->tick(); }
        save (ed->renderSnapshot(), "ui_preset.png");
        CHECK (true, "snapshots written to " + outDir.getFullPathName().toStdString());
    }

    tmpPresets.deleteRecursively();
    std::cout << (failures == 0 ? "\nALL TESTS PASSED\n" : "\nFAILURES: " + std::to_string (failures) + "\n");
    return failures == 0 ? 0 : 1;
}
