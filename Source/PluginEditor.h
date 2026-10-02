#pragma once
#include "PluginProcessor.h"
#include <map>

class WaveView : public juce::Component
{
public:
    explicit WaveView(SubForgeProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
private:
    SubForgeProcessor& proc;
    std::shared_ptr<const subforge::SampleData> cached;
    std::vector<float> peaks;   // min,max pairs per pixel column
    int cachedW = 0;
};

class SubForgeEditor : public juce::AudioProcessorEditor,
                       public juce::FileDragAndDropTarget,
                       private juce::Timer
{
public:
    explicit SubForgeEditor(SubForgeProcessor&);
    ~SubForgeEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;

private:
    void timerCallback() override;

    struct SFLook : juce::LookAndFeel_V4 { SFLook(); };
    struct Ctl
    {
        std::unique_ptr<juce::Component> comp;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
        bool isCombo = false, isToggle = false;
    };

    void add(const char* id);
    void grid(juce::Rectangle<int> area, std::initializer_list<const char*> ids, int cols);

    SFLook laf;   // must outlive every child component
    SubForgeProcessor& proc;
    WaveView wave;
    std::vector<std::unique_ptr<Ctl>> ctls;
    std::map<std::string, Ctl*> byId;
    juce::TextButton loadBtn { "Load Sample" }, detectBtn { "Detect Root" };
    juce::Label sampleLabel, readout, title;
    juce::ComboBox presetBox;
    juce::TextEditor songKey;
    void applySongKey();
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> panels;
    juce::String lastName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SubForgeEditor)
};
