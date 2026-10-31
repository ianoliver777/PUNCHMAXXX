#pragma once

// PUNCHMAXXX factory presets — TRIPLE 7 AUDIO
// thresh dB, ratio, comp atk ms, comp rel ms, trans attack %, trans sustain %,
// ceiling dB, drive %, mix %, comp on, trans on, max on, oversample (0=off,1=2x,2=4x), output dB

namespace pmx
{
struct FactoryPreset
{
    const char* name;
    float thresh, ratio, attack, release;
    float tAttack, tSustain;
    float ceiling, drive, mix;
    bool compOn, transOn, maxOn;
    int oversample;
    float output;
};

static const FactoryPreset factoryPresets[] =
{
    // name                     thr    ratio  atk    rel     tAtk  tSus   ceil  drive  mix   comp  trans max   os  out
    { "INIT",                   0.0f,  4.0f, 10.0f, 100.0f,  0.0f,   0.0f, 0.0f,  0.0f, 100.0f, true, true, true, 0, 0.0f },
    { "Kick Thump",           -18.0f,  4.0f, 15.0f, 120.0f, 35.0f, -10.0f, -0.3f, 30.0f, 100.0f, true, true, true, 1, 0.0f },
    { "Kick Sub Weight",      -16.0f,  3.0f, 30.0f, 200.0f, 15.0f,  20.0f, -0.3f, 25.0f, 100.0f, true, true, true, 1, 0.0f },
    { "Snare Crack",          -20.0f,  5.0f,  8.0f,  90.0f, 50.0f, -15.0f, -0.3f, 35.0f, 100.0f, true, true, true, 1, 0.0f },
    { "Snare Fat Body",       -22.0f,  3.0f, 20.0f, 150.0f, 20.0f,  35.0f, -0.3f, 25.0f, 100.0f, true, true, true, 1, 0.0f },
    { "Tight Hats",           -14.0f,  3.0f,  5.0f,  60.0f, 25.0f, -40.0f, -0.5f, 15.0f, 100.0f, true, true, true, 1, 0.0f },
    { "Clap Snap",            -18.0f,  4.0f,  3.0f,  80.0f, 45.0f, -20.0f, -0.3f, 30.0f, 100.0f, true, true, true, 1, 0.0f },
    { "Tom Boom",             -20.0f,  3.5f, 20.0f, 180.0f, 30.0f,  25.0f, -0.3f, 25.0f, 100.0f, true, true, true, 1, 0.0f },
    { "Drum Bus Glue",        -16.0f,  2.0f, 30.0f, 150.0f, 10.0f,   0.0f, -0.3f, 20.0f, 100.0f, true, true, true, 1, 0.0f },
    { "NY Parallel Smash",    -32.0f, 10.0f,  1.0f,  80.0f, 20.0f,  10.0f, -0.3f, 30.0f,  45.0f, true, true, true, 1, 0.0f },
    { "Punch Bus",            -18.0f,  4.0f, 25.0f, 100.0f, 40.0f, -10.0f, -0.3f, 35.0f, 100.0f, true, true, true, 1, 0.0f },
    { "Fat Room",             -24.0f,  6.0f, 10.0f, 250.0f,  0.0f,  45.0f, -0.3f, 25.0f,  70.0f, true, true, true, 1, 0.0f },
    { "808 Control",          -20.0f,  4.0f, 20.0f, 250.0f, 10.0f,  10.0f, -0.5f, 40.0f, 100.0f, true, true, true, 2, 0.0f },
    { "Dubstep Slam",         -24.0f,  8.0f,  2.0f,  60.0f, 50.0f, -20.0f, -0.1f, 60.0f, 100.0f, true, true, true, 2, 0.0f },
    { "Breakbeat Tighten",    -18.0f,  4.0f,  4.0f,  60.0f, 30.0f, -35.0f, -0.3f, 25.0f, 100.0f, true, true, true, 1, 0.0f },
    { "Riddim Knock",         -22.0f,  6.0f, 10.0f,  80.0f, 60.0f, -25.0f, -0.1f, 55.0f, 100.0f, true, true, true, 2, 0.0f },
    { "Lo-Fi Soft Knock",     -14.0f,  2.5f, 30.0f, 300.0f, -20.0f, 15.0f, -1.0f, 10.0f, 100.0f, true, true, true, 0, 0.0f },
    { "Acoustic Kit Natural", -14.0f,  2.0f, 25.0f, 200.0f, 15.0f,   0.0f, -0.5f, 15.0f, 100.0f, true, true, true, 1, 0.0f },
    { "Transient Snap Only",    0.0f,  4.0f, 10.0f, 100.0f, 60.0f, -10.0f, -0.3f,  0.0f, 100.0f, false, true, true, 1, 0.0f },
    { "Sustain Bloom",          0.0f,  4.0f, 10.0f, 100.0f, -10.0f, 60.0f, -0.3f, 10.0f, 100.0f, false, true, true, 1, 0.0f },
    { "Master Drums Loud",    -12.0f,  2.0f, 30.0f, 120.0f, 15.0f,   0.0f, -0.1f, 70.0f, 100.0f, true, true, true, 2, 0.0f },
    { "Into The Void",        -28.0f,  8.0f,  5.0f, 300.0f, 40.0f,  40.0f, -0.3f, 45.0f,  80.0f, true, true, true, 2, 0.0f },
};

static constexpr int numFactoryPresets = (int) (sizeof (factoryPresets) / sizeof (factoryPresets[0]));
} // namespace pmx
