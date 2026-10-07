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
        smoothers[(size_t) i].enabled = specs[(size_t) i].smooth != Smooth::none;
        smoothers[(size_t) i].logDomain = specs[(size_t) i].smooth == Smooth::log;
    }

    applyPreset (0);
    specimens.requestChoice ((int) std::lround (raw[P::specSource]->load()));
}

LabX10AudioProcessor::~LabX10AudioProcessor() = default;

//==============================================================================
void LabX10AudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    const auto sr = (float) currentSampleRate;

    for (int n = 0; n <= controlBlock; ++n)
    {
        smoothCoef[(size_t) n] = 1.0f - std::exp (-(float) std::max (n, 1) / (0.015f * sr));
        duckCoef[(size_t) n] = 1.0f - std::exp (-(float) std::max (n, 1) / (0.006f * sr));
    }

    for (int i = 0; i < P::count; ++i)
    {
        smoothers[(size_t) i].reset (raw[(size_t) i]->load());
        values[(size_t) i] = raw[(size_t) i]->load();
    }

    for (int v = 0; v < maxVoices; ++v)
        voices[(size_t) v].prepare (sr, (uint32_t) (v + 1) * 7919u);

    emitter.prepare (currentSampleRate);
    emitter.setOversampling ((int) std::lround (raw[P::emitOs]->load()));
    resonator.prepare (sr);
    lfo.prepare (sr, 77u);
    cooldown.prepare (sr);
    regulator.prepare (sr);

    const int latency = emitter.getLatency() + regulator.getLatency();
    setLatencySamples (latency);
    meters.latency.store (latency);

    monoCount = 0;
    monoVoice = 0;
    sustainDown = false;
    bend = bendTarget = 0.0f;
    modWheel = modWheelTarget = 0.0f;
    accentCharge = 0.0f;
    retriggerLfo = false;
    masterGain = dsp::dbToGain (raw[P::volume]->load());
    dcInL = dcInR = dcOutL = dcOutR = 0.0f;
    dcCoef = 1.0f - dsp::twoPi * 4.0f / sr;
    lfoLevelGain = lfoLevelTarget = 1.0f;

    midiAccumulator = meterSamples = 0;
    subLp1 = subLp2 = lowLpL = lowLpR = 0.0f;
    subCoef = dsp::onePoleCoefHz (60.0f, sr);
    lowCoef = dsp::onePoleCoefHz (120.0f, sr);
    subSum = corrLR = corrLL = corrRR = 0.0;
    subCount = 0;

    lastSpecimenChoice = -1;
    specimenAwait = false;
    specimenDuck = 1.0f;

    programChangePending.store (false);
    switchState = SwitchState::idle;
    switchFlushVoices = false;
    switchOversampling = -1;
    switchGain = 1.0f;
    switchFadeLength = std::max (1, (int) (0.03 * currentSampleRate));
    switchFadeRemaining = 0;
    switchFadeInStep = 1.0f / (0.005f * sr);

    transport = {};
    updateControl (controlBlock);
}

void LabX10AudioProcessor::releaseResources()
{
    for (auto& v : voices)
        v.kill();
}

bool LabX10AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet().isDisabled() || layouts.inputBuses.isEmpty();
}

double LabX10AudioProcessor::getTailLengthSeconds() const
{
    const float release = raw[P::env1R]->load() * 2.5f;
    const float ring = raw[P::resLevel]->load() > 0.0f ? 1.5f : 0.0f;
    return (double) std::clamp (std::max (release, ring), 0.1f, 30.0f);
}

