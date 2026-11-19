#include "PluginEditor.h"

namespace
{
    const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    struct KnobSpec { const char* id; const char* title; bool isFx; };
    const KnobSpec kKnobSpecs[11] = {
        { "tuning",    "TUNING",    false },
        { "cutoff",    "CUT OFF",   false },
        { "resonance", "RESONANCE", false },
        { "envMod",    "ENV MOD",   false },
        { "decay",     "DECAY",     false },
        { "accent",    "ACCENT",    false },
        { "drive",     "OVERDRIVE", true  },
        { "delayMix",  "PSY DLY",   true  },
        { "delayFb",   "DLY FDBK",  true  },
        { "delaySync", "DLY SYNC",  true  },
        { "volume",    "MASTER",    true  }
    };
}

TB303PsyAcidAudioProcessorEditor::TB303PsyAcidAudioProcessorEditor(TB303PsyAcidAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&psyLaf);
    setSize(1060, 660);

    // 11 Rotary Knobs
    for (int i = 0; i < 11; ++i)
    {
        auto& k = knobs[(size_t)i];
        k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        k.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffaaff00));
        k.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.slider.getProperties().set("isFx", kKnobSpecs[i].isFx);
        addAndMakeVisible(k.slider);

        k.label.setText(kKnobSpecs[i].title, juce::dontSendNotification);
        k.label.setJustificationType(juce::Justification::centred);
        k.label.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
        k.label.setColour(juce::Label::textColourId, juce::Colour(0xfff3ecff));
        addAndMakeVisible(k.label);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.apvts, kKnobSpecs[i].id, k.slider);
    }

    // Waveform selector
    waveBox.addItemList({ "SAW", "SQR" }, 1);
    addAndMakeVisible(waveBox);
    waveAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "waveform", waveBox);

    // Presets
    presetBox.addItemList({ "Goa Acid 142", "Berlin Minimal 132", "Phuture Classic 126", "Dark Forest 148" }, 1);
    presetBox.setSelectedId(1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        audioProcessor.loadPreset(presetBox.getSelectedId() - 1);
        refreshStepButtons();
    };
    addAndMakeVisible(presetBox);

    // Scale & Root for Acid Randomizer
    scaleBox.addItemList({ "Phrygian (Goa)", "Natural Minor", "Hirajoshi (Dark)", "Chromatic" }, 1);
    scaleBox.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(scaleBox);

    for (int i = 0; i < 12; ++i)
        rootBox.addItem(kNoteNames[i], i + 1);
    rootBox.setSelectedId(3, juce::dontSendNotification); // D
    addAndMakeVisible(rootBox);

    // Internal Sequencer Toggle + BPM
    seqToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff00f5ff));
    seqToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffaaff00));
    addAndMakeVisible(seqToggle);
    seqAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "seqOn", seqToggle);

    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 44, 22);
    bpmSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffff00cc));
    bpmSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff00f5ff));
    addAndMakeVisible(bpmSlider);
    bpmAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "bpm", bpmSlider);

    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9e8ec0));
    addAndMakeVisible(bpmLabel);

    // Sequencer Action Buttons
    randBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff38124e));
    randBtn.onClick = [this]
    {
        audioProcessor.randomizePattern(scaleBox.getSelectedId() - 1, rootBox.getSelectedId() - 1);
        refreshStepButtons();
    };
    addAndMakeVisible(randBtn);

    mutateBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b3320));
    mutateBtn.onClick = [this]
    {
        audioProcessor.mutatePattern(scaleBox.getSelectedId() - 1, rootBox.getSelectedId() - 1);
        refreshStepButtons();
    };
    addAndMakeVisible(mutateBtn);

    clearBtn.onClick = [this]
    {
        audioProcessor.clearPattern();
        refreshStepButtons();
    };
    addAndMakeVisible(clearBtn);

    // 16-Step Sequencer Buttons
    for (int i = 0; i < 16; ++i)
    {
        auto& ui = stepUIs[(size_t)i];

        ui.noteBtn.onClick = [this, i]
        {
            audioProcessor.pattern[(size_t)i].note = (audioProcessor.pattern[(size_t)i].note + 1) % 12;
            refreshStepButtons();
        };
        addAndMakeVisible(ui.noteBtn);

        ui.octBtn.onClick = [this, i]
        {
            int o = audioProcessor.pattern[(size_t)i].octave;
            audioProcessor.pattern[(size_t)i].octave = (o == 1 ? -1 : o + 1);
            refreshStepButtons();
        };
        addAndMakeVisible(ui.octBtn);

        ui.gateBtn.setClickingTogglesState(true);
        ui.gateBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff0099aa));
        ui.gateBtn.onClick = [this, i]
        {
            audioProcessor.pattern[(size_t)i].gate = stepUIs[(size_t)i].gateBtn.getToggleState();
            refreshStepButtons();
        };
        addAndMakeVisible(ui.gateBtn);

        ui.accBtn.setClickingTogglesState(true);
        ui.accBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffcc0099));
        ui.accBtn.onClick = [this, i]
        {
            audioProcessor.pattern[(size_t)i].accent = stepUIs[(size_t)i].accBtn.getToggleState();
        };
        addAndMakeVisible(ui.accBtn);

        ui.sldBtn.setClickingTogglesState(true);
        ui.sldBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff66aa00));
        ui.sldBtn.onClick = [this, i]
        {
            audioProcessor.pattern[(size_t)i].slide = stepUIs[(size_t)i].sldBtn.getToggleState();
        };
        addAndMakeVisible(ui.sldBtn);
    }

    refreshStepButtons();
    startTimerHz(30);
}

