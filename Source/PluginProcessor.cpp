#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace subforge;

namespace
{
constexpr double kTwoPi = 6.283185307179586;

struct Preset { const char* name; float v[19]; };
const char* const presetIds[19] = { ID::oscWave, ID::oscLevel, ID::subLevel, ID::sampleLevel, ID::dropAmt, ID::dropTime,
    ID::cutoff, ID::reso, ID::fltEnv, ID::fltDecay, ID::keytrack, ID::atk, ID::dec, ID::sus, ID::rel, ID::glide,
    ID::voiceMode, ID::harm, ID::drive };
const Preset presetTable[] = {
    // Afrobeats character (production-guide research): round & warm (not trap-dirty), glide on legato, leaves room for kick
    { "Afro 808 Round",   { 0, .8f, 0, .8f, 4, 40, 1200, .05f, 0, 300, .2f, 2, 900, .3f, 160, 70, 1, .3f, .1f } },
    { "Afro Log Drum",    { 4, .8f, .2f, .5f, 5, 22, 900, .45f, 1, 160, .3f, 1, 350, .2f, 120, 0, 0, .35f, .15f } },
    { "Afro House Sub",   { 0, .9f, .3f, .5f, 0, 60, 700, 0, 0, 300, .1f, 4, 200, 1, 90, 25, 1, .25f, .05f } },
    { "808 Deep Sub",    { 0, .8f, 0, .8f, 7, 45, 1500, .05f, 0, 300, .2f, 1, 1200, .35f, 220, 80, 1, .3f, .2f } },
    { "Clean Sine Sub",  { 0, .9f, 0, .5f, 0, 60, 600, 0, 0, 300, 0, 5, 100, 1, 120, 0, 1, .15f, 0 } },
    { "Afro Log Bass",   { 1, .6f, .6f, .7f, 3, 25, 700, .2f, 2, 180, .4f, 1, 350, .6f, 90, 40, 1, .35f, .25f } },
    { "Growl Saw Sub",   { 6, .55f, .7f, .5f, 0, 60, 420, .35f, 3, 250, .5f, 2, 400, .8f, 100, 30, 1, .4f, .5f } },
    { "Warm Round Bass", { 5, .8f, .4f, .6f, 2, 30, 500, .1f, 1, 400, .3f, 3, 500, .7f, 140, 0, 0, .3f, .1f } },
};
constexpr int numPresets = (int) (sizeof(presetTable) / sizeof(presetTable[0]));

inline double polyBlep(double t, double dt)
{
    if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}

inline float oscSample(int wave, double ph, double dt)
{
    switch (wave)
    {
        case 0: return (float) std::sin(kTwoPi * ph);
        case 1: return (float) (ph < 0.25 ? 4.0 * ph : (ph < 0.75 ? 2.0 - 4.0 * ph : 4.0 * ph - 4.0));
        case 2: return (float) (2.0 * ph - 1.0 - polyBlep(ph, dt));
        case 3: { double p2 = ph + 0.5; p2 -= std::floor(p2);
                  return (float) ((ph < 0.5 ? 1.0 : -1.0) + polyBlep(ph, dt) - polyBlep(p2, dt)); }
        case 4: return (float) (std::tanh(3.0 * std::sin(kTwoPi * ph)) / std::tanh(3.0));
        case 5: { const double s = std::sin(kTwoPi * ph);
                  return (float) (0.82 * s + 0.12 * std::sin(2.0 * kTwoPi * ph + 0.6) + 0.06 * std::sin(3.0 * kTwoPi * ph)); }
        default: { double s = 0.0;   // soft saw: 8 harmonics, band-limited by construction
                   for (int k = 1; k <= 8; ++k) s += std::sin(k * kTwoPi * ph) / k;
                   return (float) (s * 0.62); }
    }
}

inline float readCubic(const std::vector<float>& d, double pos)
{
    const int n = (int) d.size();
    const int i = (int) pos;
    const float t = (float) (pos - i);
    auto at = [&](int k) { return d[(size_t) juce::jlimit(0, n - 1, k)]; };
    const float y0 = at(i - 1), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2);
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

inline float softLimit(float x)
{
    const float t = 0.85f, a = std::abs(x);
    if (a <= t) return x;
    const float o = t + (1.0f - t) * std::tanh((a - t) / (1.0f - t));
    return x < 0.0f ? -o : o;
}

// YIN-style fundamental detection tuned for sub bass (20..400 Hz)
bool detectPitch(const std::vector<float>& x, double sr, double& periodOut, float& midiOut)
{
    const int n = (int) x.size();
    const int minLag = std::max(8, (int) (sr / 400.0));
    const int maxLag = std::min((int) (sr / 20.0), n / 3);
    if (maxLag <= minLag + 4) return false;
    const int W = std::min(maxLag * 2, n - maxLag - 1);
    if (W < minLag * 3) return false;

    std::vector<float> d((size_t) maxLag + 2), dn((size_t) maxLag + 2);
    double bestScore = 1e9, bestTau = 0.0;
    for (double frac : { 0.08, 0.25, 0.45, 0.6 })
    {
        const int start = std::max(0, (int) (frac * (n - W - maxLag - 1)));
        if (start + W + maxLag >= n) continue;
        const float* a = &x[(size_t) start];
        d[0] = 0.0f;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            const float* b = a + tau; double s = 0.0;
            for (int j = 0; j < W; ++j) { const float df = a[j] - b[j]; s += (double) df * df; }
            d[(size_t) tau] = (float) s;
        }
        dn[0] = 1.0f; double run = 0.0;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            run += d[(size_t) tau];
            dn[(size_t) tau] = run > 0.0 ? (float) (d[(size_t) tau] * tau / run) : 1.0f;
        }
        int tauE = -1;
        for (int tau = minLag; tau < maxLag; ++tau)
            if (dn[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < maxLag && dn[(size_t) tau + 1] < dn[(size_t) tau]) ++tau;
                tauE = tau; break;
            }
        if (tauE < 0)
        {
            tauE = minLag;
            for (int tau = minLag; tau < maxLag; ++tau) if (dn[(size_t) tau] < dn[(size_t) tauE]) tauE = tau;
        }
        const double score = dn[(size_t) tauE];
        double t = tauE;
        if (tauE > 1 && tauE < maxLag)
        {
            const double s0 = dn[(size_t) tauE - 1], s1 = dn[(size_t) tauE], s2 = dn[(size_t) tauE + 1];
            const double den = s0 - 2.0 * s1 + s2;
            if (std::abs(den) > 1e-9) t = tauE + 0.5 * (s0 - s2) / den;
        }
        if (score < bestScore) { bestScore = score; bestTau = t; }
    }
    if (bestTau <= 0.0 || bestScore > 0.5) return false;
    periodOut = bestTau;
    midiOut = (float) (69.0 + 12.0 * std::log2((sr / bestTau) / 440.0));
    return true;
}
}