//==============================================================================
void LabX10AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    buffer.clear();

    blockSpecimen = specimens.acquireForBlock();
    const uint64_t specimenId = blockSpecimen != nullptr ? blockSpecimen->id : 0;

    const int choice = (int) std::lround (raw[P::specSource]->load());
    if (choice != lastSpecimenChoice)
    {
        specimens.requestChoice (choice);
        if (lastSpecimenChoice >= 0)
        {
            specimenAwait = true;
            specimenAwaitId = specimenId;
        }
        lastSpecimenChoice = choice;
    }
    else if (specimenAwait && (specimenId != specimenAwaitId || specimens.isSettled()))
    {
        specimenAwait = false;
    }

    if (programChangePending.exchange (false))
    {
        switchFlushVoices = true;
        if (switchState != SwitchState::fadingOut)
        {
            // Start from the current gain so rapid preset clicks never jump the level back up.
            switchState = SwitchState::fadingOut;
            switchFadeRemaining = (int) std::ceil (switchGain * (float) switchFadeLength);
        }
    }

    const int wantOversampling = std::clamp ((int) std::lround (raw[P::emitOs]->load()), 0, 2);
    if (wantOversampling != emitter.getOversampling())
    {
        switchOversampling = wantOversampling;
        if (switchState != SwitchState::fadingOut)
        {
            switchState = SwitchState::fadingOut;
            switchFadeRemaining = (int) std::ceil (switchGain * (float) switchFadeLength);
        }
    }

    // Host transport: tempo always when available, beat position only while playing.
    bool synced = false;
    double ppqStart = 0.0;
    if (auto* host = getPlayHead())
    {
        if (auto position = host->getPosition())
        {
            if (auto bpm = position->getBpm())
                if (*bpm > 1.0)
                    transport.bpm = *bpm;
            if (position->getIsPlaying())
                if (auto ppq = position->getPpqPosition())
                {
                    synced = true;
                    ppqStart = *ppq;
                }
        }
    }
    transport.synced = synced;

    if (voiceMode() == Choice::poly)
    {
        const int limit = polyLimit();
        for (int v = limit; v < maxVoices; ++v)
            if (voices[(size_t) v].isActive() && voices[(size_t) v].isKeyDown())
                voices[(size_t) v].noteOff();
    }

    if (numSamples == 0)
    {
        for (const auto metadata : midi)
            handleMidi (metadata.getMessage());
        return;
    }

    auto midiIt = midi.cbegin();
    const auto midiEnd = midi.cend();

    std::array<float, controlBlock> blockL {}, blockR {};
    double sumSquares = 0.0;
    float peak = 0.0f;
    int pos = 0;
    int scopePos = scopeWrite.load (std::memory_order_relaxed);
    const double beatsPerSample = transport.bpm / 60.0 / currentSampleRate;

    while (pos < numSamples)
    {
        while (midiIt != midiEnd && (*midiIt).samplePosition <= pos)
        {
            handleMidi ((*midiIt).getMessage());
            ++midiIt;
        }

        int next = numSamples;
        if (midiIt != midiEnd)
            next = std::min (next, (*midiIt).samplePosition);

        const int n = std::min (controlBlock, std::max (1, next - pos));
        transport.ppq = synced ? ppqStart + (double) pos * beatsPerSample : 0.0;

        renderControlBlock (blockL.data(), blockR.data(), n);

        if (numChannels >= 2)
        {
            buffer.copyFrom (0, pos, blockL.data(), n);
            buffer.copyFrom (1, pos, blockR.data(), n);
        }
        else if (numChannels == 1)
        {
            buffer.copyFrom (0, pos, blockL.data(), n, 0.5f);
            buffer.addFrom (0, pos, blockR.data(), n, 0.5f);
        }

        // Metering, scope and the sub monitor
        for (int i = 0; i < n; ++i)
        {
            const float l = blockL[(size_t) i], r = blockR[(size_t) i];
            const float mid = 0.5f * (l + r);
            sumSquares += (double) (l * l + r * r) * 0.5;
            peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
            scope[(size_t) scopePos] = mid;
            scopePos = (scopePos + 1) & (scopeSize - 1);

            subLp1 += (mid - subLp1) * subCoef;
            subLp2 += (subLp1 - subLp2) * subCoef;
            subSum += (double) subLp2 * subLp2;
            lowLpL += (l - lowLpL) * lowCoef;
            lowLpR += (r - lowLpR) * lowCoef;
            corrLR += (double) lowLpL * lowLpR;
            corrLL += (double) lowLpL * lowLpL;
            corrRR += (double) lowLpR * lowLpR;
            ++subCount;
        }

        pos += n;
    }
    scopeWrite.store (scopePos, std::memory_order_release);

    while (midiIt != midiEnd)
    {
        handleMidi ((*midiIt).getMessage());
        ++midiIt;
    }

    // Readouts
    int active = 0;
    for (auto& v : voices)
        if (v.isActive() && ! v.isFading())
            ++active;

    meterSamples += numSamples;
    if (meterSamples >= (int) (currentSampleRate * 0.1))
    {
        meters.midiPerSecond.store ((float) midiAccumulator * (float) currentSampleRate / (float) meterSamples);
        meters.subDb.store (subCount > 0 ? juce::Decibels::gainToDecibels ((float) std::sqrt (subSum / subCount), -100.0f) : -100.0f);
        const double energy = std::sqrt (corrLL * corrRR);
        meters.lowCorrelation.store (energy > 1.0e-12 ? (float) (corrLR / energy) : 1.0f);
        midiAccumulator = 0;
        meterSamples = 0;
        subSum = corrLR = corrLL = corrRR = 0.0;
        subCount = 0;
    }

    const float rms = (float) std::sqrt (sumSquares / (double) numSamples);
    meters.rmsDb.store (juce::Decibels::gainToDecibels (rms, -100.0f));
    meters.peakDb.store (juce::Decibels::gainToDecibels (peak, -100.0f));
    meters.activeVoices.store (active);
    meters.scorch.store (scorchEffective);
    meters.bpm.store ((float) transport.bpm);
    meters.synced.store (synced);
    meters.gainReductionDb.store (regulator.getGainReductionDb());
}