TB303PsyAcidAudioProcessorEditor::~TB303PsyAcidAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void TB303PsyAcidAudioProcessorEditor::refreshStepButtons()
{
    for (int i = 0; i < 16; ++i)
    {
        const auto& st = audioProcessor.pattern[(size_t)i];
        auto& ui = stepUIs[(size_t)i];
        ui.noteBtn.setButtonText(juce::String(kNoteNames[st.note]) + juce::String(2 + st.octave));
        ui.octBtn.setButtonText(st.octave > 0 ? "+1" : (st.octave < 0 ? "-1" : "0"));
        ui.gateBtn.setToggleState(st.gate, juce::dontSendNotification);
        ui.gateBtn.setButtonText(st.gate ? "ON" : "---");
        ui.accBtn.setToggleState(st.accent, juce::dontSendNotification);
        ui.accBtn.setButtonText("ACC");
        ui.sldBtn.setToggleState(st.slide, juce::dontSendNotification);
        ui.sldBtn.setButtonText("SLD");
    }
}

void TB303PsyAcidAudioProcessorEditor::timerCallback()
{
    animPhase += 0.015f;
    if (animPhase > juce::MathConstants<float>::twoPi)
        animPhase -= juce::MathConstants<float>::twoPi;
    repaint();
}

void TB303PsyAcidAudioProcessorEditor::drawSacredGeometry(juce::Graphics& g, juce::Rectangle<float> area)
{
    auto cx = area.getCentreX();
    auto cy = area.getCentreY();
    float r = 68.0f;

    g.saveState();
    g.addTransform(juce::AffineTransform::rotation(animPhase * 0.15f, cx, cy));
    g.setColour(juce::Colour(0xff9d4edd).withAlpha(0.11f));

    g.drawEllipse(cx - r * 3.2f, cy - r * 3.2f, r * 6.4f, r * 6.4f, 1.2f);
    g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);

    for (int i = 0; i < 6; ++i)
    {
        float a = (float)i * juce::MathConstants<float>::pi / 3.0f;
        float px = cx + std::cos(a) * r;
        float py = cy + std::sin(a) * r;
        g.drawEllipse(px - r, py - r, r * 2.0f, r * 2.0f, 1.0f);

        float px2 = cx + std::cos(a) * r * 2.0f;
        float py2 = cy + std::sin(a) * r * 2.0f;
        g.drawEllipse(px2 - r, py2 - r, r * 2.0f, r * 2.0f, 0.8f);
    }
    g.restoreState();
}

void TB303PsyAcidAudioProcessorEditor::drawOscilloscope(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour(juce::Colour(0xff06040c));
    g.fillRoundedRectangle(area, 8.0f);
    g.setColour(juce::Colour(0xff00f5ff).withAlpha(0.45f));
    g.drawRoundedRectangle(area, 8.0f, 1.2f);

    // Center line
    g.setColour(juce::Colour(0xff9d4edd).withAlpha(0.25f));
    g.drawHorizontalLine((int)area.getCentreY(), area.getX() + 4.0f, area.getRight() - 4.0f);

    juce::Path wave;
    const int n = TB303PsyAcidAudioProcessor::scopeSize;
    const int readStart = audioProcessor.scopeWritePos.load(std::memory_order_relaxed);
    const float midY = area.getCentreY();
    const float ampY = area.getHeight() * 0.42f;

    for (int i = 0; i < n; ++i)
    {
        float sample = audioProcessor.scopeBuffer[(size_t)((readStart + i) % n)];
        float x = area.getX() + ((float)i / (float)(n - 1)) * area.getWidth();
        float y = midY - juce::jlimit(-1.0f, 1.0f, sample) * ampY;
        if (i == 0) wave.startNewSubPath(x, y);
        else wave.lineTo(x, y);
    }

    g.setColour(juce::Colour(0xffff00cc).withAlpha(0.30f));
    g.strokePath(wave, juce::PathStrokeType(4.5f));
    g.setColour(juce::Colour(0xff00f5ff));
    g.strokePath(wave, juce::PathStrokeType(1.8f));
}

