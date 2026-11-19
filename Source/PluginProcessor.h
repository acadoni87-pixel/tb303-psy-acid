#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

struct AcidStep
{
    int note = 2;     // 0..11 (C..B)
    int octave = 0;   // -1, 0, +1
    bool gate = true;
    bool accent = false;
    bool slide = false;
};

class TB303PsyAcidAudioProcessor : public juce::AudioProcessor
{
public:
    TB303PsyAcidAudioProcessor();
    ~TB303PsyAcidAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override { return 4; }
    int getCurrentProgram() override { return currentPresetIndex; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Sequencer & Presets API (called from UI thread safely)
    void loadPreset(int index);
    void randomizePattern(int scaleIndex, int rootNote);
    void mutatePattern(int scaleIndex, int rootNote);
    void clearPattern();

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Shared 16-step sequencer state
    std::array<AcidStep, 16> pattern;
    std::atomic<int> currentPlayStep { -1 };
    std::atomic<bool> internalSeqPlaying { false };

    // Lock-free oscilloscope ring buffer
    static constexpr int scopeSize = 512;
    std::array<float, scopeSize> scopeBuffer {};
    std::atomic<int> scopeWritePos { 0 };

private:
    // Authentic TB-303 DSP state
    double sr = 44100.0;
    double oscPhase = 0.0;
    double currentFreq = 73.42;
    double targetFreq = 73.42;

    // 3-pole/4-pole diode ladder filter states
    double fStage[4] = { 0.0, 0.0, 0.0, 0.0 };

    // Envelopes & Accent capacitor
    double filterEnv = 0.0;
    double ampEnv = 0.0;
    double accentCharge = 0.0;
    bool gateOpen = false;
    bool currentNoteAccented = false;
    int samplesUntilGateOff = 0;

    // Mono MIDI note stack for Bitwig legato slide
    struct ActiveNote { int note; int vel; };
    std::vector<ActiveNote> heldNotes;

    // Internal sequencer timing
    double seqSampleCounter = 0.0;
    int seqStepIndex = 0;
    bool prevStepSlid = false;
    int currentPresetIndex = 0;

    // Ping-Pong Psy Delay buffers
    std::vector<float> delayBufL;
    std::vector<float> delayBufR;
    int delayWriteIdx = 0;
    float dlyLpL = 0.0f, dlyHpL = 0.0f;
    float dlyLpR = 0.0f, dlyHpR = 0.0f;

    // Helpers
    static double polyBlep(double t, double dt);
    static double midiToFreq(double midiNote);
    void triggerNote(int midiNote, bool accented, bool slideLegato, int gateSamples);
    void releaseNote();
    float processDiodeFilter(float input, float cutoffHz, float resonance);
    float processDiodeDrive(float x, float driveAmount);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TB303PsyAcidAudioProcessor)
};
