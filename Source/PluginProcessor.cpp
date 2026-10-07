#include "PluginProcessor.h"
#include "FactoryPresets.h"

using namespace labx10;

LabX10AudioProcessor::LabX10AudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LABX10", createParameterLayout())
{
    const auto& specs = paramSpecs();
    for (int i = 0; i < P::count; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (specs[(size_t) i].id);
        jassert (raw[(size_t) i] != nullptr);
    }
    setCurrentProgram (0);
}

void LabX10AudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    for (auto& v : voices)
    {
        v.env.setSampleRate ((float) currentSampleRate);
        v.env.kill();
        v.note = -1;
    }
}

bool LabX10AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet().isDisabled() || layouts.inputBuses.isEmpty();
}

void LabX10AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    const float sr = (float) currentSampleRate;
    const float level = raw[P::subLevel]->load();
    const float gain = dsp::dbToGain (raw[P::volume]->load());

    for (auto& v : voices)
        v.env.setParameters (raw[P::env1A]->load(), raw[P::env1D]->load(), raw[P::env1S]->load(), raw[P::env1R]->load());

    auto it = midi.cbegin();
    for (int i = 0; i < numSamples; ++i)
    {
        while (it != midi.cend() && (*it).samplePosition <= i)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn())
            {
                SineVoice* target = nullptr;
                for (auto& v : voices)
                    if (! v.env.isActive()) { target = &v; break; }
                if (target == nullptr)
                    target = &voices[0];
                target->note = m.getNoteNumber();
                target->phase = 0.0f;
                target->inc = dsp::midiToHz ((float) target->note) / sr;
                target->env.noteOn();
            }
            else if (m.isNoteOff())
            {
                for (auto& v : voices)
                    if (v.note == m.getNoteNumber())
                        v.env.noteOff();
            }
            else if (m.isAllNotesOff() || m.isAllSoundOff())
            {
                for (auto& v : voices)
                    v.env.noteOff();
            }
            ++it;
        }

        float s = 0.0f;
        for (auto& v : voices)
        {
            if (! v.env.isActive())
                continue;
            s += dsp::sin2pi (v.phase) * v.env.next();
            v.phase = dsp::wrap01 (v.phase + v.inc);
        }

        const float out = dsp::safetyClip (s * level * gain * 0.5f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample (ch, i, out);
    }
}

int LabX10AudioProcessor::getNumPrograms()
{
    return (int) factoryPresets().size();
}

void LabX10AudioProcessor::setCurrentProgram (int index)
{
    const auto& presets = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    for (const auto& spec : paramSpecs())
    {
        auto* param = apvts.getParameter (spec.id);
        float target = param->convertFrom0to1 (param->getDefaultValue());
        for (const auto& [id, value] : presets[(size_t) index].values)
            if (id == spec.id)
                target = value;
        param->setValueNotifyingHost (param->convertTo0to1 (target));
    }
    currentProgram = index;
}

const juce::String LabX10AudioProcessor::getProgramName (int index)
{
    const auto& presets = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String();
}

void LabX10AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("version", versionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LabX10AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));
    apvts.replaceState (tree);
}

juce::AudioProcessorEditor* LabX10AudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LabX10AudioProcessor();
}
