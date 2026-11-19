#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PsyLookAndFeel.h"

class TB303PsyAcidAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit TB303PsyAcidAudioProcessorEditor(TB303PsyAcidAudioProcessor&);
    ~TB303PsyAcidAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshStepButtons();
    void drawSacredGeometry(juce::Graphics& g, juce::Rectangle<float> area);
    void drawOscilloscope(juce::Graphics& g, juce::Rectangle<float> area);

    TB303PsyAcidAudioProcessor& audioProcessor;
    PsyLookAndFeel psyLaf;

    struct KnobControl
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    std::array<KnobControl, 11> knobs;

    juce::ComboBox waveBox, presetBox, scaleBox, rootBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAtt;

    juce::ToggleButton seqToggle { "INT SEQ" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> seqAtt;

    juce::Slider bpmSlider;
    juce::Label bpmLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAtt;

    juce::TextButton randBtn { "ACID RANDOMIZE" };
    juce::TextButton mutateBtn { "MUTATE" };
    juce::TextButton clearBtn { "CLEAR" };

    // 16-Step Sequencer UI columns
    struct StepUI
    {
        juce::TextButton noteBtn;
        juce::TextButton octBtn;
        juce::TextButton gateBtn;
        juce::TextButton accBtn;
        juce::TextButton sldBtn;
    };
    std::array<StepUI, 16> stepUIs;

    juce::Rectangle<float> scopeBounds;
    float animPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TB303PsyAcidAudioProcessorEditor)
};
