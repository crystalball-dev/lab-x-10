#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace labx10;
using namespace labx10::ui;

namespace
{
    constexpr int headerHeight = 58;
    constexpr int footerHeight = 24;
    constexpr int margin = 10;
    constexpr int sideWidth = 236;
    constexpr int gap = 8;
    constexpr int panelPad = 8;
    constexpr int titleHeight = 18;
}

//==============================================================================
LabX10AudioProcessorEditor::SpecimenControls::SpecimenControls (LabX10AudioProcessorEditor& owner,
                                                                juce::AudioProcessorValueTreeState& state)
    : source (state, "specimen_source", "SOURCE")
{
    addAndMakeVisible (source);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (libraryButton);
    addAndMakeVisible (status);

    loadButton.setTooltip ("Load your own audio file as the specimen");
    libraryButton.setTooltip ("Choose FL Studio's 'stalker sounds' folder or your STALKER SPECIMENS library folder");
    loadButton.onClick = [&owner] { owner.chooseUserFile(); };
    libraryButton.onClick = [&owner] { owner.chooseLibraryFolder(); };

    status.setFont (monoFont (9.5f));
    status.setJustificationType (juce::Justification::topLeft);
    status.setMinimumHorizontalScale (0.6f);
}

void LabX10AudioProcessorEditor::SpecimenControls::resized()
{
    auto b = getLocalBounds().reduced (2, 0);
    source.setBounds (b.removeFromTop (37));
    b.removeFromTop (4);
    auto buttons = b.removeFromTop (19);
    loadButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 2));
    buttons.removeFromLeft (4);
    libraryButton.setBounds (buttons);
    b.removeFromTop (3);
    status.setBounds (b);
}

