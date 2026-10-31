// Offline test harness for PUNCHMAXXX DSP, presets, state recall and UI render.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/FactoryPresets.h"

using namespace juce;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++failures; std::cout << "FAIL: " << msg << "\n"; } else std::cout << "ok:   " << msg << "\n"; } while (0)

static void makeDrums (AudioBuffer<float>& b, double fs, bool bandLimited = false)
{
    Random rng (7);
    b.clear();
    for (int hit = 0; hit < 16; ++hit)
    {
        const int start = (int) (hit * fs * 0.25);
        const bool kick = hit % 2 == 0;
        for (int i = 0; i < (int) (fs * 0.24) && start + i < b.getNumSamples(); ++i)
        {
            const double t = i / fs;
            float v = kick ? (float) (std::sin (2.0 * MathConstants<double>::pi * (50.0 + 120.0 * std::exp (-t * 30.0)) * t) * std::exp (-t * 9.0))
                           : (float) ((rng.nextFloat() * 2.0f - 1.0f) * std::exp (-t * 18.0) * 0.7 + std::sin (2.0 * MathConstants<double>::pi * 190.0 * t) * std::exp (-t * 25.0) * 0.4);
            if (bandLimited) // replace noise with a 12 kHz-limited tone cluster for the null test
                v = kick ? v : (float) ((std::sin (2.0 * MathConstants<double>::pi * 190.0 * t) + 0.5 * std::sin (2.0 * MathConstants<double>::pi * 3300.0 * t)
                                         + 0.3 * std::sin (2.0 * MathConstants<double>::pi * 11800.0 * t)) * std::exp (-t * 20.0) * 0.5);
            v *= 0.9f;
            b.addSample (0, start + i, v);
            b.addSample (1, start + i, v * 0.95f);
        }
    }
}

