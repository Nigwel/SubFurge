#include "PluginEditor.h"

using namespace subforge;

namespace
{
const juce::Colour kBg(0xff0e1116), kPanel(0xff171c24), kAccent(0xffff9f1c), kDim(0xff8a94a3);
}

//==============================================================================
void WaveView::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff0b0e12));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);

    auto sd = proc.getSample();
    if (sd == nullptr)
    {
        g.setColour(kDim); g.setFont(13.0f);
        g.drawText("Drop a sub bass sample here  (WAV / AIFF / FLAC)", getLocalBounds(), juce::Justification::centred);
        return;
    }
    const int w = getWidth(), h = getHeight();
    if (sd != cached || cachedW != w)
    {
        cached = sd; cachedW = w;
        peaks.assign((size_t) w * 2, 0.0f);
        const auto n = sd->data.size();
        for (int x = 0; x < w; ++x)
        {
            const size_t a = (size_t) x * n / (size_t) w, e = std::max(a + 1, (size_t) (x + 1) * n / (size_t) w);
            float lo = 0, hi = 0;
            for (size_t i = a; i < e && i < n; ++i) { lo = std::min(lo, sd->data[i]); hi = std::max(hi, sd->data[i]); }
            peaks[(size_t) x * 2] = lo; peaks[(size_t) x * 2 + 1] = hi;
        }
    }
    const float mid = h * 0.5f, half = h * 0.45f;
    g.setColour(kAccent.withAlpha(0.85f));
    for (int x = 0; x < w; ++x)
        g.drawVerticalLine(x, mid - peaks[(size_t) x * 2 + 1] * half, mid - peaks[(size_t) x * 2] * half + 1.0f);

    if (juce::roundToInt(proc.raw(ID::loopMode)) == 1)
    {
        const float x1 = proc.raw(ID::loopStart) * w, x2 = proc.raw(ID::loopEnd) * w;
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.fillRect(juce::Rectangle<float>(x1, 0.0f, std::max(1.0f, x2 - x1), (float) h));
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawVerticalLine((int) x1, 0.0f, (float) h);
        g.drawVerticalLine((int) x2, 0.0f, (float) h);
    }
}

//==============================================================================
SubForgeEditor::SFLook::SFLook()
{
    setColour(juce::ResizableWindow::backgroundColourId, kBg);
    setColour(juce::Slider::rotarySliderFillColourId, kAccent);
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2a313c));
    setColour(juce::Slider::thumbColourId, juce::Colours::white);
    setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff222a35));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2a313c));
    setColour(juce::ComboBox::textColourId, juce::Colours::white);
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a313c));
    setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    setColour(juce::ToggleButton::tickColourId, kAccent);
    setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1b222c));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccent.darker(0.4f));
}

void SubForgeEditor::add(const char* id)
{
    auto c = std::make_unique<Ctl>();
    auto* param = proc.apvts.getParameter(id);
    const juce::String text = param->getName(32);

    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*>(param))
    {
        auto cb = std::make_unique<juce::ComboBox>();
        cb->addItemList(ch->choices, 1);
        c->ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, id, *cb);
        c->comp = std::move(cb); c->isCombo = true;
    }
    else if (dynamic_cast<juce::AudioParameterBool*>(param) != nullptr)
    {
        auto tb = std::make_unique<juce::ToggleButton>(text);
        c->ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, id, *tb);
        c->comp = std::move(tb); c->isToggle = true;
    }
    else
    {
        auto s = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 16);
        c->sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id, *s);
        c->comp = std::move(s);
    }
    addAndMakeVisible(*c->comp);
    if (! c->isToggle)
    {
        c->label = std::make_unique<juce::Label>(juce::String(), text);
        c->label->setJustificationType(juce::Justification::centred);
        c->label->setFont(juce::FontOptions(12.0f));
        c->label->setColour(juce::Label::textColourId, kDim);
        addAndMakeVisible(*c->label);
    }
    byId[id] = c.get();
    ctls.push_back(std::move(c));
}