void LabX10AudioProcessor::renderControlBlock (float* outL, float* outR, int n) noexcept
{
    updateControl (n);

    std::fill_n (outL, n, 0.0f);
    std::fill_n (outR, n, 0.0f);

    voiceParams.specimen = blockSpecimen;
    for (auto& v : voices)
        if (v.isActive())
            v.render (outL, outR, n, voiceParams);

   #if LABX10_HARNESS
    if (muteVoices)
    {
        std::fill_n (outL, n, 0.0f);
        std::fill_n (outR, n, 0.0f);
    }
    for (int i = 0; i < n && testInjectionPos < testInjection.size(); ++i, ++testInjectionPos)
    {
        outL[i] += testInjection[testInjectionPos];
        outR[i] += testInjection[testInjectionPos];
    }
   #endif

    // EMITTER splits the bands; everything after it works on the high band only.
    emitter.process (outL, outR, n, emitterSettings, lowBand.data(), highL.data(), highR.data());
    resonator.process (highL.data(), highR.data(), n, resonatorHz, resonatorFeedback, resonatorVowel, resonatorLevel);
    cooldown.process (coolGain.data(), n, coolBeats, coolShapeIndex, coolDepth, coolHoldValue, transport);

    // Recombine the bands, then master volume, so the REGULATOR's ceiling is the true output ceiling.
    const float levelStep = (lfoLevelTarget - lfoLevelGain) / (float) n;
    const float gainTarget = dsp::dbToGain (values[P::volume]);
    for (int i = 0; i < n; ++i)
    {
        lfoLevelGain += levelStep;
        masterGain += (gainTarget - masterGain) * 0.002f;
        const float g = coolGain[(size_t) i] * lfoLevelGain;
        const float hl = highL[(size_t) i] * g;
        const float hr = highR[(size_t) i] * g;
        const float mid = 0.5f * (hl + hr);
        const float side = 0.5f * (hl - hr) * widthValue;
        const float l = (lowBand[(size_t) i] + mid + side) * masterGain;
        const float r = (lowBand[(size_t) i] + mid - side) * masterGain;

        const float dl = l - dcInL + dcCoef * dcOutL;
        const float dr = r - dcInR + dcCoef * dcOutR;
        dcInL = l; dcOutL = dl;
        dcInR = r; dcOutR = dr;
        outL[i] = dl;
        outR[i] = dr;
    }

    regulator.process (outL, outR, n, values[P::regAmount], values[P::regCeiling], values[P::weight]);

    bool fault = false;
    for (int i = 0; i < n; ++i)
    {
        float l = outL[i];
        float r = outR[i];

        if (! std::isfinite (l) || ! std::isfinite (r))
        {
            // Never pass garbage to the host: silence this sample and reset after the block.
            fault = true;
            l = r = 0.0f;
        }

        if (switchState == SwitchState::fadingOut)
        {
            switchGain = (float) switchFadeRemaining / (float) switchFadeLength;
            if (switchFadeRemaining > 0)
                --switchFadeRemaining;
        }
        else if (switchState == SwitchState::fadingIn)
        {
            switchGain = std::min (1.0f, switchGain + switchFadeInStep);
            if (switchGain >= 1.0f)
                switchState = SwitchState::idle;
        }

        outL[i] = dsp::safetyClip (l) * switchGain;
        outR[i] = dsp::safetyClip (r) * switchGain;
    }

    if (fault)
        recoverFromFault();

    if (switchState == SwitchState::fadingOut && switchFadeRemaining == 0)
    {
        if (switchFlushVoices)
            flushForProgramChange();
        if (switchOversampling >= 0)
            emitter.setOversampling (switchOversampling);
        switchFlushVoices = false;
        switchOversampling = -1;
        switchState = SwitchState::fadingIn;
        switchGain = 0.0f;
    }
}

