#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"

namespace subforge
{
struct SampleData
{
    std::vector<float> data;      // mono, DC-removed, peak-normalised
    double sr = 44100.0;
    double period = 0.0;          // detected cycle length in samples (0 = unknown)
    float midiF0 = -1.0f;         // detected fundamental as fractional MIDI note (<0 = failed)
};

struct SVF   // TPT state-variable filter
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1.4142f;
    void set(float fc, float q, float sr)
    {
        fc = juce::jlimit(5.0f, sr * 0.45f, fc);
        const float g = std::tan(juce::MathConstants<float>::pi * fc / sr);
        k = q; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline void tick(float v0, float& low, float& high)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        low = v2; high = v0 - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0.0f; }
};

struct Voice
{
    bool active = false, held = false, looping = false, sampleDone = false;
    float note = 36.0f, target = 36.0f, velGain = 1.0f, dropEnv = 0.0f, fEnv = 0.0f;
    double samplePos = 0.0, loopS = 0.0, loopE = 0.0, xf = 0.0, ph = 0.0, subPh = 0.0;
    int ctr = 0;
    juce::ADSR env;
    SVF f1, f2;
};

struct Held { int note; float vel; };

struct Blk   // per-block snapshot of parameters
{
    const SampleData* sd = nullptr;
    int wave = 0, loopMode = 0;
    bool loopSnap = true, legato = true;
    float sLev = 0, oLev = 0, bLev = 0;
    float dropAmt = 0, dropCoef = 0, cutoff = 1000, k1 = 2, fltEnvAmt = 0, fEnvCoef = 0, keytrack = 0;
    float glideCoef = 1, glideMs = 0, fineBend = 0, sampleRootEff = 36, bendRange = 2;
    float loopStart = 0, loopEnd = 1, loopXfMs = 30, velSens = 0.5f;
    juce::ADSR::Parameters adsr;
    MapCfg map;
};
}

class SubForgeProcessor : public juce::AudioProcessor
{
public:
    SubForgeProcessor();
    ~SubForgeProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SubForge"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    bool loadSampleFile(const juce::File&, bool detect);
    bool loadSampleFromMemory(const juce::MemoryBlock&, const juce::String& name, bool detect);
    void applyDetectedRoot();
    std::shared_ptr<const subforge::SampleData> getSample() const
    {
        const juce::SpinLock::ScopedLockType l(sampleLock);
        return sample;
    }
    juce::String getSampleName() const { return sampleName; }

    subforge::MapCfg currentMapCfg(bool hasSample) const;
    int mappedHome() const { const auto c = currentMapCfg(getSample() != nullptr); return subforge::mapFull(c.home, c); }
    float raw(const char* id) const { return apvts.getRawParameterValue(id)->load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    using Blk = subforge::Blk;
    using Voice = subforge::Voice;

    void setParamPlain(const char* id, float v);
    float renderVoice(Voice&, const Blk&);
    void handleMidi(const juce::MidiMessage&, const Blk&);
    void noteOn(int note, float vel, const Blk&);
    void noteOff(int note, const Blk&);
    void startVoice(int snd, float vel, const Blk&);
    void killAll();

    juce::AudioFormatManager formatManager;
    mutable juce::SpinLock sampleLock;
    std::shared_ptr<const subforge::SampleData> sample, audioSample;
    juce::MemoryBlock sampleBytes;
    juce::String sampleName;

    std::array<Voice, 4> voices;
    std::vector<subforge::Held> held;
    int cur = -1, inputNote = -1, lastTarget = -1, currentProgram = 0;
    float bendSemis = 0.0f;
    double sr = 44100.0;

    subforge::SVF enhLp, enhHp1, enhHp2, hpf;
    juce::SmoothedValue<float> smSample, smOsc, smSub, smHarm, smDrive, smOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubForgeProcessor)
};
