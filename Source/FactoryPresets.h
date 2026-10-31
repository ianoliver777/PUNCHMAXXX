#pragma once
#include "Parameters.h"

namespace pmx
{
struct FactoryPreset
{
    const char* name;
    float transient, punch, body;
    Mode mode;
    Shape shape;
    float focus, drive, saturation, low, high, width, mix, output;
};

// 20 stock presets. Width 50 = untouched stereo, Body 50 = untouched tail.
inline const std::vector<FactoryPreset>& factoryPresets()
{
    using M = Mode; using S = Shape;
    static const std::vector<FactoryPreset> p {
        //  name                          trans punch body  mode      shape      focus drive sat   low   high  width mix   out
        { "Kick - Knock Hard",            45,   70,   40,  M::Punch, S::Hard,    90,  20,  15,   2.0,  0.0, 50, 100,  -3.5 },
        { "Kick - Sub Boom",              15,   30,   75,  M::Fat,   S::Smooth,  60,  10,  20,   3.0, -1.0, 50, 100,  -2.5 },
        { "Kick - Tight Click",           50,   40,   25,  M::Tight, S::Hard,   120,   0,  10,   0.0,  3.0, 50, 100,  -9.5 },
        { "Snare - Crack",                55,   35,   45,  M::Tight, S::Hard,   250,  15,  10,   0.0,  2.0, 50, 100,  -6.5 },
        { "Snare - Fat Body",             20,   50,   80,  M::Fat,   S::Smooth, 220,  10,  25,   1.5,  0.0, 55, 100,  -2.5 },
        { "Snare - Room Killer",          25,   20,   10,  M::Tight, S::Soft,   250,   0,   0,   0.0,  1.0, 50, 100,   -1.5 },
        { "Hats - Crisp",                 40,    0,   30,  M::Tight, S::Exp,    600,   0,   5,  -2.0,  2.5, 60, 100,   -6.0 },
        { "Hats - Soften",               -40,    0,   55,  M::Clean, S::Soft,   400,   0,  10,   0.0, -2.0, 50, 100,    2.5 },
        { "Toms - Thunder",               30,   55,   75,  M::Fat,   S::Smooth, 120,  15,  20,   2.0,  0.0, 55, 100,  -2.0 },
        { "Claps - Slap",                 50,   25,   35,  M::Punch, S::Hard,   300,  10,  15,   0.0,  2.0, 60, 100,  -7.0 },
        { "Perc - Pop",                   45,   20,   40,  M::Punch, S::Exp,    350,   5,  10,   0.0,  1.5, 65, 100,  -5.5 },
        { "Drum Bus - Glue Punch",        25,   35,   55,  M::Punch, S::Smooth, 150,  20,  25,   1.0,  1.0, 55,  70,  -0.5 },
        { "Drum Bus - Slam",              50,   60,   60,  M::Punch, S::Hard,   140,  55,  40,   1.5,  1.5, 55, 100,  -1.0 },
        { "Drum Bus - Clean Lift",        20,   15,   50,  M::Clean, S::Soft,   200,   0,   0,   0.0,  1.0, 50, 100,  -1.5 },
        { "Breaks - Dusty Smack",         40,   45,   45,  M::Punch, S::Hard,   180,  35,  55,  -1.0, -3.0, 45, 100,   3.0 },
        { "808 - Snap",                   35,   50,   65,  M::Punch, S::Smooth,  70,  30,  35,   2.0,  0.0, 40, 100,   0.0 },
        { "Riddim - Neck Snapper",        70,   75,   35,  M::Tight, S::Hard,   160,  45,  30,   1.0,  2.0, 55, 100,  -2.5 },
        { "Lo-Fi - Squash",              -30,   10,   80,  M::Fat,   S::Soft,   250,  40,  70,   1.0, -5.0, 35, 100,   4.5 },
        { "Parallel - NY Smash",          60,   60,   70,  M::Fat,   S::Hard,   150,  70,  50,   2.0,  2.0, 60,  45,   1.0 },
        { "Bass - Pluck Tamer",          -35,    0,   45,  M::Clean, S::Soft,   100,   0,  10,   0.0, -1.0, 50, 100,    3.0 },
    };
    return p;
}

inline std::map<juce::String, float> toValueMap (const FactoryPreset& f)
{
    return { { ID::transient, f.transient }, { ID::punch, f.punch }, { ID::body, f.body },
             { ID::mode, (float) (int) f.mode }, { ID::shape, (float) (int) f.shape },
             { ID::focus, f.focus }, { ID::drive, f.drive }, { ID::saturation, f.saturation },
             { ID::toneLow, f.low }, { ID::toneHigh, f.high }, { ID::width, f.width },
             { ID::mix, f.mix }, { ID::output, f.output } };
}
} // namespace pmx
