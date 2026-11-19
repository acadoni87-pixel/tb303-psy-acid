#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    const std::vector<std::vector<int>> kScales = {
        { 0, 1, 3, 5, 7, 8, 10 },                  // Phrygian (Goa/Psy)
        { 0, 2, 3, 5, 7, 8, 10 },                  // Natural Minor
        { 0, 2, 3, 7, 8 },                         // Hirajoshi (Dark)
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }   // Chromatic Acid
    };

    const float kDelayMultipliers[5] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f };
}

TB303PsyAcidAudioProcessor::TB303PsyAcidAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    loadPreset(0);
}

juce::AudioProcessorValueTreeState::ParameterLayout TB303PsyAcidAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back(std::make_unique<juce::AudioParameterFloat>("tuning",    "Tuning",     juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("cutoff",    "Cut Off",    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.36f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("resonance", "Resonance",  juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.68f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("envMod",    "Env Mod",    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.76f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("decay",     "Decay",      juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.38f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("accent",    "Accent",     juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.78f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>("drive",     "Overdrive",  juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("delayMix",  "Psy Delay",  juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.30f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("delayFb",   "Delay Fdbk", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.44f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("delaySync", "Delay Sync", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),  0.50f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("volume",    "Master",     juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.78f));

    p.push_back(std::make_unique<juce::AudioParameterChoice>("waveform", "Waveform", juce::StringArray { "SAW", "SQR" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("bpm",       "BPM",      juce::NormalisableRange<float>(60.0f, 200.0f, 1.0f), 142.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("seqOn",      "Seq On",   false));

    return { p.begin(), p.end() };
}

void TB303PsyAcidAudioProcessor::prepareToPlay(double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    const int maxDelaySamples = (int)(sr * 2.5);
    delayBufL.assign((size_t)maxDelaySamples, 0.0f);
    delayBufR.assign((size_t)maxDelaySamples, 0.0f);
    delayWriteIdx = 0;

    for (auto& s : fStage) s = 0.0;
    dlyLpL = dlyHpL = dlyLpR = dlyHpR = 0.0f;
    oscPhase = 0.0;
    filterEnv = 0.0;
    ampEnv = 0.0;
    accentCharge = 0.0;
    heldNotes.clear();
}

void TB303PsyAcidAudioProcessor::releaseResources() {}

bool TB303PsyAcidAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

double TB303PsyAcidAudioProcessor::polyBlep(double t, double dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

double TB303PsyAcidAudioProcessor::midiToFreq(double midiNote)
{
    return 440.0 * std::pow(2.0, (midiNote - 69.0) / 12.0);
}

void TB303PsyAcidAudioProcessor::triggerNote(int midiNote, bool accented, bool slideLegato, int gateSamples)
{
    const double tuning = (double)apvts.getRawParameterValue("tuning")->load();
    targetFreq = midiToFreq((double)midiNote + tuning);
    samplesUntilGateOff = gateSamples;

    if (slideLegato && gateOpen)
    {
        // Authentic 303 Slide: glide pitch smoothly without retriggering envelopes
        return;
    }

    currentFreq = targetFreq;
    gateOpen = true;
    currentNoteAccented = accented;

    const double accAmt = (double)apvts.getRawParameterValue("accent")->load();
    if (accented)
        accentCharge = std::min(1.0, accentCharge * 0.45 + accAmt);
    else
        accentCharge *= 0.28;

    filterEnv = 1.0;
    ampEnv = 1.0;
}

void TB303PsyAcidAudioProcessor::releaseNote()
{
    gateOpen = false;
}

float TB303PsyAcidAudioProcessor::processDiodeFilter(float input, float cutoffHz, float resonance)
{
    // 4-stage cascaded diode-ladder style filter with nonlinear saturation
    const double clampedCut = juce::jlimit(35.0, sr * 0.42, (double)cutoffHz);
    const double g = std::tan(juce::MathConstants<double>::pi * clampedCut / sr);
    const double G = g / (1.0 + g);

    // Resonance feedback scaled for authentic 303 squelch without blowing up
    const double k = juce::jlimit(0.0, 3.85, (double)resonance * 3.82);

    // Pre-filter diode warmth
    double in = std::tanh((double)input * 1.35);

    // Feedback from 4th stage with slight soft clipping
    double u = in - k * std::tanh(fStage[3] * 1.15);

    // 4 cascaded one-pole sections (diode ladder character)
    for (int i = 0; i < 4; ++i)
    {
        double v = (u - fStage[i]) * G;
        double y = v + fStage[i];
        fStage[i] = y + v;
        u = std::tanh(y * 1.05);
    }

    return (float)u;
}

float TB303PsyAcidAudioProcessor::processDiodeDrive(float x, float driveAmount)
{
    const float k = 1.0f + driveAmount * 22.0f;
    return std::tanh(x * k + 0.08f * driveAmount * x * x);
}

void TB303PsyAcidAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    auto* outL = buffer.getWritePointer(0);
    auto* outR = buffer.getWritePointer(1);

    // Read parameters
    const float cutoffParam  = apvts.getRawParameterValue("cutoff")->load();
    const float resParam     = apvts.getRawParameterValue("resonance")->load();
    const float envModParam  = apvts.getRawParameterValue("envMod")->load();
    const float decayParam   = apvts.getRawParameterValue("decay")->load();
    const float driveParam   = apvts.getRawParameterValue("drive")->load();
    const float dlyMixParam  = apvts.getRawParameterValue("delayMix")->load();
    const float dlyFbParam   = apvts.getRawParameterValue("delayFb")->load();
    const float dlySyncParam = apvts.getRawParameterValue("delaySync")->load();
    const float volParam     = apvts.getRawParameterValue("volume")->load();
    const int   waveChoice   = (int)apvts.getRawParameterValue("waveform")->load();
    const bool  seqEnabled   = apvts.getRawParameterValue("seqOn")->load() > 0.5f;
    float bpm                = apvts.getRawParameterValue("bpm")->load();

    // Sync to DAW Host BPM if available (Bitwig Studio)
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto hostBpm = pos->getBpm())
                if (*hostBpm >= 40.0 && *hostBpm <= 240.0)
                    bpm = (float)*hostBpm;
        }
    }

    // Delay time calculation
    const int divIdx = juce::jlimit(0, 4, (int)std::floor(dlySyncParam * 5.0f));
    const float beatSec = 60.0f / bpm;
    const int dlySamplesL = juce::jlimit(1, (int)delayBufL.size() - 2, (int)(beatSec * kDelayMultipliers[divIdx] * sr));
    const int dlySamplesR = juce::jlimit(1, (int)delayBufR.size() - 2, (int)((float)dlySamplesL * 0.992f));
    const float fbAmt = juce::jlimit(0.0f, 0.80f, dlyFbParam * 0.80f);

    // 60ms exponential slide coefficient (tau ~ 20ms => 3*tau = 60ms)
    const double slideCoeff = 1.0 - std::exp(-1.0 / (0.020 * sr));

    // Sequencer step length in samples (16th notes)
    const double samplesPerStep = (60.0 / (double)bpm) * 0.25 * sr;

    // Collect incoming MIDI events indexed by sample offset
    auto midiIt = midiMessages.begin();

    for (int i = 0; i < numSamples; ++i)
    {
        // Process MIDI at exact sample offset
        while (midiIt != midiMessages.end() && (*midiIt).samplePosition <= i)
        {
            const auto msg = (*midiIt).getMessage();
            if (msg.isNoteOn())
            {
                const bool isLegato = !heldNotes.empty();
                const bool isAcc = msg.getVelocity() >= 105;
                heldNotes.push_back({ msg.getNoteNumber(), msg.getVelocity() });
                triggerNote(msg.getNoteNumber(), isAcc, isLegato, 0);
            }
            else if (msg.isNoteOff())
            {
                const int n = msg.getNoteNumber();
                heldNotes.erase(std::remove_if(heldNotes.begin(), heldNotes.end(),
                    [n](const ActiveNote& an) { return an.note == n; }), heldNotes.end());

                if (heldNotes.empty())
                {
                    releaseNote();
                }
                else
                {
                    const auto& back = heldNotes.back();
                    triggerNote(back.note, back.vel >= 105, true, 0);
                }
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                heldNotes.clear();
                releaseNote();
            }
            ++midiIt;
        }

        // Internal 16-Step Sequencer
        if (seqEnabled)
        {
            if (seqSampleCounter <= 0.0)
            {
                const auto& st = pattern[(size_t)seqStepIndex];
                currentPlayStep.store(seqStepIndex);

                if (st.gate)
                {
                    const int midiNote = 36 + st.octave * 12 + st.note;
                    const int gateSamps = st.slide ? (int)(samplesPerStep * 1.15) : (int)(samplesPerStep * 0.55);
                    triggerNote(midiNote, st.accent, prevStepSlid, gateSamps);
                    prevStepSlid = st.slide;
                }
                else
                {
                    prevStepSlid = false;
                    releaseNote();
                }

                seqStepIndex = (seqStepIndex + 1) % 16;
                seqSampleCounter += samplesPerStep;
            }
            seqSampleCounter -= 1.0;

            if (samplesUntilGateOff > 0)
            {
                --samplesUntilGateOff;
                if (samplesUntilGateOff == 0 && heldNotes.empty())
                    releaseNote();
            }
        }
        else
        {
            currentPlayStep.store(-1);
        }

        // Smooth pitch slide
        currentFreq += (targetFreq - currentFreq) * slideCoeff;

        // Oscillator (PolyBLEP Sawtooth or Authentic 303 Asymmetric Square)
        const double dt = juce::jlimit(0.00001, 0.45, currentFreq / sr);
        oscPhase += dt;
        if (oscPhase >= 1.0) oscPhase -= 1.0;

        double rawOsc = 0.0;
        if (waveChoice == 0)
        {
            // Sawtooth with PolyBLEP
            rawOsc = (2.0 * oscPhase - 1.0) - polyBlep(oscPhase, dt);
        }
        else
        {
            // Authentic 303 Asymmetric Pulse/Square (slightly off 50% duty + diode rounding)
            const double pw = 0.46;
            double sq = (oscPhase < pw ? 1.0 : -1.0);
            sq += polyBlep(oscPhase, dt);
            sq -= polyBlep(std::fmod(oscPhase + (1.0 - pw), 1.0), dt);
            rawOsc = sq * 0.88;
        }

        // Authentic 303 Filter Envelope (Accent forces snappy ~95ms decay)
        const double effDecaySec = currentNoteAccented ? 0.095 : (0.08 + (double)decayParam * 0.62);
        const double envDecayCoeff = std::exp(-1.0 / (effDecaySec * sr));
        filterEnv *= envDecayCoeff;

        // Amp Envelope
        if (gateOpen)
        {
            const double ampDecay = std::exp(-1.0 / ((0.18 + (double)decayParam * 0.8) * sr));
            ampEnv = std::max(0.65, ampEnv * ampDecay);
        }
        else
        {
            ampEnv *= std::exp(-1.0 / (0.014 * sr));
        }

        // Compute dynamic filter cutoff & resonance
        const double baseCut = 65.0 * std::pow(65.0, (double)cutoffParam);
        const double envRange = (120.0 + (double)envModParam * 6800.0) * (1.0 + (currentNoteAccented ? accentCharge * 1.45 : 0.0));
        const float dynamicCut = (float)juce::jlimit(45.0, 15500.0, baseCut + filterEnv * envRange);
        const float dynamicRes = juce::jlimit(0.0f, 0.99f, resParam + (currentNoteAccented ? (float)accentCharge * 0.12f : 0.0f));

        // Filter -> VCA -> Overdrive
        float sig = processDiodeFilter((float)rawOsc, dynamicCut, dynamicRes);
        const float accBoost = 0.52f + (currentNoteAccented ? (float)accentCharge * 0.48f : 0.0f);
        sig *= (float)ampEnv * accBoost;
        sig = processDiodeDrive(sig, driveParam);

        // Ping-Pong Stereo Psy Delay with Bandpass feedback
        const int bufLen = (int)delayBufL.size();
        const int readL = (delayWriteIdx - dlySamplesL + bufLen) % bufLen;
        const int readR = (delayWriteIdx - dlySamplesR + bufLen) % bufLen;

        const float tapL = delayBufL[(size_t)readL];
        const float tapR = delayBufR[(size_t)readR];

        // Simple 1-pole bandpass on delay input/feedback for psytrance clarity
        dlyLpL += 0.22f * ((sig + tapR * fbAmt) - dlyLpL);
        dlyHpL += 0.02f * (dlyLpL - dlyHpL);
        const float feedL = dlyLpL - dlyHpL;

        dlyLpR += 0.22f * ((tapL * fbAmt) - dlyLpR);
        dlyHpR += 0.02f * (dlyLpR - dlyHpR);
        const float feedR = dlyLpR - dlyHpR;

        delayBufL[(size_t)delayWriteIdx] = std::tanh(feedL);
        delayBufR[(size_t)delayWriteIdx] = std::tanh(feedR);
        delayWriteIdx = (delayWriteIdx + 1) % bufLen;

        const float dryAmt = 1.0f - dlyMixParam * 0.35f;
        const float wetAmt = dlyMixParam * 0.85f;

        // Soft limiter on master output
        const float finalL = std::tanh((sig * dryAmt + tapL * wetAmt) * volParam * 1.15f);
        const float finalR = std::tanh((sig * dryAmt + tapR * wetAmt) * volParam * 1.15f);

        outL[i] = finalL;
        outR[i] = finalR;

        // Feed oscilloscope buffer
        const int wPos = scopeWritePos.load(std::memory_order_relaxed);
        scopeBuffer[(size_t)wPos] = 0.5f * (finalL + finalR);
        scopeWritePos.store((wPos + 1) % scopeSize, std::memory_order_relaxed);
    }
}

void TB303PsyAcidAudioProcessor::setCurrentProgram(int index)
{
    loadPreset(index);
}

const juce::String TB303PsyAcidAudioProcessor::getProgramName(int index)
{
    switch (index)
    {
        case 0: return "Goa Acid 142 (D Phrygian)";
        case 1: return "Berlin Minimal 132";
        case 2: return "Phuture Classic 126";
        case 3: return "Dark Forest 148";
        default: return "Acid Preset";
    }
}

void TB303PsyAcidAudioProcessor::loadPreset(int index)
{
    currentPresetIndex = juce::jlimit(0, 3, index);

    auto setParam = [this](const char* id, float val)
    {
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(val));
    };

    if (currentPresetIndex == 0) // Goa Acid 142
    {
        setParam("bpm", 142.0f); setParam("waveform", 0.0f);
        setParam("cutoff", 0.36f); setParam("resonance", 0.68f); setParam("envMod", 0.76f);
        setParam("decay", 0.38f); setParam("accent", 0.78f); setParam("drive", 0.35f); setParam("delayMix", 0.32f);
        pattern = {{
            {2,0,1,1,0},{2,0,1,0,0},{2,1,1,0,1},{3,1,1,1,0},
            {2,0,1,0,0},{9,0,1,0,1},{10,0,1,1,0},{2,0,1,0,0},
            {2,0,1,1,0},{3,0,1,0,1},{2,1,1,1,0},{7,0,1,0,0},
            {5,0,1,0,1},{3,0,1,1,0},{2,1,1,0,1},{2,0,1,0,0}
        }};
    }
    else if (currentPresetIndex == 1) // Berlin Minimal 132
    {
        setParam("bpm", 132.0f); setParam("waveform", 1.0f);
        setParam("cutoff", 0.24f); setParam("resonance", 0.56f); setParam("envMod", 0.54f);
        setParam("decay", 0.32f); setParam("accent", 0.68f); setParam("drive", 0.42f); setParam("delayMix", 0.24f);
        pattern = {{
            {0,0,1,1,0},{0,0,0,0,0},{0,0,1,0,1},{7,0,1,0,0},
            {0,0,0,0,0},{0,1,1,1,0},{3,0,1,0,1},{0,0,1,0,0},
            {0,0,1,0,0},{10,-1,1,1,1},{0,0,1,0,0},{0,0,0,0,0},
            {7,0,1,1,0},{0,0,1,0,0},{3,0,1,0,1},{0,0,0,0,0}
        }};
    }
    else if (currentPresetIndex == 2) // Phuture Classic 126
    {
        setParam("bpm", 126.0f); setParam("waveform", 0.0f);
        setParam("cutoff", 0.30f); setParam("resonance", 0.78f); setParam("envMod", 0.82f);
        setParam("decay", 0.48f); setParam("accent", 0.85f); setParam("drive", 0.25f); setParam("delayMix", 0.18f);
        pattern = {{
            {9,0,1,1,0},{9,0,1,0,1},{9,1,1,1,0},{0,1,1,0,0},
            {9,0,0,0,0},{7,0,1,1,1},{9,0,1,0,0},{4,0,1,0,0},
            {9,0,1,1,0},{9,1,1,0,1},{7,1,1,1,0},{9,0,0,0,0},
            {0,1,1,0,1},{9,0,1,1,0},{7,0,1,0,0},{9,0,1,0,1}
        }};
    }
    else // Dark Forest 148
    {
        setParam("bpm", 148.0f); setParam("waveform", 0.0f);
        setParam("cutoff", 0.28f); setParam("resonance", 0.84f); setParam("envMod", 0.88f);
        setParam("decay", 0.28f); setParam("accent", 0.90f); setParam("drive", 0.55f); setParam("delayMix", 0.38f);
        pattern = {{
            {5,0,1,1,0},{6,0,1,0,1},{5,1,1,1,0},{5,0,1,0,0},
            {8,0,1,0,1},{5,0,1,1,0},{6,1,1,0,1},{5,0,0,0,0},
            {5,0,1,1,0},{1,1,1,0,1},{0,1,1,1,0},{5,0,1,0,0},
            {6,0,1,1,1},{5,1,1,0,0},{8,0,1,1,1},{5,0,1,0,0}
        }};
    }
}

void TB303PsyAcidAudioProcessor::randomizePattern(int scaleIndex, int rootNote)
{
    auto& rng = juce::Random::getSystemRandom();
    const auto& scale = kScales[(size_t)juce::jlimit(0, (int)kScales.size() - 1, scaleIndex)];

    for (int i = 0; i < 16; ++i)
    {
        int deg = scale[(size_t)rng.nextInt((int)scale.size())];
        pattern[(size_t)i].note = (rootNote + deg) % 12;
        float rOct = rng.nextFloat();
        pattern[(size_t)i].octave = rOct < 0.14f ? -1 : (rOct > 0.72f ? 1 : 0);
        pattern[(size_t)i].gate = (i % 4 == 0) ? true : (rng.nextFloat() < 0.80f);
        pattern[(size_t)i].accent = pattern[(size_t)i].gate && (rng.nextFloat() < 0.34f);
        pattern[(size_t)i].slide = pattern[(size_t)i].gate && (rng.nextFloat() < 0.28f);
    }
}

void TB303PsyAcidAudioProcessor::mutatePattern(int scaleIndex, int rootNote)
{
    auto& rng = juce::Random::getSystemRandom();
    const auto& scale = kScales[(size_t)juce::jlimit(0, (int)kScales.size() - 1, scaleIndex)];

    for (auto& st : pattern)
    {
        if (rng.nextFloat() < 0.25f)
            st.note = (rootNote + scale[(size_t)rng.nextInt((int)scale.size())]) % 12;
        if (rng.nextFloat() < 0.18f) st.accent = !st.accent;
        if (rng.nextFloat() < 0.18f) st.slide = !st.slide;
        if (rng.nextFloat() < 0.12f) st.octave = (st.octave == 1 ? 0 : 1);
    }
}

void TB303PsyAcidAudioProcessor::clearPattern()
{
    for (auto& st : pattern)
    {
        st.gate = false;
        st.accent = false;
        st.slide = false;
    }
}

void TB303PsyAcidAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void TB303PsyAcidAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* TB303PsyAcidAudioProcessor::createEditor()
{
    return new TB303PsyAcidAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TB303PsyAcidAudioProcessor();
}