void TB303PsyAcidAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Obsidian UV Background
    juce::ColourGradient bg(juce::Colour(0xff140d26), 0.0f, 0.0f,
                            juce::Colour(0xff06040c), bounds.getWidth(), bounds.getHeight(), false);
    g.setGradientFill(bg);
    g.fillAll();

    // Sacred Geometry Watermark
    drawSacredGeometry(g, bounds);

    // Animated Iridescent Outer Border
    auto c1 = juce::Colour::fromHSV(std::fmod(animPhase * 0.16f, 1.0f), 0.95f, 1.0f, 0.9f);
    auto c2 = juce::Colour::fromHSV(std::fmod(animPhase * 0.16f + 0.35f, 1.0f), 0.95f, 1.0f, 0.9f);
    juce::ColourGradient borderGrad(c1, 0.0f, 0.0f, c2, bounds.getWidth(), bounds.getHeight(), false);
    g.setGradientFill(borderGrad);
    g.drawRoundedRectangle(bounds.reduced(2.0f), 12.0f, 3.0f);

    // Title
    g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff00f5ff));
    g.drawText("TB-303 PSYCHEDELIC ACID LINE", 20, 14, 420, 30, juce::Justification::centredLeft);

    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xffff00cc));
    g.drawText("UV DIODE-LADDER VST3 / AU  |  BITWIG READY", 20, 42, 340, 16, juce::Justification::centredLeft);

    // Section panels
    g.setColour(juce::Colour(0xff120c24).withAlpha(0.78f));
    g.fillRoundedRectangle(16.0f, 68.0f, 668.0f, 326.0f, 10.0f);
    g.fillRoundedRectangle(696.0f, 68.0f, 348.0f, 326.0f, 10.0f);
    g.fillRoundedRectangle(16.0f, 406.0f, 1028.0f, 238.0f, 10.0f);

    g.setColour(juce::Colour(0xff9d4edd).withAlpha(0.4f));
    g.drawRoundedRectangle(16.0f, 68.0f, 668.0f, 326.0f, 10.0f, 1.0f);
    g.drawRoundedRectangle(696.0f, 68.0f, 348.0f, 326.0f, 10.0f, 1.0f);
    g.drawRoundedRectangle(16.0f, 406.0f, 1028.0f, 238.0f, 10.0f, 1.0f);

    // Oscilloscope
    drawOscilloscope(g, scopeBounds);

    // Active Step LED Highlight
    const int activeStep = audioProcessor.currentPlayStep.load();
    const float colW = 1004.0f / 16.0f;
    for (int i = 0; i < 16; ++i)
    {
        float x = 28.0f + (float)i * colW;
        bool isNow = (i == activeStep);
        g.setColour(isNow ? juce::Colour(0xffaaff00) : juce::Colour(0xff2a1e44));
        g.fillEllipse(x + colW * 0.5f - 5.0f, 456.0f, 10.0f, 10.0f);
    }
}

void TB303PsyAcidAudioProcessorEditor::resized()
{
    // Top bar controls
    seqToggle.setBounds(455, 18, 92, 28);
    bpmLabel.setBounds(552, 18, 36, 28);
    bpmSlider.setBounds(588, 18, 130, 28);
    waveBox.setBounds(730, 18, 86, 28);
    presetBox.setBounds(826, 18, 214, 28);

    // Main 6 TB-303 Knobs (Row 1)
    for (int i = 0; i < 6; ++i)
    {
        int x = 28 + i * 108;
        knobs[(size_t)i].label.setBounds(x, 80, 100, 18);
        knobs[(size_t)i].slider.setBounds(x + 4, 98, 92, 106);
    }

    // FX & Master 5 Knobs (Row 2)
    for (int i = 0; i < 5; ++i)
    {
        int x = 42 + i * 126;
        knobs[(size_t)(6 + i)].label.setBounds(x, 236, 110, 18);
        knobs[(size_t)(6 + i)].slider.setBounds(x + 9, 254, 92, 106);
    }

    // Right panel: Oscilloscope + Randomizer controls
    scopeBounds = juce::Rectangle<float>(710.0f, 82.0f, 320.0f, 165.0f);
    scaleBox.setBounds(710, 260, 200, 28);
    rootBox.setBounds(920, 260, 110, 28);
    randBtn.setBounds(710, 302, 155, 32);
    mutateBtn.setBounds(875, 302, 155, 32);
    clearBtn.setBounds(710, 344, 320, 30);

    // Bottom panel: 16-Step Sequencer Grid
    const float colW = 1004.0f / 16.0f;
    for (int i = 0; i < 16; ++i)
    {
        int x = (int)(28.0f + (float)i * colW);
        int w = (int)colW - 6;
        auto& ui = stepUIs[(size_t)i];
        ui.noteBtn.setBounds(x, 472, w, 28);
        ui.octBtn.setBounds(x, 504, w, 24);
        ui.gateBtn.setBounds(x, 532, w, 30);
        ui.accBtn.setBounds(x, 566, w, 30);
        ui.sldBtn.setBounds(x, 600, w, 30);
    }
}