//==============================================================================
LabX10AudioProcessorEditor::LabX10AudioProcessorEditor (LabX10AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);
    auto& state = audioProcessor.apvts;

    specimenControls = std::make_unique<SpecimenControls> (*this, state);
    addAndMakeVisible (*specimenControls);

    // Row 1: the generators
    rows.push_back ({
        { utf8 ("GENERATOR A  //  ГЕНЕРАТОР"), { { choices ("gen_a_wave", "WAVE"), 1.5f }, { knob ("gen_a_octave", "OCT"), 1.0f },
                                                 { knob ("gen_a_semi", "SEMI"), 1.0f }, { knob ("gen_a_fine", "FINE"), 1.0f },
                                                 { knob ("gen_a_shape", "SHAPE"), 1.0f }, { knob ("gen_a_level", "LEVEL"), 1.0f } } },
        { utf8 ("MONOLITH  //  МОНОЛИТ"), { { knob ("uni_voices", "VOICES"), 1.0f }, { knob ("uni_detune", "DETUNE"), 1.0f },
                                            { knob ("uni_spread", "SPREAD"), 1.0f } } },
        { "GENERATOR B  //  MODULATOR", { { choices ("gen_b_wave", "WAVE"), 1.5f }, { knob ("gen_b_ratio", "RATIO"), 1.0f },
                                          { knob ("gen_b_fm", "FM > A"), 1.0f }, { knob ("gen_b_ring", "RING"), 1.0f },
                                          { knob ("gen_b_level", "LEVEL"), 1.0f } } },
    });

    // Row 2: the foundation, the attack and the duty cycle
    rows.push_back ({
        { "SUB", { { choices ("sub_wave", "WAVE", "sub_route", "ROUTE"), 1.5f }, { knob ("sub_octave", "OCT"), 1.0f },
                   { knob ("sub_level", "LEVEL"), 1.0f }, { choices ("lock", "LOCK"), 1.4f } } },
        { "DROP", { { knob ("drop_amount", "AMOUNT"), 1.0f }, { knob ("drop_time", "TIME"), 1.0f } } },
        { "SPARK  +  NOISE", { { knob ("spark_level", "SPARK"), 1.0f }, { knob ("spark_colour", "COLOUR"), 1.0f },
                               { knob ("spark_decay", "DECAY"), 1.0f }, { knob ("noise_level", "NOISE"), 1.0f } } },
        { utf8 ("COOLDOWN  //  ОХЛАЖДЕНИЕ"), { { choices ("cool_rate", "RATE", "cool_shape", "SHAPE"), 1.5f },
                                               { knob ("cool_depth", "DEPTH"), 1.0f }, { knob ("cool_hold", "HOLD"), 1.0f } } },
    });

    // Row 3: the lab's own sounds and the filter
    rows.push_back ({
        { utf8 ("SPECIMEN  //  ОБРАЗЕЦ"), { { specimenControls.get(), 2.6f }, { choices ("specimen_mode", "MODE"), 1.4f },
                                            { knob ("specimen_level", "LEVEL"), 1.0f }, { knob ("specimen_position", "POSITION"), 1.0f },
                                            { knob ("specimen_size", "GRAIN"), 1.0f }, { knob ("specimen_density", "DENSITY"), 1.0f },
                                            { knob ("specimen_track", "KEYTRACK"), 1.0f } } },
        { "FILTER", { { choices ("filter_model", "MODEL"), 1.5f }, { knob ("filter_cutoff", "CUTOFF"), 1.0f },
                      { knob ("filter_res", "RESO"), 1.0f }, { knob ("filter_drive", "DRIVE"), 1.0f },
                      { knob ("filter_env", "ENV"), 1.0f }, { knob ("filter_keytrack", "KEYTRACK"), 1.0f },
                      { knob ("accent_amount", "ACCENT"), 1.0f }, { knob ("accent_vel", "ACC VEL"), 1.0f } } },
    });

    // Row 4: envelopes, the emitter and the regulator
    rows.push_back ({
        { "AMP ENVELOPE",    { { knob ("env1_attack", "A"), 1.0f }, { knob ("env1_decay", "D"), 1.0f },
                               { knob ("env1_sustain", "S"), 1.0f }, { knob ("env1_release", "R"), 1.0f } } },
        { "FILTER ENVELOPE", { { knob ("env2_attack", "A"), 1.0f }, { knob ("env2_decay", "D"), 1.0f },
                               { knob ("env2_sustain", "S"), 1.0f }, { knob ("env2_release", "R"), 1.0f } } },
        { utf8 ("EMITTER  //  РАДУГА"), { { choices ("emit_mode", "MODE", "emit_os", "OVERSAMPLE"), 1.5f },
                                          { knob ("emit_drive", "DRIVE"), 1.0f }, { knob ("emit_bias", "BIAS"), 1.0f },
                                          { knob ("emit_tone", "TONE"), 1.0f }, { knob ("emit_split", "SPLIT"), 1.0f },
                                          { knob ("emit_mix", "MIX"), 1.0f } } },
        { utf8 ("REGULATOR  //  РЕГУЛЯТОР"), { { knob ("reg_amount", "AMOUNT"), 1.0f }, { knob ("reg_ceiling", "CEILING"), 1.0f },
                                               { knob ("weight", "WEIGHT"), 1.0f } } },
    });

    // Row 5: modulation, the resonator and the master section
    rows.push_back ({
        { "LFO", { { choices ("lfo_shape", "SHAPE", "lfo_sync", "SYNC"), 1.5f },
                   { choices ("lfo_target", "TARGET", "lfo_retrig", "RETRIG"), 1.5f },
                   { knob ("lfo_rate", "RATE"), 1.0f }, { knob ("lfo_depth", "DEPTH"), 1.0f } } },
        { "RESONATOR", { { knob ("res_level", "LEVEL"), 1.0f }, { knob ("res_ratio", "RATIO"), 1.0f },
                         { knob ("res_feedback", "FEEDBACK"), 1.0f }, { knob ("res_vowel", "VOWEL"), 1.0f } } },
        { "MASTER", { { choices ("voice_mode", "MODE"), 1.4f }, { knob ("voices", "VOICES"), 1.0f },
                      { knob ("glide", "GLIDE"), 1.0f }, { knob ("bend_range", "BEND"), 1.0f },
                      { knob ("width", "WIDTH"), 1.0f }, { knob ("master_volume", "VOLUME"), 1.0f } } },
    });

    scorchKnob = knob ("scorch", "SCORCH", true);

    sidePanels.push_back ({ utf8 ("POWER  //  МОЩНОСТЬ"), { { &gauge, 1.0f } } });
    sidePanels.push_back ({ utf8 ("SCORCH  //  ВЫЖИГАТЕЛЬ"), { { scorchKnob, 1.0f } }, {}, true });
    sidePanels.push_back ({ "SUB MONITOR", { { &subMonitor, 1.0f } } });
    sidePanels.push_back ({ "MONITOR", { { &scopeView, 1.0f } } });
    sidePanels.push_back ({ "SYSTEM", { { &systemLabel, 1.0f } } });

    addAndMakeVisible (gauge);
    addAndMakeVisible (subMonitor);
    addAndMakeVisible (scopeView);
    systemLabel.setFont (monoFont (11.0f));
    systemLabel.setJustificationType (juce::Justification::topLeft);
    systemLabel.setColour (juce::Label::textColourId, Palette::phosphor);
    addAndMakeVisible (systemLabel);

    // Preset browser
    const auto& presets = factoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        presetBox.addItem (juce::String (i + 1).paddedLeft ('0', 2) + "  " + presets[(size_t) i].name, i + 1);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0 && index != audioProcessor.getCurrentProgram())
            audioProcessor.setCurrentProgram (index);
        lastProgram = audioProcessor.getCurrentProgram();
    };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    addAndMakeVisible (presetBox);
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    addAndMakeVisible (overlay);

    scopeBuffer.assign ((size_t) LabX10AudioProcessor::scopeSize, 0.0f);

    // Controls were built before they had a parent, so they captured the default theme's colours.
    // Re-broadcasting makes every slider rebuild its text box through the CRT look-and-feel.
    sendLookAndFeelChange();

    setSize (editorWidth, editorHeight);
    refreshFromProcessor();
    startTimerHz (30);
}