//==============================================================================
SubForgeProcessor::SubForgeProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", createLayout())
{
    formatManager.registerBasicFormats();
    held.reserve(256);
}

bool SubForgeProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void SubForgeProcessor::prepareToPlay(double rate, int)
{
    sr = rate;
    for (auto& v : voices) { v.env.setSampleRate(rate); v.env.reset(); v.active = false; v.held = false; v.f1.reset(); v.f2.reset(); }
    held.clear(); cur = -1; inputNote = -1; lastTarget = -1; bendSemis = 0.0f;
    enhLp.reset(); enhHp1.reset(); enhHp2.reset(); hpf.reset();
    enhLp.set(150.0f, 1.4142f, (float) rate);
    enhHp1.set(90.0f, 1.4142f, (float) rate);
    enhHp2.set(90.0f, 1.4142f, (float) rate);
    for (auto* s : { &smSample, &smOsc, &smSub, &smHarm, &smDrive, &smOut }) s->reset(rate, 0.02);
    smSample.setCurrentAndTargetValue(raw(ID::sampleLevel));
    smOsc.setCurrentAndTargetValue(raw(ID::oscLevel));
    smSub.setCurrentAndTargetValue(raw(ID::subLevel));
    smHarm.setCurrentAndTargetValue(raw(ID::harm));
    smDrive.setCurrentAndTargetValue(raw(ID::drive));
    smOut.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(raw(ID::outGain)));
}