void LabX10AudioProcessor::updateControl (int n) noexcept
{
    const float coef = smoothCoef[(size_t) std::clamp (n, 0, controlBlock)];

    for (int i = 0; i < P::count; ++i)
    {
        const float target = raw[(size_t) i]->load (std::memory_order_relaxed);
        values[(size_t) i] = smoothers[(size_t) i].enabled ? smoothers[(size_t) i].advance (target, coef) : target;
    }

    bend += (bendTarget - bend) * std::min (1.0f, coef * 4.0f);
    modWheel += (modWheelTarget - modWheel) * coef;
    specimenDuck += ((specimenAwait ? 0.0f : 1.0f) - specimenDuck) * duckCoef[(size_t) std::clamp (n, 0, controlBlock)];

    const auto& v = values;
    const float sr = (float) currentSampleRate;
    auto choiceOf = [&v] (int index) { return (int) std::lround (v[(size_t) index]); };

    const float sc = std::clamp (v[P::scorch] + modWheel, 0.0f, 1.0f);
    scorchEffective = sc;
    accentCharge *= std::exp (-(float) n / (0.5f * sr));

    // LFO
    if (retriggerLfo)
    {
        lfo.retrigger();
        retriggerLfo = false;
    }
    const int syncIndex = std::clamp (choiceOf (P::lfoSync), 0, (int) std::size (lfoSyncBeats) - 1);
    const float lfoValue = lfo.advance (n, choiceOf (P::lfoShape), v[P::lfoRate], lfoSyncBeats[syncIndex], transport,
                                        choiceOf (P::lfoRetrig) == 1);
    const float depth = std::clamp (v[P::lfoDepth] * (1.0f + 0.5f * sc), -1.0f, 1.0f);
    const float mod = lfoValue * depth;
    const int target = choiceOf (P::lfoTarget);

    auto& p = voiceParams;
    p.lfoCutoffOct = target == Choice::toCutoff   ? mod * 4.0f  : 0.0f;
    p.lfoPitch     = target == Choice::toPitch    ? mod * 12.0f : 0.0f;
    p.lfoShape     = target == Choice::toShape    ? mod         : 0.0f;
    p.lfoPosition  = target == Choice::toPosition ? mod         : 0.0f;
    const float lfoDrive = target == Choice::toDrive     ? mod * 20.0f : 0.0f;
    const float lfoVowel = target == Choice::toResonator ? mod         : 0.0f;
    if (target == Choice::toLevel)
        lfoLevelTarget = depth >= 0.0f ? 1.0f - depth * 0.5f * (1.0f - lfoValue) : 1.0f + depth * 0.5f * (1.0f + lfoValue);
    else
        lfoLevelTarget = 1.0f;

    // Voices
    p.genAWave  = choiceOf (P::genAWave);
    p.genAPitch = 12.0f * (float) choiceOf (P::genAOctave) + (float) choiceOf (P::genASemi) + v[P::genAFine] * 0.01f;
    p.genAShape = v[P::genAShape];
    p.genALevel = v[P::genALevel];

    p.uniVoices = choiceOf (P::uniVoices);
    p.uniDetune = v[P::uniDetune] + 25.0f * sc;
    p.uniSpread = v[P::uniSpread];

    p.genBWave  = choiceOf (P::genBWave);
    p.genBRatio = v[P::genBRatio];
    p.genBFm    = v[P::genBFm];
    p.genBRing  = v[P::genBRing];
    p.genBLevel = v[P::genBLevel];

    p.subWave   = choiceOf (P::subWave);
    p.subOctave = (float) choiceOf (P::subOctave);
    p.subLevel  = v[P::subLevel];
    p.subDirect = choiceOf (P::subRoute) == Choice::routeDirect;

    p.sparkLevel  = v[P::sparkLevel];
    p.sparkColour = v[P::sparkColour];
    p.sparkDecay  = v[P::sparkDecay] * 0.001f;
    p.noiseLevel  = v[P::noiseLevel];

    p.specMode     = choiceOf (P::specMode);
    p.specLevel    = v[P::specLevel] * specimenDuck;
    p.specPosition = v[P::specPosition];
    p.specSize     = v[P::specSize] * 0.001f;
    p.specDensity  = v[P::specDensity];
    p.specTrack    = v[P::specTrack];

    p.filterModel  = choiceOf (P::filterModel);
    p.cutoff       = v[P::cutoff];
    p.resonance    = std::min (1.0f, v[P::resonance] + 0.2f * sc);
    p.driveDb      = v[P::filterDrive];
    p.filterEnv    = v[P::filterEnv];
    p.keytrack     = v[P::keytrack];
    p.accentAmount = v[P::accentAmount];

    p.a1 = v[P::env1A]; p.d1 = v[P::env1D]; p.s1 = v[P::env1S]; p.r1 = v[P::env1R];
    p.a2 = v[P::env2A]; p.d2 = v[P::env2D]; p.s2 = v[P::env2S]; p.r2 = v[P::env2R];

    const float bendSemis = bend * (float) choiceOf (P::bendRange);
    p.glide     = v[P::glide];
    p.pitchBend = bendSemis;
    p.dropTime  = v[P::dropTime] * 0.001f;

    // EMITTER
    emitterSettings.mode    = choiceOf (P::emitMode);
    emitterSettings.driveDb = v[P::emitDrive] + 18.0f * sc + lfoDrive;
    emitterSettings.bias    = v[P::emitBias];
    emitterSettings.tone    = std::clamp (v[P::emitTone] - 0.3f * sc, -1.0f, 1.0f);
    emitterSettings.splitHz = v[P::emitSplit];
    emitterSettings.mix     = v[P::emitMix];

    // RESONATOR follows the newest sounding note.
    uint64_t newest = 0;
    for (auto& voice : voices)
        if (voice.isActive() && ! voice.isFading() && voice.getAge() >= newest)
        {
            newest = voice.getAge();
            resonatorPitch = voice.getPitch();
        }
    resonatorHz       = dsp::midiToHz (resonatorPitch + bendSemis) * v[P::resRatio];
    resonatorFeedback = std::clamp (v[P::resFeedback] + 0.3f * sc, 0.0f, 1.0f);
    resonatorVowel    = std::clamp (v[P::resVowel] + lfoVowel, 0.0f, 1.0f);
    resonatorLevel    = v[P::resLevel];

    // COOLDOWN
    const int rate = std::clamp (choiceOf (P::coolRate), 0, (int) std::size (coolRateBeats) - 1);
    coolBeats      = coolRateBeats[rate];
    coolDepth      = rate > 0 ? std::clamp (v[P::coolDepth] + 0.3f * sc, 0.0f, 1.0f) : 0.0f;
    coolShapeIndex = choiceOf (P::coolShape);
    coolHoldValue  = v[P::coolHold];

    widthValue = v[P::width];
}

