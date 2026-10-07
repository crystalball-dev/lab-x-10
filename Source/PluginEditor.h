#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/CrtLookAndFeel.h"
#include "UI/Widgets.h"

class LabX10AudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LabX10AudioProcessorEditor (LabX10AudioProcessor&);
    ~LabX10AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Pulls meters, scope and status from the processor. Public so the render harness can call it.
    void refreshFromProcessor();

    static constexpr int editorWidth = 1220;
    static constexpr int editorHeight = 760;

private:
    void timerCallback() override { refreshFromProcessor(); }

    struct PanelItem
    {
        juce::Component* component;
        float units;
    };

    struct Panel
    {
        juce::String title;
        std::vector<PanelItem> items;
        juce::Rectangle<int> bounds;
        bool accent = false;
    };

    // SPECIMEN source selection, file buttons and loader status.
    class SpecimenControls final : public juce::Component
    {
    public:
        SpecimenControls (LabX10AudioProcessorEditor& owner, juce::AudioProcessorValueTreeState& state);
        void resized() override;

        labx10::ui::LabeledChoice source;
        juce::TextButton loadButton { "LOAD FILE" }, libraryButton { "LIBRARY" };
        juce::Label status;
    };

    labx10::ui::LabeledKnob* knob (const char* id, const char* caption, bool accent = false);
    labx10::ui::ChoiceStack* choices (const char* idA, const char* captionA, const char* idB = "", const char* captionB = "");
    void layoutRow (std::vector<Panel>& row, juce::Rectangle<int> area, float unitWidth);
    void stepPreset (int delta);
    void chooseLibraryFolder();
    void chooseUserFile();

    LabX10AudioProcessor& audioProcessor;
    labx10::ui::CrtLookAndFeel lookAndFeel;

    std::vector<std::unique_ptr<labx10::ui::LabeledKnob>> knobs;
    std::vector<std::unique_ptr<labx10::ui::ChoiceStack>> stacks;
    std::vector<std::vector<Panel>> rows;
    std::vector<Panel> sidePanels;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    std::unique_ptr<SpecimenControls> specimenControls;
    labx10::ui::PowerGauge gauge;
    labx10::ui::SubMonitor subMonitor;
    labx10::ui::ScopeView scopeView;
    labx10::ui::LabeledKnob* scorchKnob = nullptr;
    juce::Label systemLabel;
    labx10::ui::ScanlineOverlay overlay;
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<float> scopeBuffer;
    int lastProgram = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabX10AudioProcessorEditor)
};