MapCfg SubForgeProcessor::currentMapCfg(bool hasSample) const
{
    MapCfg c;
    c.mode = juce::roundToInt(raw(ID::playMode));
    c.key = juce::roundToInt(raw(ID::key));
    c.scale = juce::roundToInt(raw(ID::scale));
    c.home = juce::roundToInt(raw(ID::home));
    c.octave = juce::roundToInt(raw(ID::octave));
    c.semi = juce::roundToInt(raw(ID::semi));
    c.floorNote = juce::roundToInt(raw(ID::subFloor));
    c.autoOct = raw(ID::autoOct) > 0.5f;
    c.sampleRoot = hasSample ? juce::roundToInt(raw(ID::sampleRoot)) : -1;
    return c;
}

//==============================================================================
void SubForgeProcessor::killAll()
{
    held.clear();
    for (auto& v : voices) { v.env.reset(); v.active = false; v.held = false; }
    cur = -1; inputNote = -1;
}

void SubForgeProcessor::startVoice(int snd, float vel, const Blk& b)
{
    float startPitch = (float) snd;
    if (b.glideMs > 0.5f)
        startPitch = (cur >= 0 && voices[(size_t) cur].active) ? voices[(size_t) cur].note
                   : (lastTarget >= 0 ? (float) lastTarget : (float) snd);

    if (cur >= 0 && voices[(size_t) cur].active)   // fast fade of the previous voice = click-free retrigger
    {
        auto& o = voices[(size_t) cur];
        auto pr = b.adsr; pr.release = 0.006f;
        o.env.setParameters(pr);
        o.env.noteOff();
        o.held = false;
    }
    int idx = -1;
    for (int i = 0; i < (int) voices.size(); ++i) if (! voices[(size_t) i].active) { idx = i; break; }
    if (idx < 0) idx = (cur + 1) % (int) voices.size();

    auto& v = voices[(size_t) idx];
    v.active = true; v.held = true;
    v.note = startPitch; v.target = (float) snd;
    v.velGain = 1.0f - b.velSens + b.velSens * vel;
    v.dropEnv = 1.0f; v.fEnv = 1.0f; v.ctr = 0;
    v.ph = (b.wave == 2) ? 0.5 : 0.0; v.subPh = 0.0;
    v.samplePos = 0.0; v.sampleDone = false;
    v.f1.reset(); v.f2.reset();

    v.looping = false;
    if (b.sd != nullptr && b.loopMode == 1 && b.sd->data.size() > 64)
    {
        const double N = (double) b.sd->data.size();
        double ls = juce::jlimit(0.0, N - 16.0, (double) b.loopStart * N);
        double le = juce::jlimit(ls + 8.0, N - 2.0, (double) b.loopEnd * N);
        if (b.loopSnap && b.sd->period > 1.0)   // loop length = whole number of cycles -> phase-coherent sustain
        {
            double cycles = std::max(1.0, std::round((le - ls) / b.sd->period));
            double len = cycles * b.sd->period;
            while (ls + len > N - 2.0 && cycles > 1.0) { cycles -= 1.0; len = cycles * b.sd->period; }
            le = std::min(ls + len, N - 2.0);
        }
        v.loopS = ls; v.loopE = le;
        v.xf = std::min({ (double) b.loopXfMs * 0.001 * b.sd->sr, ls, (le - ls) * 0.5 });
        v.looping = (le - ls) > 8.0;
    }

    v.env.reset();
    v.env.setParameters(b.adsr);
    v.env.noteOn();
    cur = idx; lastTarget = snd;
}

void SubForgeProcessor::noteOn(int note, float vel, const Blk& b)
{
    held.erase(std::remove_if(held.begin(), held.end(), [note](const Held& h) { return h.note == note; }), held.end());
    held.push_back({ note, vel });
    inputNote = note;
    const int snd = mapFull(note, b.map);
    if (b.legato && cur >= 0 && voices[(size_t) cur].active && voices[(size_t) cur].held)
    {
        voices[(size_t) cur].target = (float) snd; lastTarget = snd;   // glide, no retrigger
        return;
    }
    startVoice(snd, vel, b);
}