void LabX10AudioProcessor::flushForProgramChange() noexcept
{
    // The new preset starts from silence: no voices or effect tails carrying old settings.
    for (auto& v : voices)
        v.kill();
    monoCount = 0;
    emitter.reset();
    resonator.reset();
    regulator.reset();
    lfo.reset();
    cooldown.reset();
    accentCharge = 0.0f;
    dcInL = dcInR = dcOutL = dcOutR = 0.0f;
    for (int i = 0; i < P::count; ++i)
        smoothers[(size_t) i].reset (raw[(size_t) i]->load());
    masterGain = dsp::dbToGain (raw[P::volume]->load());
}

void LabX10AudioProcessor::recoverFromFault() noexcept
{
    for (auto& v : voices)
        v.kill();
    monoCount = 0;
    emitter.reset();
    resonator.reset();
    regulator.reset();
    dcInL = dcInR = dcOutL = dcOutR = 0.0f;
    meters.faults.fetch_add (1);
}

//==============================================================================
void LabX10AudioProcessor::handleMidi (const juce::MidiMessage& m) noexcept
{
    ++midiAccumulator;

    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        bendTarget = ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f;
    else if (m.isAllNotesOff())
        allNotesOff();
    else if (m.isAllSoundOff())
        allSoundOff();
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const int val = m.getControllerValue();
        if (cc == 1)
            modWheelTarget = (float) val / 127.0f;
        else if (cc == 64)
            setSustainPedal (val >= 64);
    }
}

