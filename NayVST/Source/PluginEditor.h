#pragma once

#include "PluginProcessor.h"
#include <array>

class NayLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NayLookAndFeel();

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};

// Small number box showing a per-note fine tune in cents.
// Drag up/down to change, double-click toggles 0 <-> -50 (quarter tone flat).
class CentsBox : public juce::Slider
{
public:
    explicit CentsBox (juce::RangedAudioParameter& p) : param (p) {}
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& param;
};

class NayAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NayAudioProcessorEditor (NayAudioProcessor&);
    ~NayAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    void refreshGroups();
    void stepGroup (int delta);

    NayAudioProcessor& proc;
    NayLookAndFeel laf;

    std::array<std::unique_ptr<CentsBox>, 12> boxes;   // indexed by pitch class 0..11 (C..B)
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 12> boxAtt;

    juce::ToggleButton monoBtn, legatoBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoAtt, legatoAtt;

    std::array<Knob, 11> knobs;

    juce::ShapeButton prevButton { "prev", juce::Colour (0xff1e90ff), juce::Colour (0xff6cb8ff), juce::Colour (0xff0b5cb5) };
    juce::ShapeButton nextButton { "next", juce::Colour (0xff1e90ff), juce::Colour (0xff6cb8ff), juce::Colour (0xff0b5cb5) };
    juce::TextButton renameButton { "Rename" };
    juce::TextButton addButton { "Add Folder..." };
    juce::Label info;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Image bgImage;
    juce::Rectangle<int> leftPanel, qtPanel, bottomPanel, lcdRect;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NayAudioProcessorEditor)
};