void SubForgeProcessor::noteOff(int note, const Blk& b)
{
    const bool wasSounding = (note == inputNote);
    held.erase(std::remove_if(held.begin(), held.end(), [note](const Held& h) { return h.note == note; }), held.end());
    if (held.empty())
    {
        if (cur >= 0 && voices[(size_t) cur].held) { voices[(size_t) cur].env.noteOff(); voices[(size_t) cur].held = false; }
        return;
    }
    if (wasSounding)   // last-note priority: fall back to the previously held key
    {
        const Held top = held.back();
        inputNote = top.note;
        const int snd = mapFull(top.note, b.map);
        if (b.legato && cur >= 0 && voices[(size_t) cur].active && voices[(size_t) cur].held)
        { voices[(size_t) cur].target = (float) snd; lastTarget = snd; }
        else startVoice(snd, top.vel, b);
    }
}

void SubForgeProcessor::handleMidi(const juce::MidiMessage& m, const Blk& b)
{
    if (m.isNoteOn())                              noteOn(m.getNoteNumber(), m.getFloatVelocity(), b);
    else if (m.isNoteOff())                        noteOff(m.getNoteNumber(), b);
    else if (m.isPitchWheel())                     bendSemis = ((m.getPitchWheelValue() - 8192) / 8192.0f) * b.bendRange;
    else if (m.isAllNotesOff() || m.isAllSoundOff()) killAll();
}

//==============================================================================
float SubForgeProcessor::renderVoice(Voice& v, const Blk& b)
{
    v.note += (v.target - v.note) * b.glideCoef;
    const float env = v.env.getNextSample();
    if (! v.env.isActive()) { v.active = false; return 0.0f; }

    v.fEnv *= b.fEnvCoef;
    if ((v.ctr++ & 7) == 0)
    {
        const float oct = b.fltEnvAmt * v.fEnv + b.keytrack * (v.note - 60.0f) / 12.0f;
        const float fc = b.cutoff * std::exp2(oct);
        v.f1.set(fc, b.k1, (float) sr);
        v.f2.set(fc, 1.5f, (float) sr);
    }

    const float pitch = v.note + b.fineBend;
    v.dropEnv *= b.dropCoef;
    float x = 0.0f;

    // synth layers
    const float oscNote = pitch + b.dropAmt * v.dropEnv;
    const double dt = 440.0 * std::exp2((oscNote - 69.0) / 12.0) / sr;
    if (b.oLev > 0.0001f) x += oscSample(b.wave, v.ph, dt) * b.oLev;
    if (b.bLev > 0.0001f) x += (float) std::sin(kTwoPi * v.subPh) * b.bLev;
    v.ph += dt;       v.ph -= std::floor(v.ph);
    v.subPh += dt * 0.5; v.subPh -= std::floor(v.subPh);

    // sample layer
    if (b.sd != nullptr && b.sLev > 0.0001f && ! v.sampleDone)
    {
        const auto& d = b.sd->data;
        const double ratio = std::exp2((double) (pitch - b.sampleRootEff) / 12.0) * b.sd->sr / sr;
        double pos = v.samplePos;
        float s = 0.0f;
        if (v.looping)
        {
            s = readCubic(d, pos);
            if (v.xf > 0.0 && pos > v.loopE - v.xf)
            {
                const float t = (float) ((pos - (v.loopE - v.xf)) / v.xf);
                s = s * (1.0f - t) + readCubic(d, pos - (v.loopE - v.loopS)) * t;
            }
            pos += ratio;
            if (pos >= v.loopE) pos -= (v.loopE - v.loopS);
        }
        else
        {
            if (pos >= (double) d.size() - 1.0) v.sampleDone = true;
            else { s = readCubic(d, pos); pos += ratio; }
        }
        v.samplePos = pos;
        x += s * b.sLev;
    }

    // 24 dB/oct low-pass
    float lo1, hi1, lo2, hi2;
    v.f1.tick(x, lo1, hi1);
    v.f2.tick(lo1, lo2, hi2);
    return lo2 * env * v.velGain;
}