int LabX10AudioProcessor::voiceMode() const noexcept
{
    return std::clamp ((int) std::lround (raw[P::voiceMode]->load()), 0, 2);
}

int LabX10AudioProcessor::polyLimit() const noexcept
{
    return juce::jlimit (1, maxVoices, (int) std::lround (raw[P::voices]->load()));
}

int LabX10AudioProcessor::pickFreeVoice (int exclude) const noexcept
{
    for (int v = 0; v < maxVoices; ++v)
        if (v != exclude && ! voices[(size_t) v].isActive())
            return v;

    // All busy: take the quietest one that is not the excluded voice.
    int best = exclude == 0 ? 1 : 0;
    for (int v = 0; v < maxVoices; ++v)
        if (v != exclude && voices[(size_t) v].getLevel() < voices[(size_t) best].getLevel())
            best = v;
    return best;
}

void LabX10AudioProcessor::pushMonoNote (int note) noexcept
{
    removeMonoNote (note);
    if (monoCount == (int) monoStack.size())
    {
        for (int j = 0; j < monoCount - 1; ++j)
            monoStack[(size_t) j] = monoStack[(size_t) j + 1];
        --monoCount;
    }
    monoStack[(size_t) monoCount++] = note;
}

void LabX10AudioProcessor::removeMonoNote (int note) noexcept
{
    for (int i = 0; i < monoCount; ++i)
        if (monoStack[(size_t) i] == note)
        {
            for (int j = i; j < monoCount - 1; ++j)
                monoStack[(size_t) j] = monoStack[(size_t) j + 1];
            --monoCount;
            return;
        }
}

void LabX10AudioProcessor::noteOn (int note, float velocity) noexcept
{
    const bool accented = velocity >= raw[P::accentVel]->load() - 1.0e-4f;
    if (accented)
        accentCharge = std::min (1.0f, accentCharge + 0.5f);

    if ((int) std::lround (raw[P::lfoRetrig]->load()) == 1)
        retriggerLfo = true;

    dsp::NoteStart s;
    s.note = note;
    s.velocity = velocity;
    s.accent = accented;
    s.accentSweep = accented ? accentCharge : 0.0f;
    s.dropAmount = raw[P::dropAmount]->load();
    s.lock = std::clamp ((int) std::lround (raw[P::lock]->load()), 0, 2);
    s.stamp = ++voiceClock;

    const bool glideOn = raw[P::glide]->load() > 0.0005f;
    const int mode = voiceMode();

    if (mode != Choice::poly)
    {
        const bool keyWasHeld = monoCount > 0;
        pushMonoNote (note);

        // Notes left over from poly playing are released.
        for (int v = 0; v < maxVoices; ++v)
            if (v != monoVoice && voices[(size_t) v].isActive() && voices[(size_t) v].isKeyDown())
                voices[(size_t) v].noteOff();

        auto* current = &voices[(size_t) monoVoice];

        if (mode == Choice::legato && keyWasHeld && current->isActive() && ! current->isFading())
        {
            current->legatoTo (note, velocity, accented, s.accentSweep);   // overlapping notes slide
        }
        else
        {
            s.startPitch = current->isActive() ? current->getPitch() : lastNotePitch;
            s.glideFromStart = glideOn && anyNotePlayed && mode == Choice::mono;

            if (s.lock != Choice::lockOff && current->isActive() && ! current->isFading())
            {
                // Hand over to a fresh voice: the new note starts at its locked phase, the old one
                // fades out in 5 ms instead of being cut.
                current->fastRelease();
                monoVoice = pickFreeVoice (monoVoice);
                current = &voices[(size_t) monoVoice];
            }
            current->noteOn (s);
        }
    }
    else
    {
        const int limit = polyLimit();
        dsp::Voice* target = nullptr;

        for (int v = 0; v < limit && target == nullptr; ++v)
            if (! voices[(size_t) v].isActive())
                target = &voices[(size_t) v];

        if (target == nullptr)
        {
            // Steal the quietest releasing voice, otherwise the oldest one.
            float quietest = 2.0f;
            for (int v = 0; v < limit; ++v)
            {
                auto& candidate = voices[(size_t) v];
                if (candidate.isReleasing() && candidate.getLevel() < quietest)
                {
                    quietest = candidate.getLevel();
                    target = &candidate;
                }
            }
        }

        if (target == nullptr)
        {
            uint64_t oldest = std::numeric_limits<uint64_t>::max();
            for (int v = 0; v < limit; ++v)
                if (voices[(size_t) v].getAge() < oldest)
                {
                    oldest = voices[(size_t) v].getAge();
                    target = &voices[(size_t) v];
                }
        }

        s.startPitch = lastNotePitch;
        s.glideFromStart = glideOn && anyNotePlayed;
        if (target != nullptr)
            target->noteOn (s);
    }

    lastNotePitch = (float) note;
    anyNotePlayed = true;
}