SubForgeEditor::SubForgeEditor(SubForgeProcessor& p)
    : AudioProcessorEditor(&p), proc(p), wave(p)
{
    setLookAndFeel(&laf);
    for (auto* id : { ID::sampleLevel, ID::sampleRoot, ID::sampleFine, ID::loopMode, ID::loopStart, ID::loopEnd, ID::loopXfade, ID::loopSnap,
                      ID::oscWave, ID::oscLevel, ID::subLevel, ID::dropAmt, ID::dropTime,
                      ID::key, ID::scale, ID::playMode, ID::autoOct, ID::home, ID::octave, ID::semi, ID::fine, ID::subFloor, ID::bendRange,
                      ID::cutoff, ID::reso, ID::fltEnv, ID::fltDecay, ID::keytrack,
                      ID::atk, ID::dec, ID::sus, ID::rel, ID::velSens, ID::glide, ID::voiceMode,
                      ID::harm, ID::drive, ID::subHpf, ID::outGain, ID::limiter })
        add(id);

    addAndMakeVisible(wave);
    addAndMakeVisible(loadBtn); addAndMakeVisible(detectBtn);
    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>("Select a sub bass sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) { const auto f = fc.getResult(); if (f.existsAsFile()) proc.loadSampleFile(f, true); });
    };
    detectBtn.onClick = [this] { proc.applyDetectedRoot(); };

    sampleLabel.setColour(juce::Label::textColourId, kDim); addAndMakeVisible(sampleLabel);
    readout.setColour(juce::Label::textColourId, kAccent); readout.setJustificationType(juce::Justification::centredRight);
    readout.setFont(juce::FontOptions(13.0f)); addAndMakeVisible(readout);
    title.setText("SUBFORGE", juce::dontSendNotification);
    title.setFont(juce::FontOptions(26.0f, juce::Font::bold)); title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem(proc.getProgramName(i), i + 1);
    presetBox.setTextWhenNothingSelected("Presets");
    presetBox.onChange = [this] { proc.setCurrentProgram(presetBox.getSelectedItemIndex()); };
    addAndMakeVisible(presetBox);

    songKey.setTextToShowWhenEmpty("Song key: Eb Min9", kDim);
    songKey.setTooltip("Type the song key or chord, e.g. Eb Min9, F#m7, Bb major, G dorian, then press Enter");
    songKey.setFont(juce::FontOptions(14.0f));
    songKey.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff222a35));
    songKey.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    songKey.setColour(juce::TextEditor::outlineColourId, kAccent);
    songKey.onReturnKey = [this] { applySongKey(); };
    songKey.onFocusLost = [this] { applySongKey(); };
    addAndMakeVisible(songKey);

    setSize(940, 700);
    startTimerHz(10);
}

SubForgeEditor::~SubForgeEditor() { setLookAndFeel(nullptr); }

bool SubForgeEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File(f).hasFileExtension("wav;aif;aiff;flac;ogg")) return true;
    return false;
}

void SubForgeEditor::filesDropped(const juce::StringArray& files, int, int)
{
    if (files.size() > 0) proc.loadSampleFile(juce::File(files[0]), true);
}

void SubForgeEditor::applySongKey()
{
    const auto text = songKey.getText();
    if (text.isEmpty()) return;
    const auto k = parseKeySymbol(text);
    if (! k.ok) { songKey.setColour(juce::TextEditor::textColourId, juce::Colours::salmon); songKey.repaint(); return; }
    songKey.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    for (auto [id, v] : { std::pair<const char*, float>{ ID::key, (float) k.key }, { ID::scale, (float) k.scale } })
        if (auto* p = proc.apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v));
    if (auto* pm = proc.apvts.getParameter(ID::autoOct)) pm->setValueNotifyingHost(1.0f);
    songKey.setText(juce::String(keyNames[k.key]) + " " + scaleTable()[k.scale].name, juce::dontSendNotification);
}

void SubForgeEditor::timerCallback()
{
    const auto c = proc.currentMapCfg(proc.getSample() != nullptr);
    const int snd = proc.mappedHome();
    auto hz = [](double n) { return 440.0 * std::pow(2.0, (n - 69.0) / 12.0); };
    juce::String s = noteName(c.home) + " plays " + noteName(snd) + "  " + juce::String(hz(snd), 1) + " Hz";
    readout.setText(s, juce::dontSendNotification);

    const auto name = proc.getSampleName();
    if (name != lastName)
    {
        lastName = name;
        auto sd = proc.getSample();
        juce::String t = name.isEmpty() ? "No sample loaded" : name;
        if (sd != nullptr)
            t += sd->midiF0 >= 0.0f ? "   |   detected " + noteName(juce::roundToInt(sd->midiF0)) + "  (" + juce::String(hz(sd->midiF0), 1) + " Hz)"
                                    : "   |   pitch not detected - set Sample Root manually";
        sampleLabel.setText(t, juce::dontSendNotification);
    }
    wave.repaint();
}