//==============================================================================
void SubForgeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    buffer.clear();

    { const juce::SpinLock::ScopedTryLockType l(sampleLock); if (l.isLocked()) audioSample = sample; }

    Blk b;
    b.sd = audioSample.get();
    b.map = currentMapCfg(b.sd != nullptr);
    b.wave = juce::roundToInt(raw(ID::oscWave));
    b.loopMode = juce::roundToInt(raw(ID::loopMode));
    b.loopSnap = raw(ID::loopSnap) > 0.5f;
    b.legato = juce::roundToInt(raw(ID::voiceMode)) == 1;
    b.dropAmt = raw(ID::dropAmt);
    b.dropCoef = (float) std::exp(-1.0 / (juce::jmax(1.0f, raw(ID::dropTime)) * 0.001 * sr));
    b.cutoff = raw(ID::cutoff);
    b.k1 = 2.0f * (1.0f - 0.95f * raw(ID::reso));
    b.fltEnvAmt = raw(ID::fltEnv);
    b.fEnvCoef = (float) std::exp(-1.0 / (juce::jmax(1.0f, raw(ID::fltDecay)) * 0.001 * sr));
    b.keytrack = raw(ID::keytrack);
    b.glideMs = raw(ID::glide);
    b.glideCoef = b.glideMs < 0.5f ? 1.0f : (float) (1.0 - std::exp(-1.0 / ((b.glideMs * 0.001 / 3.0) * sr)));
    b.sampleRootEff = raw(ID::sampleRoot) + raw(ID::sampleFine) * 0.01f;
    b.bendRange = raw(ID::bendRange);
    b.loopStart = raw(ID::loopStart); b.loopEnd = raw(ID::loopEnd); b.loopXfMs = raw(ID::loopXfade);
    b.velSens = raw(ID::velSens);
    b.adsr = { raw(ID::atk) * 0.001f, raw(ID::dec) * 0.001f, raw(ID::sus), raw(ID::rel) * 0.001f };
    const float fineBase = raw(ID::fine) * 0.01f;

    smSample.setTargetValue(raw(ID::sampleLevel));
    smOsc.setTargetValue(raw(ID::oscLevel));
    smSub.setTargetValue(raw(ID::subLevel));
    smHarm.setTargetValue(raw(ID::harm));
    smDrive.setTargetValue(raw(ID::drive));
    smOut.setTargetValue(juce::Decibels::decibelsToGain(raw(ID::outGain)));
    hpf.set(raw(ID::subHpf), 1.4142f, (float) sr);
    const bool limiterOn = raw(ID::limiter) > 0.5f;

    auto it = midi.begin();
    for (int i = 0; i < numSamples; ++i)
    {
        while (it != midi.end() && (*it).samplePosition <= i) { handleMidi((*it).getMessage(), b); ++it; }

        b.fineBend = fineBase + bendSemis;
        b.sLev = smSample.getNextValue(); b.oLev = smOsc.getNextValue(); b.bLev = smSub.getNextValue();

        float x = 0.0f;
        for (auto& v : voices) if (v.active) x += renderVoice(v, b);

        // harmonic enhancer: harmonics of the sub band only, so small speakers "hear" the bass
        const float harm = smHarm.getNextValue();
        if (harm > 0.001f)
        {
            float lo, hi, h1, h2, h3, h4;
            enhLp.tick(x, lo, hi);
            const float sh = std::tanh(3.0f * lo) + 0.6f * lo * std::abs(lo);
            enhHp1.tick(sh, h1, h2);
            enhHp2.tick(h2, h3, h4);
            x += h4 * harm * 0.35f;
        }

        // saturation (normalised tanh, blended)
        const float drv = smDrive.getNextValue();
        if (drv > 0.001f)
        {
            const float pre = 1.0f + 9.0f * drv;
            x += drv * (std::tanh(pre * x) / std::tanh(pre) - x);
        }

        // subsonic high-pass (DC + inaudible rumble), output gain, soft limiter
        float l, h;
        hpf.tick(x, l, h);
        float y = h * smOut.getNextValue();
        if (limiterOn) y = softLimit(y);

        for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer(ch)[i] = y;
    }
}