void LabX10AudioProcessor::noteOff (int note) noexcept
{
    if (voiceMode() != Choice::poly)
    {
        removeMonoNote (note);

        auto& voice = voices[(size_t) monoVoice];
        if (voice.isActive() && voice.isKeyDown() && voice.getNote() == note)
        {
            if (monoCount > 0)
            {
                const int back = monoStack[(size_t) monoCount - 1];
                voice.returnTo (back);
                lastNotePitch = (float) back;
            }
            else if (sustainDown)
                voice.setSustained (true);
            else
                voice.noteOff();
        }
        return;
    }

    for (auto& v : voices)
        if (v.isActive() && v.isKeyDown() && v.getNote() == note)
        {
            if (sustainDown)
                v.setSustained (true);
            else
                v.noteOff();
        }
}

void LabX10AudioProcessor::setSustainPedal (bool down) noexcept
{
    sustainDown = down;
    if (! down)
        for (auto& v : voices)
            if (v.isSustained())
            {
                v.setSustained (false);
                v.noteOff();
            }
}

void LabX10AudioProcessor::allNotesOff() noexcept
{
    monoCount = 0;
    sustainDown = false;
    for (auto& v : voices)
    {
        v.setSustained (false);
        if (v.isActive())
            v.noteOff();
    }
}

void LabX10AudioProcessor::allSoundOff() noexcept
{
    monoCount = 0;
    sustainDown = false;
    for (auto& v : voices)
        v.kill();
    emitter.reset();
    resonator.reset();
}

//==============================================================================
void LabX10AudioProcessor::copyScope (float* dest, int numSamples) const noexcept
{
    numSamples = std::min (numSamples, scopeSize);
    int r = (scopeWrite.load (std::memory_order_acquire) - numSamples) & (scopeSize - 1);
    for (int i = 0; i < numSamples; ++i)
    {
        dest[i] = scope[(size_t) r];
        r = (r + 1) & (scopeSize - 1);
    }
}

//==============================================================================
int LabX10AudioProcessor::getNumPrograms()
{
    return (int) factoryPresets().size();
}

int LabX10AudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

void LabX10AudioProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPrograms()))
        applyPreset (index);
}

const juce::String LabX10AudioProcessor::getProgramName (int index)
{
    const auto& presets = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String();
}

void LabX10AudioProcessor::applyPreset (int index)
{
    const auto& presets = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    // Every parameter is set once, straight to its final value, so the audio thread never
    // hears an intermediate all-defaults patch.
    const auto& preset = presets[(size_t) index];
    for (const auto& spec : paramSpecs())
    {
        auto* param = apvts.getParameter (spec.id);
        if (param == nullptr)
            continue;

        float target = param->convertFrom0to1 (param->getDefaultValue());
        for (const auto& [id, value] : preset.values)
            if (id == spec.id)
            {
                target = value;
                break;
            }
        param->setValueNotifyingHost (param->convertTo0to1 (target));
    }

    currentProgram = index;
    programChangePending.store (true);
}

//==============================================================================
void LabX10AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("userFile", specimens.getUserFile().getFullPathName(), nullptr);
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
    const auto userPath = tree.getProperty ("userFile").toString();
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));

    apvts.replaceState (tree);
    specimens.setUserFile (juce::File::isAbsolutePath (userPath) ? juce::File (userPath) : juce::File());
}

//==============================================================================
juce::AudioProcessorEditor* LabX10AudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LabX10AudioProcessor();
}
