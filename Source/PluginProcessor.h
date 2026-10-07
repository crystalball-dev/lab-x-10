#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Config.h"
#include "Params.h"
#include "DSP/Common.h"
#include "DSP/Envelope.h"

// M0 bootstrap: one sine voice per note so the build, plugin IDs and host scan can be verified.
class LabX10AudioProcessor final : public juce::AudioProcessor
{
public:
    LabX10AudioProcessor();
    ~LabX10AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return labx10::pluginName; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    struct SineVoice
    {
        int note = -1;
        float phase = 0.0f, inc = 0.0f;
        labx10::dsp::Envelope env;
    };

    std::array<std::atomic<float>*, labx10::P::count> raw {};
    std::array<SineVoice, labx10::maxVoices> voices;
    double currentSampleRate = 48000.0;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabX10AudioProcessor)
};