//==============================================================================
void SubForgeProcessor::setParamPlain(const char* id, float v)
{
    if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v));
}

bool SubForgeProcessor::loadSampleFile(const juce::File& f, bool detect)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData(mb)) return false;
    return loadSampleFromMemory(mb, f.getFileName(), detect);
}

bool SubForgeProcessor::loadSampleFromMemory(const juce::MemoryBlock& mb, const juce::String& name, bool detect)
{
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(std::make_unique<juce::MemoryInputStream>(mb, true)));
    if (reader == nullptr || reader->lengthInSamples < 256) return false;

    const int n = (int) std::min<juce::int64>(reader->lengthInSamples, (juce::int64) (reader->sampleRate * 30.0));
    const int nch = (int) std::max(1u, reader->numChannels);
    juce::AudioBuffer<float> tmp(nch, n);
    reader->read(&tmp, 0, n, 0, true, true);

    auto sd = std::make_shared<SampleData>();
    sd->sr = reader->sampleRate;
    sd->data.assign((size_t) n, 0.0f);
    for (int c = 0; c < nch; ++c)
    {
        const float* s = tmp.getReadPointer(c);
        for (int i = 0; i < n; ++i) sd->data[(size_t) i] += s[i] / (float) nch;
    }
    double mean = 0.0; for (float v : sd->data) mean += v;
    mean /= (double) n;
    float peak = 0.0f;
    for (auto& v : sd->data) { v -= (float) mean; peak = std::max(peak, std::abs(v)); }   // DC removal
    if (peak > 1.0e-6f) { const float g = 0.89f / peak; for (auto& v : sd->data) v *= g; }  // normalise to -1 dBFS

    double period = 0.0; float midi = -1.0f;
    if (detectPitch(sd->data, sd->sr, period, midi)) { sd->period = period; sd->midiF0 = midi; }

    {
        const juce::SpinLock::ScopedLockType l(sampleLock);
        sample = sd;
    }
    sampleName = name;
    sampleBytes = mb;
    if (detect) applyDetectedRoot();
    return true;
}

void SubForgeProcessor::applyDetectedRoot()
{
    auto sd = getSample();
    if (sd == nullptr || sd->midiF0 < 0.0f) return;
    const int root = juce::jlimit(0, 127, juce::roundToInt(sd->midiF0));
    setParamPlain(ID::sampleRoot, (float) root);
    setParamPlain(ID::sampleFine, juce::jlimit(-100.0f, 100.0f, (sd->midiF0 - (float) root) * 100.0f));
}

//==============================================================================
int SubForgeProcessor::getNumPrograms() { return numPresets; }
const juce::String SubForgeProcessor::getProgramName(int i) { return juce::isPositiveAndBelow(i, numPresets) ? presetTable[i].name : ""; }

void SubForgeProcessor::setCurrentProgram(int i)
{
    if (! juce::isPositiveAndBelow(i, numPresets)) return;
    currentProgram = i;
    for (int k = 0; k < 19; ++k) setParamPlain(presetIds[k], presetTable[i].v[k]);
}

void SubForgeProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::MemoryOutputStream out(dest, false);
    out.writeString(apvts.copyState().toXmlString());
    out.writeString(sampleName);
    out.writeInt64((juce::int64) sampleBytes.getSize());
    if (sampleBytes.getSize() > 0) out.write(sampleBytes.getData(), sampleBytes.getSize());
}

void SubForgeProcessor::setStateInformation(const void* data, int size)
{
    juce::MemoryInputStream in(data, (size_t) size, false);
    const auto xmlText = in.readString();
    const auto name = in.readString();
    const auto bytes = in.readInt64();
    if (auto xml = juce::parseXML(xmlText))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    if (bytes > 0 && bytes < (juce::int64) 512 * 1024 * 1024)
    {
        juce::MemoryBlock mb;
        in.readIntoMemoryBlock(mb, (juce::int64) bytes);
        loadSampleFromMemory(mb, name, false);   // keep saved root/loop params
    }
}

juce::AudioProcessorEditor* SubForgeProcessor::createEditor() { return new SubForgeEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SubForgeProcessor(); }