LabX10AudioProcessorEditor::~LabX10AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

LabeledKnob* LabX10AudioProcessorEditor::knob (const char* id, const char* caption, bool accent)
{
    knobs.push_back (std::make_unique<LabeledKnob> (audioProcessor.apvts, id, caption, accent));
    addAndMakeVisible (*knobs.back());
    return knobs.back().get();
}

ChoiceStack* LabX10AudioProcessorEditor::choices (const char* idA, const char* captionA, const char* idB, const char* captionB)
{
    stacks.push_back (std::make_unique<ChoiceStack> (audioProcessor.apvts, idA, captionA, idB, captionB));
    addAndMakeVisible (*stacks.back());
    return stacks.back().get();
}

//==============================================================================
void LabX10AudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    g.fillAll (Palette::background);

    // Faint grid behind everything
    g.setColour (Palette::faint.withAlpha (0.35f));
    for (int x = 0; x < bounds.getWidth(); x += 24)
        g.drawVerticalLine (x, 0.0f, (float) bounds.getHeight());
    for (int y = 0; y < bounds.getHeight(); y += 24)
        g.drawHorizontalLine (y, 0.0f, (float) bounds.getWidth());

    // Header
    auto header = bounds.withHeight (headerHeight).reduced (margin + 6, 6);
    g.setFont (monoFont (30.0f, true));
    g.setColour (Palette::phosphor.withAlpha (0.18f));
    g.drawText ("LAB X-10", header.translated (0, 1).expanded (1, 0), juce::Justification::topLeft);
    g.setColour (Palette::phosphor);
    g.drawText ("LAB X-10", header, juce::Justification::topLeft);
    g.setFont (monoFont (11.0f));
    g.setColour (Palette::dim);
    g.drawText (utf8 ("БУНКЕР X-10  //  RAINBOW EMITTER BASS GENERATOR"), header.withTrimmedTop (32), juce::Justification::topLeft);
    g.drawText (juce::String (vendorName) + "  //  v" + versionString, header.withTrimmedTop (32), juce::Justification::topRight);
    g.setColour (Palette::border);
    g.drawHorizontalLine (headerHeight - 2, (float) margin, (float) (bounds.getWidth() - margin));

    // Panels
    auto drawPanel = [&g] (const Panel& panel)
    {
        const auto r = panel.bounds.toFloat();
        g.setColour (Palette::panel.withAlpha (0.92f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (panel.accent ? Palette::red.withAlpha (0.45f) : Palette::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

        auto title = panel.bounds.withHeight (titleHeight).reduced (panelPad, 0);
        g.setFont (monoFont (10.5f, true));
        g.setColour (panel.accent ? Palette::red : Palette::phosphor.withAlpha (0.85f));
        g.drawText (panel.title, title.withTrimmedTop (3), juce::Justification::centredLeft);
        g.setColour (panel.accent ? Palette::red.withAlpha (0.3f) : Palette::faint);
        g.drawHorizontalLine (panel.bounds.getY() + titleHeight, r.getX() + 4.0f, r.getRight() - 4.0f);
    };

    for (const auto& row : rows)
        for (const auto& panel : row)
            drawPanel (panel);
    for (const auto& panel : sidePanels)
        drawPanel (panel);

    // Footer
    auto footer = bounds.withTop (bounds.getHeight() - footerHeight).reduced (margin + 6, 4);
    g.setFont (monoFont (10.0f));
    g.setColour (Palette::dim);
    g.drawText (juce::String ("OPERATION FAIRWAY  //  AGPL-3.0  //  ") + projectUrl, footer, juce::Justification::centredLeft);
    g.drawText ("SOUND SOURCES READ FROM YOUR OWN DISK  //  NO GAME AUDIO INCLUDED", footer, juce::Justification::centredRight);
}

void LabX10AudioProcessorEditor::layoutRow (std::vector<Panel>& row, juce::Rectangle<int> area, float unitWidth)
{
    float naturalTotal = 0.0f;
    for (const auto& panel : row)
    {
        float units = 0.0f;
        for (const auto& item : panel.items)
            units += item.units;
        naturalTotal += units * unitWidth + (float) (2 * panelPad);
    }

    const float spare = std::max (0.0f, (float) area.getWidth() - naturalTotal - (float) (gap * ((int) row.size() - 1)));
    const float extraPerPanel = spare / (float) row.size();

    float x = (float) area.getX();
    for (auto& panel : row)
    {
        float units = 0.0f;
        for (const auto& item : panel.items)
            units += item.units;

        const float width = units * unitWidth + (float) (2 * panelPad) + extraPerPanel;
        panel.bounds = juce::Rectangle<int> ((int) std::round (x), area.getY(), (int) std::round (width), area.getHeight());

        auto content = panel.bounds.withTrimmedTop (titleHeight + 4).reduced (panelPad, 0).withTrimmedBottom (4);
        const float contentNatural = units * unitWidth;
        float cx = (float) content.getX() + ((float) content.getWidth() - contentNatural) * 0.5f;
        for (const auto& item : panel.items)
        {
            const float w = item.units * unitWidth;
            item.component->setBounds (juce::Rectangle<int> ((int) std::round (cx), content.getY(), (int) std::round (w), content.getHeight()));
            cx += w;
        }

        x += width + (float) gap;
    }
}

void LabX10AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    overlay.setBounds (bounds);

    // Header controls
    auto header = bounds.withHeight (headerHeight).reduced (margin, 14);
    auto presetArea = header.withSizeKeepingCentre (340, 26).translated (60, -4);
    prevButton.setBounds (presetArea.removeFromLeft (26));
    nextButton.setBounds (presetArea.removeFromRight (26));
    presetBox.setBounds (presetArea.reduced (4, 0));

    auto body = bounds.withTrimmedTop (headerHeight + 4).withTrimmedBottom (footerHeight).reduced (margin, 0);
    auto side = body.removeFromRight (sideWidth);
    body.removeFromRight (gap);

    // One unit width for the whole grid: the widest row decides, so every knob is the same size.
    const int rowCount = (int) rows.size();
    const int rowHeight = (body.getHeight() - gap * (rowCount - 1)) / rowCount;
    float unitWidth = 64.0f;
    for (const auto& row : rows)
    {
        float units = 0.0f;
        for (const auto& panel : row)
            for (const auto& item : panel.items)
                units += item.units;
        const float overhead = (float) (row.size() * 2 * panelPad + (row.size() - 1) * gap);
        unitWidth = std::min (unitWidth, ((float) body.getWidth() - overhead) / units);
    }

    for (int r = 0; r < rowCount; ++r)
        layoutRow (rows[(size_t) r], body.withTrimmedTop (r * (rowHeight + gap)).withHeight (rowHeight), unitWidth);

    // Side column: POWER, SCORCH, SUB MONITOR, scope, SYSTEM (takes the rest)
    const int heights[] = { 150, 128, 72, 96, 0 };
    auto column = side;
    for (size_t i = 0; i < sidePanels.size(); ++i)
    {
        auto& panel = sidePanels[i];
        panel.bounds = heights[i] > 0 ? column.removeFromTop (heights[i]) : column;
        column.removeFromTop (gap);
        auto content = panel.bounds.withTrimmedTop (titleHeight + 4).reduced (panelPad, 4);
        for (auto& item : panel.items)
            item.component->setBounds (content);
    }

    if (scorchKnob != nullptr)
        scorchKnob->setBounds (sidePanels[1].bounds.withTrimmedTop (titleHeight + 2).reduced (62, 4));

    overlay.toFront (false);
}

//==============================================================================
void LabX10AudioProcessorEditor::refreshFromProcessor()
{
    const auto& m = audioProcessor.meters;
    const float level = juce::jlimit (0.0f, 1.0f, (m.rmsDb.load() + 48.0f) / 48.0f);
    gauge.setTarget (60.0f * m.scorch.load() + 40.0f * level);
    gauge.tick();

    subMonitor.setValues (m.subDb.load(), m.lowCorrelation.load());

    audioProcessor.copyScope (scopeBuffer.data(), (int) scopeBuffer.size());
    scopeView.setSamples (scopeBuffer.data(), (int) scopeBuffer.size());

    auto& lib = audioProcessor.getSpecimenLibrary();
    const auto status = lib.getStatus();
    specimenControls->status.setText (lib.getStatusText(), juce::dontSendNotification);
    specimenControls->status.setColour (juce::Label::textColourId,
        status == SpecimenLibrary::Status::ready ? Palette::phosphor
        : (status == SpecimenLibrary::Status::loading || status == SpecimenLibrary::Status::idle ? Palette::dim : Palette::red));

    const int program = audioProcessor.getCurrentProgram();
    if (program != lastProgram)
    {
        presetBox.setSelectedId (program + 1, juce::dontSendNotification);
        lastProgram = program;
    }

    auto& state = audioProcessor.apvts;
    const int mode = (int) std::lround (state.getRawParameterValue ("voice_mode")->load());
    const int limit = mode == Choice::poly ? (int) std::lround (state.getRawParameterValue ("voices")->load()) : 1;
    const char* modeNames[] = { "MONO", "LEGATO", "POLY" };
    const bool pack = lib.getLibraryRoot().isDirectory();
    const bool specimens = lib.getSpecimenLibraryRoot().isDirectory();

    juce::String text;
    text << "VOICES    " << m.activeVoices.load() << " / " << limit << "  " << modeNames[juce::jlimit (0, 2, mode)] << "\n"
         << "MIDI IN   " << juce::String (m.midiPerSecond.load(), 1) << " /s\n"
         << "BPM       " << juce::String (m.bpm.load(), 1) << "  " << (m.synced.load() ? "SYNC" : "FREE") << "\n"
         << "LATENCY   " << m.latency.load() << " smp\n"
         << "PEAK      " << juce::String (m.peakDb.load(), 1) << " dBFS\n"
         << "REGULATE  " << juce::String (m.gainReductionDb.load(), 1) << " dB\n"
         << "FAULTS    " << m.faults.load() << "\n"
         << "LIBRARY   " << (pack && specimens ? "PACK + SPECIMENS" : (pack ? "PACK ONLY" : (specimens ? "SPECIMENS ONLY" : "NOT FOUND"))) << "\n"
         << "DOOR      342089";
    systemLabel.setText (text, juce::dontSendNotification);
}

void LabX10AudioProcessorEditor::stepPreset (int delta)
{
    const int count = audioProcessor.getNumPrograms();
    const int next = (audioProcessor.getCurrentProgram() + delta + count) % count;
    audioProcessor.setCurrentProgram (next);
    presetBox.setSelectedId (next + 1, juce::dontSendNotification);
    lastProgram = next;
}

void LabX10AudioProcessorEditor::chooseLibraryFolder()
{
    auto& lib = audioProcessor.getSpecimenLibrary();
    chooser = std::make_unique<juce::FileChooser> ("Select the 'stalker sounds' folder or the STALKER SPECIMENS library",
                                                   lib.getLibraryRoot(), "*");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto dir = fc.getResult();
                              if (dir.isDirectory())
                                  audioProcessor.getSpecimenLibrary().setLibraryRoot (dir, true);
                          });
}

void LabX10AudioProcessorEditor::chooseUserFile()
{
    auto& lib = audioProcessor.getSpecimenLibrary();
    const auto start = lib.getUserFile().existsAsFile() ? lib.getUserFile() : lib.getLibraryRoot();
    chooser = std::make_unique<juce::FileChooser> ("Load a specimen", start, "*.wav;*.ogg;*.flac;*.aif;*.aiff");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (! file.existsAsFile())
                                  return;
                              audioProcessor.getSpecimenLibrary().setUserFile (file);
                              if (auto* param = audioProcessor.apvts.getParameter ("specimen_source"))
                                  param->setValueNotifyingHost (param->convertTo0to1 ((float) specimenUserChoice));
                          });
}