static void setParam (PunchmaxxxAudioProcessor& p, const char* id, float v)
{
    auto* prm = dynamic_cast<RangedAudioParameter*> (p.apvts.getParameter (id));
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void render (PunchmaxxxAudioProcessor& p, AudioBuffer<float>& io, int block)
{
    MidiBuffer midi;
    for (int pos = 0; pos < io.getNumSamples(); pos += block)
    {
        const int n = jmin (block, io.getNumSamples() - pos);
        AudioBuffer<float> sub (io.getArrayOfWritePointers(), 2, pos, n);
        p.processBlock (sub, midi);
    }
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const double fs = 48000.0;
    const int block = 512;

    for (int os = 0; os < 3; ++os)
    {
        // ---- 1. null / phase test: everything off -> output == input delayed by reported latency
        {
            PunchmaxxxAudioProcessor p;
            p.prepareToPlay (fs, block);
            setParam (p, pmx::ids::compOn, 0); setParam (p, pmx::ids::transOn, 0); setParam (p, pmx::ids::maxOn, 0);
            setParam (p, pmx::ids::oversample, (float) os);
            AudioBuffer<float> in (2, (int) fs * 4); makeDrums (in, fs, true);
            AudioBuffer<float> out; out.makeCopyOf (in);
            render (p, out, block);
            const int lat = p.getLatencySamples();
            double err = 0, ref = 0;
            for (int i = lat + 2000; i < in.getNumSamples(); ++i)
            {
                const double d = out.getSample (0, i) - in.getSample (0, i - lat);
                err += d * d; ref += (double) in.getSample (0, i - lat) * in.getSample (0, i - lat);
            }
            const double nullDb = 10.0 * std::log10 (err / ref + 1e-30);
            int bestLag = lat; double best = 1e30;
            for (int lg = lat - 8; lg <= lat + 8; ++lg)
            {
                double e = 0;
                for (int i = lat + 2000; i < in.getNumSamples(); ++i) { const double d = out.getSample (0, i) - in.getSample (0, i - lg); e += d * d; }
                if (e < best) { best = e; bestLag = lg; }
            }
            std::cout << "      best lag " << bestLag << " residual " << 10.0 * std::log10 (best / ref + 1e-30) << " dB\n";
            CHECK (nullDb < (os == 0 ? -200.0 : -60.0), "OS " << os << ": bypass null test (latency " << lat << " samples, residual " << String (nullDb, 1) << " dB)");
        }

        // ---- 2. every preset: finite output, ceiling respected
        for (int pi = 0; pi < pmx::numFactoryPresets; ++pi)
        {
            PunchmaxxxAudioProcessor p;
            p.prepareToPlay (fs, block);
            p.presets.loadIndex (pi);
            setParam (p, pmx::ids::oversample, (float) os);
            AudioBuffer<float> buf (2, (int) fs * 4); makeDrums (buf, fs);
            buf.applyGain (1.6f); // hot input
            render (p, buf, block);
            bool finite = true; float pk = 0;
            for (int c = 0; c < 2; ++c) for (int i = (int) (fs * 0.1); i < buf.getNumSamples(); ++i)
            { const float v = buf.getSample (c, i); finite &= std::isfinite (v); pk = jmax (pk, std::abs (v)); }
            const float ceilDb = pmx::factoryPresets[pi].ceiling;
            const bool ok = finite && pmx::gainToDb (pk) <= ceilDb + 0.01f;
            if (! ok || pi == 0 || pi == 13)
                CHECK (ok, "OS " << os << ": preset '" << pmx::factoryPresets[pi].name << "' peak " << String (pmx::gainToDb (pk), 2) << " dB (ceiling " << ceilDb << ")");
        }
    }
    std::cout << "checked " << pmx::numFactoryPresets << " factory presets x 3 oversampling modes\n";

    // ---- 3. punch test: transient attack raises crest factor
    {
        PunchmaxxxAudioProcessor p; p.prepareToPlay (fs, block);
        setParam (p, pmx::ids::compOn, 0); setParam (p, pmx::ids::maxOn, 0); setParam (p, pmx::ids::transAttack, 80);
        AudioBuffer<float> in (2, (int) fs * 4); makeDrums (in, fs);
        AudioBuffer<float> out; out.makeCopyOf (in); render (p, out, block);
        auto crest = [] (AudioBuffer<float>& b) { return b.getMagnitude (0, 0, b.getNumSamples()) / (b.getRMSLevel (0, 0, b.getNumSamples()) + 1e-9f); };
        CHECK (crest (out) > crest (in) * 1.05f, "transient ATTACK +80% increases crest factor (" << crest (in) << " -> " << crest (out) << ")");
    }

    // ---- 4. state recall (project save) incl. preset name
    {
        MemoryBlock mb;
        {
            PunchmaxxxAudioProcessor p; p.prepareToPlay (fs, block);
            p.presets.loadIndex (9);
            setParam (p, pmx::ids::drive, 77);
            p.getStateInformation (mb);
        }
        PunchmaxxxAudioProcessor q; q.prepareToPlay (fs, block);
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (q.presets.getCurrentName() == pmx::factoryPresets[9].name, "project recall restores preset name: " << q.presets.getCurrentName());
        CHECK (std::abs (q.apvts.getRawParameterValue (pmx::ids::drive)->load() - 77.0f) < 0.01f, "project recall restores tweaked params");
        CHECK (q.presets.isDirty(), "project recall restores modified (*) flag");
    }

    // ---- 5. user presets: save / list / load / delete
    {
        PunchmaxxxAudioProcessor p; p.prepareToPlay (fs, block);
        setParam (p, pmx::ids::thresh, -21.5f); setParam (p, pmx::ids::mix, 63);
        CHECK (p.presets.saveUserPreset ("ZZ Test Preset"), "save user preset");
        const int idx = p.presets.getCurrentIndex();
        CHECK (idx >= pmx::numFactoryPresets && p.presets.getNameAt (idx) == "ZZ Test Preset", "user preset appears in list");
        p.presets.loadIndex (0);
        p.presets.loadIndex (idx);
        CHECK (std::abs (p.apvts.getRawParameterValue (pmx::ids::thresh)->load() + 21.5f) < 0.01f &&
               std::abs (p.apvts.getRawParameterValue (pmx::ids::mix)->load() - 63.0f) < 0.01f, "user preset reloads values");
        CHECK (p.presets.deleteUserPreset (idx), "delete user preset");
    }

    // ---- 6. render the editor to PNG
    if (argc > 1)
    {
        PunchmaxxxAudioProcessor p; p.prepareToPlay (fs, block);
        p.presets.loadIndex (13);
        AudioBuffer<float> buf (2, block);
        AudioBuffer<float> drums (2, (int) fs * 4); makeDrums (drums, fs);
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (PunchmaxxxAudioProcessorEditor::artW, PunchmaxxxAudioProcessorEditor::artH);
        MidiBuffer midi;
        for (int pos = 0; pos + block <= drums.getNumSamples(); pos += block)
        {
            for (int c = 0; c < 2; ++c) buf.copyFrom (c, 0, drums, c, pos, block);
            p.processBlock (buf, midi);
            if ((pos / block) % 2 == 0) dynamic_cast<Timer*> (ed.get())->timerCallback();
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        File out (argv[1]);
        out.deleteFile();
        FileOutputStream os (out);
        PNGImageFormat().writeImageToStream (img, os);
        std::cout << "wrote " << out.getFullPathName() << "\n";
    }

    std::cout << (failures == 0 ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
