#pragma once
#include "PluginProcessor.h"

class RFLook : public juce::LookAndFeel_V4
{
public:
    RFLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool over, bool down) override;
    int getTabButtonBestWidth (juce::TabBarButton&, int tabDepth) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;

    static const juce::Colour bg, panel, panelHi, gold, green, red, text, dim;
};

class RiddimForgeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RiddimForgeEditor (RiddimForgeProcessor&);
    ~RiddimForgeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    RiddimForgeProcessor& proc;
    RFLook look;

    // header controls
    std::unique_ptr<juce::TextButton> playButton;
    juce::TextButton riddimButton { "RIDDIM" };
    juce::ComboBox clockBox;
    juce::Slider bpmSlider;
    juce::Label bpmLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> riddimAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> clockAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAtt;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::MidiKeyboardComponent keyboard;
    juce::Label keyboardLabel;

    juce::Rectangle<int> ledArea, chordArea;
    int lastStep = -2;
    juce::String lastChord;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiddimForgeEditor)
};