void SubForgeEditor::grid(juce::Rectangle<int> area, std::initializer_list<const char*> ids, int cols)
{
    const int n = (int) ids.size(), rows = (n + cols - 1) / cols;
    const int cw = area.getWidth() / cols, ch = area.getHeight() / rows;
    int i = 0;
    for (auto* id : ids)
    {
        auto cell = juce::Rectangle<int>(area.getX() + (i % cols) * cw, area.getY() + (i / cols) * ch, cw, ch).reduced(2);
        auto& c = *byId[id];
        if (c.isToggle) c.comp->setBounds(cell.withSizeKeepingCentre(cw - 8, 24));
        else
        {
            c.label->setBounds(cell.removeFromTop(14));
            if (c.isCombo) c.comp->setBounds(cell.removeFromTop(26).reduced(4, 0));
            else c.comp->setBounds(cell);
        }
        ++i;
    }
}

void SubForgeEditor::resized()
{
    panels.clear();
    auto r = getLocalBounds().reduced(10);
    auto header = r.removeFromTop(42);
    title.setBounds(header.removeFromLeft(150));
    presetBox.setBounds(header.removeFromLeft(150).reduced(0, 8)); header.removeFromLeft(8);
    songKey.setBounds(header.removeFromLeft(190).reduced(0, 6)); header.removeFromLeft(8);
    readout.setBounds(header);
    r.removeFromTop(4);

    const int rowH = 200, gap = 8, leftW = 600;
    auto rowA = r.removeFromTop(rowH); r.removeFromTop(gap);
    auto rowB = r.removeFromTop(rowH); r.removeFromTop(gap);
    auto rowC = r.removeFromTop(rowH);

    auto split = [&](juce::Rectangle<int>& row, const juce::String& l, const juce::String& rt)
    {
        auto left = row.removeFromLeft(leftW); row.removeFromLeft(gap);
        panels.push_back({ l, left }); panels.push_back({ rt, row });
        return std::make_pair(left.reduced(8).withTrimmedTop(16), row.reduced(8).withTrimmedTop(16));
    };

    auto a = split(rowA, "SAMPLE", "OSCILLATOR");
    auto top = a.first.removeFromTop(24);
    loadBtn.setBounds(top.removeFromLeft(100)); top.removeFromLeft(6);
    detectBtn.setBounds(top.removeFromLeft(100)); top.removeFromLeft(8);
    sampleLabel.setBounds(top);
    a.first.removeFromTop(4);
    wave.setBounds(a.first.removeFromTop(52)); a.first.removeFromTop(2);
    grid(a.first, { ID::sampleLevel, ID::sampleRoot, ID::sampleFine, ID::loopMode, ID::loopStart, ID::loopEnd, ID::loopXfade, ID::loopSnap }, 8);
    grid(a.second, { ID::oscWave, ID::oscLevel, ID::subLevel, ID::dropAmt, ID::dropTime }, 3);

    auto b = split(rowB, "KEY  /  SCALE  /  OCTAVE  (Home key plays the tonic in its best octave)", "FILTER");
    grid(b.first, { ID::key, ID::scale, ID::playMode, ID::autoOct, ID::home, ID::octave, ID::semi, ID::fine, ID::subFloor, ID::bendRange }, 5);
    grid(b.second, { ID::cutoff, ID::reso, ID::fltEnv, ID::fltDecay, ID::keytrack }, 3);

    auto c = split(rowC, "AMP  /  PLAY", "BASS FX  /  OUTPUT");
    grid(c.first, { ID::atk, ID::dec, ID::sus, ID::rel, ID::velSens, ID::glide, ID::voiceMode }, 4);
    grid(c.second, { ID::harm, ID::drive, ID::subHpf, ID::outGain, ID::limiter }, 3);
}

void SubForgeEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
    for (auto& p : panels)
    {
        g.setColour(kPanel); g.fillRoundedRectangle(p.second.toFloat(), 6.0f);
        g.setColour(kAccent); g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(p.first, p.second.reduced(10, 4).removeFromTop(16), juce::Justification::centredLeft);
    }
}
