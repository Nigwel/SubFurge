#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "KeyMap.h"

namespace subforge
{
namespace ID
{
#define SF(n) inline constexpr const char* n = #n;
SF(sampleLevel) SF(sampleRoot) SF(sampleFine) SF(loopMode) SF(loopStart) SF(loopEnd) SF(loopXfade) SF(loopSnap)
SF(oscWave) SF(oscLevel) SF(subLevel) SF(dropAmt) SF(dropTime)
SF(key) SF(scale) SF(playMode) SF(autoOct) SF(home) SF(octave) SF(semi) SF(fine) SF(subFloor) SF(bendRange)
SF(cutoff) SF(reso) SF(fltEnv) SF(fltDecay) SF(keytrack)
SF(atk) SF(dec) SF(sus) SF(rel) SF(velSens) SF(glide) SF(voiceMode)
SF(harm) SF(drive) SF(subHpf) SF(outGain) SF(limiter)
#undef SF
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto F = [&](const char* id, const char* name, float lo, float hi, float def, float skew = 1.0f, const char* unit = "")
    {
        p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{ id, 1 }, name,
            NormalisableRange<float>(lo, hi, 0.0f, skew), def, AudioParameterFloatAttributes().withLabel(unit)));
    };
    auto I = [&](const char* id, const char* name, int lo, int hi, int def, const char* unit = "")
    {
        p.push_back(std::make_unique<AudioParameterInt>(ParameterID{ id, 1 }, name, lo, hi, def,
            AudioParameterIntAttributes().withLabel(unit)));
    };
    auto N = [&](const char* id, const char* name, int lo, int hi, int def)
    {
        p.push_back(std::make_unique<AudioParameterInt>(ParameterID{ id, 1 }, name, lo, hi, def,
            AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return noteName(v); })));
    };
    auto C = [&](const char* id, const char* name, StringArray items, int def)
    {
        p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{ id, 1 }, name, items, def));
    };
    auto B = [&](const char* id, const char* name, bool def)
    {
        p.push_back(std::make_unique<AudioParameterBool>(ParameterID{ id, 1 }, name, def));
    };

    StringArray keys, scaleNames;
    for (auto* k : keyNames) keys.add(k);
    for (int i = 0; i < numScales; ++i) scaleNames.add(scaleTable()[i].name);

    // Sample layer
    F(ID::sampleLevel, "Sample", 0.0f, 1.5f, 0.8f);
    N(ID::sampleRoot, "Sample Root", 0, 127, 36);
    F(ID::sampleFine, "Sample Fine", -100.0f, 100.0f, 0.0f, 1.0f, "ct");
    C(ID::loopMode, "Play Mode", { "One-Shot", "Loop" }, 0);
    F(ID::loopStart, "Loop Start", 0.0f, 1.0f, 0.25f);
    F(ID::loopEnd, "Loop End", 0.0f, 1.0f, 0.9f);
    F(ID::loopXfade, "Loop Xfade", 1.0f, 300.0f, 30.0f, 0.5f, "ms");
    B(ID::loopSnap, "Cycle Snap", true);

    // Oscillator layer
    C(ID::oscWave, "Waveform", { "Sine", "Triangle", "Saw", "Square", "Soft Square", "Warm Sine", "Soft Saw" }, 0);
    F(ID::oscLevel, "Osc", 0.0f, 1.5f, 0.7f);
    F(ID::subLevel, "Sub -1 Oct", 0.0f, 1.5f, 0.0f);
    F(ID::dropAmt, "Pitch Drop", 0.0f, 24.0f, 0.0f, 1.0f, "st");
    F(ID::dropTime, "Drop Time", 5.0f, 400.0f, 60.0f, 0.5f, "ms");

    // Key / scale / octave
    C(ID::key, "Key", keys, 0);
    C(ID::scale, "Scale", scaleNames, 0);
    C(ID::playMode, "Key Mode", { "Free", "Key Home", "Scale Snap", "Degree Map" }, 2);
    B(ID::autoOct, "Auto Octave", true);
    N(ID::home, "Home Key", 24, 48, 36);
    I(ID::octave, "Octave", -3, 3, 0);
    I(ID::semi, "Semitone", -12, 12, 0, "st");
    F(ID::fine, "Fine", -100.0f, 100.0f, 0.0f, 1.0f, "ct");
    N(ID::subFloor, "Sub Floor", 12, 36, 24);
    F(ID::bendRange, "Bend Range", 0.0f, 12.0f, 2.0f, 1.0f, "st");

    // Filter
    F(ID::cutoff, "Cutoff", 20.0f, 20000.0f, 1800.0f, 0.3f, "Hz");
    F(ID::reso, "Resonance", 0.0f, 0.95f, 0.1f);
    F(ID::fltEnv, "Env Amount", -4.0f, 4.0f, 0.0f, 1.0f, "oct");
    F(ID::fltDecay, "Env Decay", 10.0f, 2000.0f, 300.0f, 0.5f, "ms");
    F(ID::keytrack, "Key Track", 0.0f, 1.0f, 0.3f);

    // Amp
    F(ID::atk, "Attack", 0.5f, 2000.0f, 2.0f, 0.4f, "ms");
    F(ID::dec, "Decay", 5.0f, 5000.0f, 600.0f, 0.4f, "ms");
    F(ID::sus, "Sustain", 0.0f, 1.0f, 0.85f);
    F(ID::rel, "Release", 5.0f, 5000.0f, 120.0f, 0.4f, "ms");
    F(ID::velSens, "Velocity", 0.0f, 1.0f, 0.5f);
    F(ID::glide, "Glide", 0.0f, 1000.0f, 0.0f, 0.5f, "ms");
    C(ID::voiceMode, "Voice", { "Mono", "Legato" }, 1);

    // FX / output
    F(ID::harm, "Harmonics", 0.0f, 1.0f, 0.25f);
    F(ID::drive, "Drive", 0.0f, 1.0f, 0.15f);
    F(ID::subHpf, "Sub HPF", 15.0f, 40.0f, 20.0f, 1.0f, "Hz");
    F(ID::outGain, "Output", -24.0f, 6.0f, -3.0f, 1.0f, "dB");
    B(ID::limiter, "Limiter", true);

    return { p.begin(), p.end() };
}
}
