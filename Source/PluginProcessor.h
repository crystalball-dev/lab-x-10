#pragma once

#include <array>
#include <atomic>
#include <limits>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Config.h"
#include "Params.h"
#include "SpecimenLibrary.h"
#include "DSP/Emitter.h"
#include "DSP/Modulation.h"
#include "DSP/Regulator.h"
#include "DSP/Resonator.h"
#include "DSP/Voice.h"

class LabX10AudioProcessor final : public juce::AudioProcessor
{
public:
    LabX10AudioProcessor();
    ~LabX10AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return labx10::pluginName; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    labx10::SpecimenLibrary& getSpecimenLibrary() noexcept { return specimens; }

    // Editor-facing readouts, written on the audio thread.
    struct Meters
    {
        std::atomic<float> rmsDb { -100.0f }, peakDb { -100.0f }, subDb { -100.0f }, lowCorrelation { 1.0f };
        std::atomic<float> scorch { 0.0f }, midiPerSecond { 0.0f }, bpm { 128.0f }, gainReductionDb { 0.0f };
        std::atomic<int> activeVoices { 0 }, faults { 0 }, latency { 0 };
        std::atomic<bool> synced { false };
    };
    Meters meters;

    static constexpr int scopeSize = 2048;
    void copyScope (float* dest, int numSamples) const noexcept;

    juce::AudioProcessorValueTreeState apvts;

   #if LABX10_HARNESS
    // Test hook: audio added to the voice sum before the global chain (latency measurement).
    std::vector<float> testInjection;
    size_t testInjectionPos = 0;
    bool muteVoices = false;
   #endif

private:
    struct Smoother
    {
        bool enabled = false, logDomain = false;
        float current = 0.0f;

        void reset (float v) noexcept { current = logDomain ? std::log2 (std::max (v, 1.0e-4f)) : v; }

        float advance (float target, float coef) noexcept
        {
            const float t = logDomain ? std::log2 (std::max (target, 1.0e-4f)) : target;
            current += (t - current) * coef;
            return logDomain ? std::exp2 (current) : current;
        }
    };

    void renderControlBlock (float* outL, float* outR, int n) noexcept;
    void updateControl (int n) noexcept;
    void flushForProgramChange() noexcept;
    void recoverFromFault() noexcept;

    void handleMidi (const juce::MidiMessage& m) noexcept;
    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void setSustainPedal (bool down) noexcept;
    void allNotesOff() noexcept;
    void allSoundOff() noexcept;
    int voiceMode() const noexcept;
    int polyLimit() const noexcept;
    int pickFreeVoice (int exclude) const noexcept;
    void pushMonoNote (int note) noexcept;
    void removeMonoNote (int note) noexcept;

    void applyPreset (int index);

    labx10::SpecimenLibrary specimens;
    const labx10::dsp::SpecimenData* blockSpecimen = nullptr;

    std::array<std::atomic<float>*, labx10::P::count> raw {};
    std::array<Smoother, labx10::P::count> smoothers {};
    std::array<float, labx10::P::count> values {};
    std::array<float, labx10::controlBlock + 1> smoothCoef {}, duckCoef {};

    std::array<labx10::dsp::Voice, labx10::maxVoices> voices;
    labx10::dsp::VoiceParams voiceParams;
    labx10::dsp::Emitter emitter;
    labx10::dsp::Emitter::Settings emitterSettings;
    labx10::dsp::Resonator resonator;
    labx10::dsp::Lfo lfo;
    labx10::dsp::Cooldown cooldown;
    labx10::dsp::Regulator regulator;
    labx10::dsp::Transport transport;

    // Per control block scratch
    std::array<float, labx10::controlBlock> lowBand {}, highL {}, highR {}, coolGain {};
    double currentSampleRate = 48000.0;

    // Modulation results for the current control block
    float scorchEffective = 0.0f, lfoLevelTarget = 1.0f, lfoLevelGain = 1.0f;
    float resonatorHz = 110.0f, resonatorFeedback = 0.5f, resonatorVowel = 0.0f, resonatorLevel = 0.0f;
    float coolBeats = 0.0f, coolDepth = 0.0f, coolHoldValue = 0.4f;
    int coolShapeIndex = 0;
    float widthValue = 1.0f;
    float resonatorPitch = 48.0f;

    // MIDI state (audio thread)
    std::array<int, 16> monoStack {};
    int monoCount = 0, monoVoice = 0;
    bool sustainDown = false;
    float bendTarget = 0.0f, bend = 0.0f, modWheelTarget = 0.0f, modWheel = 0.0f;
    float lastNotePitch = 48.0f, accentCharge = 0.0f;
    bool anyNotePlayed = false;
    uint64_t voiceClock = 0;
    int lastSpecimenChoice = -1;
    bool retriggerLfo = false;

    // SPECIMEN source changes: the specimen layer ducks out until the new file has been published.
    bool specimenAwait = false;
    uint64_t specimenAwaitId = 0;
    float specimenDuck = 1.0f;

    // Preset and oversampling changes: fade out, switch, fade back in.
    enum class SwitchState { idle, fadingOut, fadingIn };
    std::atomic<bool> programChangePending { false };
    SwitchState switchState = SwitchState::idle;
    bool switchFlushVoices = false;
    int switchOversampling = -1;
    int switchFadeRemaining = 0, switchFadeLength = 1440;
    float switchGain = 1.0f, switchFadeInStep = 0.005f;

    // Output stage
    float masterGain = 0.5f, dcInL = 0.0f, dcInR = 0.0f, dcOutL = 0.0f, dcOutR = 0.0f, dcCoef = 0.9995f;

    // Metering
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    int midiAccumulator = 0, meterSamples = 0;
    float subLp1 = 0.0f, subLp2 = 0.0f, subCoef = 0.01f, lowLpL = 0.0f, lowLpR = 0.0f, lowCoef = 0.02f;
    double subSum = 0.0, corrLR = 0.0, corrLL = 0.0, corrRR = 0.0;
    int subCount = 0;

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabX10AudioProcessor)
};
