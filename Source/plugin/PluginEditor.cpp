#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
// Look & feel
//==============================================================================
const juce::Colour RFLook::bg      { 0xff14110f };
const juce::Colour RFLook::panel   { 0xff201c18 };
const juce::Colour RFLook::panelHi { 0xff2c2620 };
const juce::Colour RFLook::gold    { 0xfff2b632 };
const juce::Colour RFLook::green   { 0xff36a852 };
const juce::Colour RFLook::red     { 0xffd9412b };
const juce::Colour RFLook::text    { 0xffefe6d8 };
const juce::Colour RFLook::dim     { 0xff8c8274 };

static juce::Font rfFont (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

RFLook::RFLook()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, gold);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, panelHi);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a332b));
    setColour (juce::ComboBox::arrowColourId, gold);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, gold.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, bg);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, bg);
    setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedComponent::backgroundColourId, bg);
    setColour (juce::TextEditor::backgroundColourId, panelHi);
    setColour (juce::TextEditor::textColourId, text);
}

void RFLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float lw = juce::jmax (2.5f, r * 0.16f);
    const float ang = start + pos * (end - start);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - lw, r - lw, 0, start, end, true);
    g.setColour (juce::Colour (0xff3a332b));
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    juce::Path val;
    val.addCentredArc (c.x, c.y, r - lw, r - lw, 0, std::min (from, ang), std::max (from, ang), true);
    g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
    g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kr = r - lw * 2.2f;
    g.setColour (panelHi);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    juce::Path p;
    p.addRoundedRectangle (-1.5f, -kr, 3.0f, kr * 0.55f, 1.5f);
    g.setColour (text);
    g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c));
}

void RFLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                               juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style == juce::Slider::LinearVertical)
    {
        // ---- console fader: groove, dB scale, metal cap
        const float cx = (float) x + (float) w * 0.5f + ((bool) s.getProperties().getWithDefault ("dbScale", false) ? 7.0f : 0.0f);
        auto slot = juce::Rectangle<float> (cx - 3.0f, (float) y - 2.0f, 6.0f, (float) h + 4.0f);
        g.setColour (juce::Colour (0xff050403));
        g.fillRoundedRectangle (slot, 3.0f);
        g.setColour (juce::Colour (0xff2a241f));
        g.drawRoundedRectangle (slot, 3.0f, 1.0f);

        if ((bool) s.getProperties().getWithDefault ("dbScale", false))
        {
            static const float marks[] = { 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f };
            g.setFont (rfFont (9.5f, true));
            for (float m : marks)
            {
                if (m < (float) s.getMinimum() + 1.0f || m > (float) s.getMaximum()) continue;   // the bottom mark is shown as infinity
                const float py = (float) s.getPositionOfValue ((double) m);
                const bool zero = std::abs (m) < 0.01f;
                g.setColour (zero ? gold : juce::Colour (0xff9a8f80));
                g.drawLine (cx - 15.0f, py, cx - 6.0f, py, zero ? 1.6f : 1.0f);
                g.drawLine (cx + 6.0f, py, cx + 11.0f, py, 1.0f);
                g.drawText (m > 0 ? "+" + juce::String ((int) m) : juce::String ((int) m),
                            juce::Rectangle<float> ((float) x - 2.0f, py - 6.0f, cx - 16.0f - (float) x + 2.0f, 12.0f),
                            juce::Justification::centredRight, false);
            }
            const float pyMin = (float) s.getPositionOfValue (s.getMinimum());
            g.setColour (juce::Colour (0xff9a8f80));
            g.drawText (juce::CharPointer_UTF8 ("\xe2\x88\x9e"), juce::Rectangle<float> ((float) x - 2.0f, pyMin - 7.0f, cx - 16.0f - (float) x + 2.0f, 12.0f),
                        juce::Justification::centredRight, false);
        }

        const float capW = juce::jmin ((float) w * 0.62f, 38.0f), capH = 24.0f;
        auto cap = juce::Rectangle<float> (cx - capW * 0.5f, pos - capH * 0.5f, capW, capH);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (cap.translated (0.0f, 3.0f), 3.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9e6df), cap.getX(), cap.getY(),
                                                 juce::Colour (0xff5b5852), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (juce::Colour (0xff2b2925));
        g.drawRoundedRectangle (cap, 3.0f, 1.0f);
        for (float gy : { -7.0f, -4.0f, 4.0f, 7.0f })
        {
            g.setColour (juce::Colour (0x55000000));
            g.drawLine (cap.getX() + 4.0f, cap.getCentreY() + gy, cap.getRight() - 4.0f, cap.getCentreY() + gy, 1.0f);
        }
        g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId).brighter (0.2f));
        g.fillRect (cap.getX() + 2.0f, cap.getCentreY() - 1.0f, capW - 4.0f, 2.0f);
    }
    else
    {
        const float cy = (float) y + (float) h * 0.5f;
        g.setColour (juce::Colour (0xff3a332b));
        g.fillRoundedRectangle ((float) x, cy - 2.5f, (float) w, 5.0f, 2.5f);
        g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
        g.fillRoundedRectangle ((float) x, cy - 2.5f, pos - (float) x, 5.0f, 2.5f);
        g.setColour (text);
        g.fillEllipse (pos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
    }
}

void RFLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    juce::Colour onCol = b.findColour (juce::TextButton::buttonOnColourId);
    if (onCol == juce::Colour()) onCol = green;
    juce::Colour c = b.getToggleState() ? onCol : panelHi;
    if (over) c = c.brighter (0.12f);
    if (down) c = c.brighter (0.25f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colour (0xff3a332b));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void RFLook::drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (3.0f, 4.0f);
    const bool front = b.isFrontTab();
    g.setColour (front ? gold : (over ? panelHi.brighter (0.1f) : panelHi));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (front ? bg : text);
    g.setFont (rfFont (15.0f, true));
    g.drawText (b.getButtonText().toUpperCase(), r, juce::Justification::centred);
}

int RFLook::getTabButtonBestWidth (juce::TabBarButton& b, int)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (rfFont (15.0f, true), b.getButtonText().toUpperCase(), 0.0f, 0.0f);
    return juce::roundToInt (ga.getBoundingBox (0, -1, true).getWidth()) + 34;
}

juce::Font RFLook::getTextButtonFont (juce::TextButton&, int h) { return rfFont (juce::jmin (14.0f, (float) h * 0.55f), true); }
juce::Font RFLook::getComboBoxFont (juce::ComboBox&) { return rfFont (14.0f); }
juce::Font RFLook::getLabelFont (juce::Label& l) { return l.getFont(); }

//==============================================================================
// Small building blocks
//==============================================================================
struct Knob : juce::Component
{
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<APVTS::SliderAttachment> att;

    Knob (APVTS& s, const juce::String& id, const juce::String& name, juce::Colour col = RFLook::gold, bool horizontal = false)
    {
        slider.setSliderStyle (horizontal ? juce::Slider::LinearHorizontal : juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (horizontal ? juce::Slider::TextBoxRight : juce::Slider::TextBoxBelow, false, horizontal ? 44 : 70, 16);

        slider.setColour (juce::Slider::rotarySliderFillColourId, col);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (horizontal ? juce::Justification::centredLeft : juce::Justification::centred);
        label.setFont (rfFont (12.5f, true));
        label.setColour (juce::Label::textColourId, RFLook::dim);
        addAndMakeVisible (slider);
        addAndMakeVisible (label);
        att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
        slider.setDoubleClickReturnValue (true, (double) s.getParameterRange (id).convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (16));
        slider.setBounds (r);
    }
};

struct Combo : juce::Component
{
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<APVTS::ComboBoxAttachment> att;

    Combo (APVTS& s, const juce::String& id, const juce::String& name)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
            box.addItemList (p->choices, 1);
        label.setText (name, juce::dontSendNotification);
        label.setFont (rfFont (12.5f, true));
        label.setColour (juce::Label::textColourId, RFLook::dim);
        addAndMakeVisible (box);
        addAndMakeVisible (label);
        att = std::make_unique<APVTS::ComboBoxAttachment> (s, id, box);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (16));
        box.setBounds (r.removeFromTop (28));
    }
};

struct Toggle : juce::TextButton
{
    std::unique_ptr<APVTS::ButtonAttachment> att;
    Toggle (APVTS& s, const juce::String& id, const juce::String& name, juce::Colour on = RFLook::green)
        : juce::TextButton (name)
    {
        setClickingTogglesState (true);
        setColour (juce::TextButton::buttonOnColourId, on);
        att = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
    }
};

static void drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour accent = RFLook::gold)
{
    g.setColour (RFLook::panel);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    g.setColour (accent);
    g.fillRoundedRectangle ((float) r.getX() + 12.0f, (float) r.getY() + 12.0f, 4.0f, 14.0f, 2.0f);
    g.setColour (RFLook::text);
    g.setFont (rfFont (14.0f, true));
    g.drawText (title.toUpperCase(), r.getX() + 22, r.getY() + 9, r.getWidth() - 30, 20, juce::Justification::centredLeft);
}

static void layoutRow (std::vector<juce::Component*> items, juce::Rectangle<int> r, int gap = 8)
{
    if (items.empty()) return;
    const int w = (r.getWidth() - gap * ((int) items.size() - 1)) / (int) items.size();
    for (auto* c : items) { c->setBounds (r.removeFromLeft (w)); r.removeFromLeft (gap); }
}

//==============================================================================
// RIDDIM PAGE
//==============================================================================
// Segmented LED meter (green / amber / red) with a peak marker
static void drawLedMeter (juce::Graphics& g, juce::Rectangle<float> r, float level, float peak)
{
    g.setColour (juce::Colour (0xff080706));
    g.fillRoundedRectangle (r.expanded (1.5f), 2.0f);
    const int segs = juce::jmax (8, (int) (r.getHeight() / 5.0f));
    const float sh = r.getHeight() / (float) segs;
    const float lvlDb = juce::Decibels::gainToDecibels (level, -100.0f);
    const float pkDb = juce::Decibels::gainToDecibels (peak, -100.0f);
    int pkSeg = -1;
    for (int i = 0; i < segs; ++i)
    {
        const float segDb = -48.0f + 51.0f * (float) (i + 1) / (float) segs;   // -48 .. +3 dB
        juce::Colour c = segDb > -3.0f ? juce::Colour (0xffff3b2f) : (segDb > -12.0f ? juce::Colour (0xffffc23a) : juce::Colour (0xff3ad85a));
        const bool lit = lvlDb >= segDb - 51.0f / (float) segs;
        if (pkDb >= segDb - 51.0f / (float) segs) pkSeg = i;
        auto seg = juce::Rectangle<float> (r.getX(), r.getBottom() - (float) (i + 1) * sh + 0.8f, r.getWidth(), sh - 1.6f);
        g.setColour (lit ? c : c.withAlpha (0.12f));
        g.fillRect (seg);
    }
    if (pkSeg >= 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillRect (r.getX(), r.getBottom() - (float) (pkSeg + 1) * sh + 0.8f, r.getWidth(), 1.6f);
    }
}

// A button that switches one or more bool parameters together (e.g. Drums + Perc)
class LinkedToggle : public juce::TextButton, private juce::Timer
{
public:
    LinkedToggle (APVTS& s, std::vector<juce::String> paramIds, const juce::String& buttonText, juce::Colour on)
        : juce::TextButton (buttonText)
    {
        for (auto& id : paramIds) if (auto* p = s.getParameter (id)) params.push_back (p);
        setColour (juce::TextButton::buttonOnColourId, on);
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xff14110f));
        onClick = [this]
        {
            const bool v = ! getToggleState();
            for (auto* p : params) { p->beginChangeGesture(); p->setValueNotifyingHost (v ? 1.0f : 0.0f); p->endChangeGesture(); }
            setToggleState (v, juce::dontSendNotification);
        };
        timerCallback();
        startTimerHz (12);
    }
private:
    void timerCallback() override
    {
        bool v = false;
        for (auto* p : params) v |= p->getValue() > 0.5f;
        if (v != getToggleState()) setToggleState (v, juce::dontSendNotification);
    }
    std::vector<juce::RangedAudioParameter*> params;
};

class DragMidiBox : public juce::Component
{
public:
    DragMidiBox (RiddimForgeProcessor& p, std::function<int()> barsFn, std::function<unsigned()> maskFn = nullptr,
                 juce::String labelText = "DRAG MIDI TO DAW", juce::String fileSuffix = {}, bool forceOnParts = false)
        : proc (p), bars (std::move (barsFn)), partMask (std::move (maskFn)), label (std::move (labelText)), suffix (std::move (fileSuffix)), forceOn (forceOnParts)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Drag this onto a track in your DAW");
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (RFLook::panelHi);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (RFLook::green);
        g.drawRoundedRectangle (r, 6.0f, 1.5f);
        g.setColour (RFLook::text);
        g.setFont (rfFont (r.getHeight() < 30 ? 11.0f : 13.0f, true));
        g.drawText (label, r, juce::Justification::centred);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        dragging = true;
        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (proc.riddimFileName (suffix));
        file.deleteFile();
        juce::FileOutputStream out (file);
        if (out.openedOk())
        {
            proc.createRiddimMidi (bars(), partMask ? partMask() : 0xffu, forceOn).writeTo (out);
            out.flush();
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                        [this] { dragging = false; });
        }
        else dragging = false;
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    void setTooltip (const juce::String&) {}

private:
    RiddimForgeProcessor& proc;
    std::function<int()> bars;
    std::function<unsigned()> partMask;
    juce::String label, suffix;
    bool forceOn = false;
    bool dragging = false;
};

static int barsFromCombo (const juce::ComboBox& c)
{
    static const int b[] = { 4, 8, 16, 32 };
    return b[juce::jlimit (0, 3, c.getSelectedItemIndex())];
}

class RiddimPage : public juce::Component
{
public:
    explicit RiddimPage (RiddimForgeProcessor& p)
        : proc (p), s (p.apvts),
          style (s, ids::style, "Style"), key (s, ids::key, "Key"), prog (s, ids::progression, "Chord Progression"),
          bpc (s, ids::barsPerChord, "Chord Length"), source (s, ids::chordSource, "Chord Source"),
          fills (s, ids::fills, "Fills"), target (s, ids::ch1Target, "MIDI Ch 1 / Keyboard Plays"),
          swing (s, ids::swing, "Swing"), humanize (s, ids::humanize, "Humanize"),
          dragBox (p, [this] { return barsFromCombo (barsBox); })
    {
        for (auto* c : std::initializer_list<juce::Component*> { &style, &key, &prog, &bpc, &source, &fills, &target, &swing, &humanize })
            addAndMakeVisible (c);

        const int seqParts[] = { rf::PartDrums, rf::PartPerc, rf::PartSkank, rf::PartPiano, rf::PartOrgan, rf::PartBass, rf::PartPad };
        for (int part : seqParts)
        {
            auto t = std::make_unique<Toggle> (s, ids::seq (part), rf::partName (part), RFLook::green);
            addAndMakeVisible (*t);
            partToggles.push_back (std::move (t));
        }

        barsBox.addItemList ({ "4 bars", "8 bars", "16 bars", "32 bars" }, 1);
        barsBox.setSelectedItemIndex (1, juce::dontSendNotification);
        addAndMakeVisible (barsBox);
        exportButton.onClick = [this] { exportMidi(); };
        addAndMakeVisible (exportButton);
        addAndMakeVisible (dragBox);

        info.setMultiLine (true);
        info.setReadOnly (true);
        info.setCaretVisible (false);
        info.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.5f, juce::Font::plain)));
        info.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        info.setColour (juce::TextEditor::backgroundColourId, RFLook::panel);
        info.setText (
            "MIDI CHANNELS\n"
            "Ch 1   plays the part chosen above (default: Drums & Perc)\n"
            "Ch 2   Skank Guitar   Ch 3   Piano   Ch 4   Organ\n"
            "Ch 5   Bass           Ch 6   Lead    Ch 7   Pad\n"
            "Ch 10  Drums & Percussion (see the Pads tab for the note map)\n"
            "Ch 16  Chord input: hold a chord here and set Chord Source to\n"
            "       'MIDI Ch 16' - skank, bubble, piano, bass and pad follow it.\n\n"
            "Riddim plays when your DAW transport runs (Host Sync), or on its\n"
            "own in Free Run. Exported MIDI uses the same channels, so you can\n"
            "drop it on a track, turn the riddim off and edit every note.", juce::dontSendNotification);
        addAndMakeVisible (info);
    }

    void paint (juce::Graphics& g) override
    {
        drawSection (g, groove, "Groove");
        drawSection (g, harmony, "Harmony", RFLook::green);
        drawSection (g, parts, "Parts in the riddim", RFLook::red);
        drawSection (g, exportArea, "Export");
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto top = r.removeFromTop (190);
        groove = top.removeFromLeft (top.getWidth() / 2 - 6);
        top.removeFromLeft (12);
        harmony = top;
        r.removeFromTop (12);
        auto mid = r.removeFromTop (92);
        parts = mid;
        r.removeFromTop (12);
        exportArea = r.removeFromLeft (330);
        r.removeFromLeft (12);
        info.setBounds (r);

        auto g = groove.reduced (14).withTrimmedTop (26);
        auto row1 = g.removeFromTop (46);
        layoutRow ({ &style, &fills }, row1, 10);
        g.removeFromTop (8);
        auto row2 = g;
        auto kn = row2.removeFromLeft (row2.getWidth() / 2);
        layoutRow ({ &swing, &humanize }, kn.withSizeKeepingCentre (juce::jmin (kn.getWidth(), 200), kn.getHeight()), 10);
        target.setBounds (row2.withSizeKeepingCentre (row2.getWidth() - 10, 46));

        auto h = harmony.reduced (14).withTrimmedTop (26);
        auto hr1 = h.removeFromTop (46);
        layoutRow ({ &key, &prog }, hr1, 10);
        key.setBounds (key.getBounds().withWidth (90));
        prog.setBounds (prog.getBounds().withLeft (key.getRight() + 10));
        h.removeFromTop (14);
        layoutRow ({ &bpc, &source }, h.removeFromTop (46), 10);

        auto p = parts.reduced (14).withTrimmedTop (28);
        std::vector<juce::Component*> ts;
        for (auto& t : partToggles) ts.push_back (t.get());
        layoutRow (ts, p.removeFromTop (34), 8);

        auto e = exportArea.reduced (14).withTrimmedTop (30);
        auto er = e.removeFromTop (30);
        barsBox.setBounds (er.removeFromLeft (100));
        er.removeFromLeft (8);
        exportButton.setBounds (er);
        e.removeFromTop (10);
        dragBox.setBounds (e.removeFromTop (44));
    }

private:
    void exportMidi()
    {
        chooser = std::make_unique<juce::FileChooser> ("Save riddim as MIDI",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (proc.riddimFileName()), "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File()) return;
                f = f.withFileExtension ("mid");
                f.deleteFile();
                juce::FileOutputStream out (f);
                if (out.openedOk()) proc.createRiddimMidi (barsFromCombo (barsBox)).writeTo (out);
            });
    }

    RiddimForgeProcessor& proc;
    APVTS& s;
    Combo style, key, prog, bpc, source, fills, target;
    Knob swing, humanize;
    std::vector<std::unique_ptr<Toggle>> partToggles;
    juce::ComboBox barsBox;
    juce::TextButton exportButton { "EXPORT MIDI..." };
    DragMidiBox dragBox;
    juce::TextEditor info;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> groove, harmony, parts, exportArea;
};

//==============================================================================
// MIXER PAGE
//==============================================================================
class Strip : public juce::Component
{
public:
    Strip (RiddimForgeProcessor& p, int part)
        : proc (p), idx (part),
          pan (p.apvts, ids::pan (part), "PAN", juce::Colour (0xffd9d2c5)),
          dly (p.apvts, ids::dly (part), "ECHO", RFLook::green),
          rev (p.apvts, ids::rev (part), "VERB", juce::Colour (0xff4fa3ff)),
          on (p.apvts, part <= rf::PartPerc ? std::vector<juce::String> { ids::seq (part) } : std::vector<juce::String> { ids::seq (part) }, "ON", RFLook::green),
          mute (p.apvts, ids::mute (part), "M", juce::Colour (0xffff7a1a)),
          solo (p.apvts, ids::solo (part), "S", juce::Colour (0xffffd23a))
    {
        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        fader.setColour (juce::Slider::rotarySliderFillColourId, RFLook::gold);
        fader.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff2a241f));
        fader.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0b0a08));
        fader.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff7dff8a));
        fader.getProperties().set ("dbScale", true);
        att = std::make_unique<APVTS::SliderAttachment> (p.apvts, ids::lvl (part), fader);
        fader.setDoubleClickReturnValue (true, (double) p.apvts.getParameterRange (ids::lvl (part))
            .convertFrom0to1 (p.apvts.getParameter (ids::lvl (part))->getDefaultValue()));
        for (auto* k : { &pan, &dly, &rev }) k->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        for (auto* b : { &mute, &solo }) b->setColour (juce::TextButton::textColourOnId, juce::Colour (0xff14110f));
        for (auto* c : std::initializer_list<juce::Component*> { &fader, &pan, &dly, &rev, &on, &mute, &solo }) addAndMakeVisible (c);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        // brushed dark metal channel
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2622), r.getX(), r.getY(), juce::Colour (0xff1a1714), r.getRight(), r.getY(), false));
        g.fillRoundedRectangle (r, 6.0f);
        juce::Random rnd ((juce::int64) idx + 7);
        for (int i = 0; i < 40; ++i)
        {
            g.setColour (juce::Colours::white.withAlpha (0.015f + 0.02f * rnd.nextFloat()));
            const float lx = r.getX() + rnd.nextFloat() * r.getWidth();
            g.drawVerticalLine ((int) lx, r.getY() + 4.0f, r.getBottom() - 4.0f);
        }
        g.setColour (juce::Colour (0xff3d362f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        // section separators
        g.setColour (juce::Colour (0xff0e0c0a));
        for (int sy : sepY) g.drawHorizontalLine (sy, r.getX() + 6.0f, r.getRight() - 6.0f);

        // LED meter with peak hold
        const float m = juce::jlimit (0.0f, 1.5f, proc.engine.uiMeter[idx].load());
        peak = juce::jmax (m, peak * 0.97f);
        drawLedMeter (g, meterArea.toFloat(), m, peak);

        // scribble strip
        auto tape = scribble.toFloat();
        g.setColour (juce::Colour (0xffece4cf));
        g.fillRoundedRectangle (tape, 2.0f);
        g.setColour (juce::Colour (0x22000000));
        g.drawRoundedRectangle (tape, 2.0f, 1.0f);
        g.setColour (juce::Colour (0xff1c2a6b));
        g.setFont (juce::Font (juce::FontOptions ("Marker Felt", 15.0f, juce::Font::bold)));
        g.drawFittedText (juce::String (rf::partName (idx)).toUpperCase(), scribble, juce::Justification::centred, 1);

        // channel number
        g.setColour (juce::Colour (0xff8c8274));
        g.setFont (rfFont (10.0f, true));
        g.drawText (juce::String (idx + 1), getLocalBounds().removeFromTop (16).reduced (6, 0), juce::Justification::centredLeft);
    }
    void resized() override
    {
        sepY.clear();
        auto r = getLocalBounds().reduced (6, 6);
        r.removeFromTop (12);
        on.setBounds (r.removeFromTop (24).reduced (2, 0));
        r.removeFromTop (6); sepY.push_back (r.getY());
        auto sends = r.removeFromTop (62);
        dly.setBounds (sends.removeFromLeft (sends.getWidth() / 2));
        rev.setBounds (sends);
        pan.setBounds (r.removeFromTop (58).withSizeKeepingCentre (juce::jmin (64, r.getWidth()), 58));
        r.removeFromTop (4); sepY.push_back (r.getY());
        auto ms = r.removeFromTop (26).reduced (2, 0);
        mute.setBounds (ms.removeFromLeft (ms.getWidth() / 2).reduced (2, 0));
        solo.setBounds (ms.reduced (2, 0));
        r.removeFromTop (6);
        scribble = r.removeFromBottom (26).reduced (2, 0);
        r.removeFromBottom (6);
        meterArea = r.removeFromRight (9).withTrimmedBottom (26).withTrimmedTop (6);
        r.removeFromRight (3);
        fader.setBounds (r);
    }

private:
    RiddimForgeProcessor& proc;
    int idx;
    juce::Slider fader;
    std::unique_ptr<APVTS::SliderAttachment> att;
    Knob pan, dly, rev;
    LinkedToggle on;
    Toggle mute, solo;
    juce::Rectangle<int> meterArea, scribble;
    std::vector<int> sepY;
    float peak = 0.0f;
};

class MixerPage : public juce::Component, private juce::Timer
{
public:
    explicit MixerPage (RiddimForgeProcessor& p)
        : s (p.apvts),
          delayDiv (s, ids::delayDiv, "Echo Time"), fb (s, ids::delayFb, "Feedback", RFLook::green), tone (s, ids::delayTone, "Echo Tone", RFLook::green),
          size (s, ids::revSize, "Size", RFLook::red), damp (s, ids::revDamp, "Damp", RFLook::red),
          dub (s, ids::dubFilter, "DUB FILTER"), master (s, ids::master, "Master")
    {
        for (int i = 0; i < rf::kNumParts; ++i)
        {
            strips.push_back (std::make_unique<Strip> (p, i));
            addAndMakeVisible (*strips.back());
        }
        for (auto* c : std::initializer_list<juce::Component*> { &delayDiv, &fb, &tone, &size, &damp, &dub, &master }) addAndMakeVisible (c);
        dub.label.setColour (juce::Label::textColourId, RFLook::gold);
        startTimerHz (30);
    }
    void paint (juce::Graphics& g) override
    {
        drawSection (g, echoArea, "Dub Echo", RFLook::green);
        drawSection (g, verbArea, "Reverb", RFLook::red);
        drawSection (g, masterArea, "Master");
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto fx = r.removeFromRight (250);
        r.removeFromRight (12);
        const int w = (r.getWidth() - 7 * 8) / 8;
        for (auto& st : strips) { st->setBounds (r.removeFromLeft (w)); r.removeFromLeft (8); }

        echoArea = fx.removeFromTop (180);
        fx.removeFromTop (10);
        verbArea = fx.removeFromTop (110);
        fx.removeFromTop (10);
        masterArea = fx;

        auto e = echoArea.reduced (12).withTrimmedTop (26);
        delayDiv.setBounds (e.removeFromTop (46));
        e.removeFromTop (4);
        layoutRow ({ &fb, &tone }, e, 8);
        auto v = verbArea.reduced (12).withTrimmedTop (24);
        layoutRow ({ &size, &damp }, v, 8);
        auto m = masterArea.reduced (12).withTrimmedTop (26);
        layoutRow ({ &dub, &master }, m.withSizeKeepingCentre (m.getWidth(), juce::jmin (m.getHeight(), 110)), 8);
    }

private:
    void timerCallback() override { for (auto& st : strips) st->repaint(); }

    APVTS& s;
    std::vector<std::unique_ptr<Strip>> strips;
    Combo delayDiv;
    Knob fb, tone, size, damp, dub, master;
    juce::Rectangle<int> echoArea, verbArea, masterArea;
};

//==============================================================================
// SOUNDS PAGE
//==============================================================================
class SoundsPage : public juce::Component
{
public:
    explicit SoundsPage (RiddimForgeProcessor& p)
        : s (p.apvts),
          kick (s, ids::kickTune, "Kick Tune"), snare (s, ids::snareTune, "Snare Tune"), tom (s, ids::tomTune, "Tom Tune"),
          hat (s, ids::hatTone, "Hat Tone"), kitDecay (s, ids::kitDecay, "Kit Decay"),
          percTune (s, ids::percTune, "Perc Tune", RFLook::green), percDecay (s, ids::percDecay, "Perc Decay", RFLook::green),
          skType (s, ids::skankType, "Type"), skTone (s, ids::skankTone, "Tone"), skMute (s, ids::skankMute, "Palm Mute"),
          piType (s, ids::pianoType, "Type"), piTone (s, ids::pianoTone, "Tone"),
          orType (s, ids::organType, "Type"), orRot (s, ids::organRotary, "Rotary"), orTone (s, ids::organTone, "Click"),
          baType (s, ids::bassType, "Type"), baTone (s, ids::bassTone, "Tone"),
          leType (s, ids::leadType, "Type"), leTone (s, ids::leadTone, "Tone"),
          paType (s, ids::padType, "Type"), paTone (s, ids::padTone, "Tone")
    {
        for (auto* c : std::initializer_list<juce::Component*> { &kick, &snare, &tom, &hat, &kitDecay, &percTune, &percDecay, &skType, &skTone, &skMute,
                                                                &piType, &piTone, &orType, &orRot, &orTone, &baType, &baTone, &leType, &leTone, &paType, &paTone })
            addAndMakeVisible (c);
    }
    void paint (juce::Graphics& g) override
    {
        drawSection (g, kitA, "Drum Kit");
        drawSection (g, percA, "Percussion", RFLook::green);
        drawSection (g, skA, "Skank Guitar", RFLook::red);
        drawSection (g, piA, "Piano");
        drawSection (g, orA, "Organ (Bubble)", RFLook::green);
        drawSection (g, baA, "Bass", RFLook::red);
        drawSection (g, leA, "Lead");
        drawSection (g, paA, "Pad", RFLook::green);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto row1 = r.removeFromTop (r.getHeight() / 2 - 6);
        r.removeFromTop (12);
        auto row2 = r;

        kitA = row1.removeFromLeft (row1.getWidth() * 5 / 10); row1.removeFromLeft (12);
        percA = row1.removeFromLeft ((row1.getWidth() - 12) / 2); row1.removeFromLeft (12);
        skA = row1;

        const int w = (row2.getWidth() - 4 * 12) / 5;
        piA = row2.removeFromLeft (w); row2.removeFromLeft (12);
        orA = row2.removeFromLeft (w); row2.removeFromLeft (12);
        baA = row2.removeFromLeft (w); row2.removeFromLeft (12);
        leA = row2.removeFromLeft (w); row2.removeFromLeft (12);
        paA = row2;

        auto inner = [] (juce::Rectangle<int> a) { return a.reduced (12).withTrimmedTop (26); };
        auto knobRow = [] (juce::Rectangle<int> a) { return a.withSizeKeepingCentre (a.getWidth(), juce::jmin (a.getHeight(), 104)); };
        layoutRow ({ &kick, &snare, &tom, &hat, &kitDecay }, knobRow (inner (kitA)));
        layoutRow ({ &percTune, &percDecay }, knobRow (inner (percA)));
        {
            auto a = inner (skA);
            skType.setBounds (a.removeFromTop (46));
            a.removeFromTop (4);
            layoutRow ({ &skTone, &skMute }, a.withSizeKeepingCentre (a.getWidth(), juce::jmin (a.getHeight(), 100)));
        }

        auto typed = [] (Combo& c, std::vector<juce::Component*> knobs, juce::Rectangle<int> a)
        {
            c.setBounds (a.removeFromTop (46));
            a.removeFromTop (6);
            layoutRow (knobs, a.withSizeKeepingCentre (a.getWidth(), juce::jmin (a.getHeight(), 100)));
        };
        {
            auto a = inner (orA);
            orType.setBounds (a.removeFromTop (46));
            a.removeFromTop (4);
            auto lower = a.withSizeKeepingCentre (a.getWidth(), juce::jmin (a.getHeight(), 96));
            const int half = lower.getWidth() / 2;
            orRot.setBounds (lower.removeFromLeft (half).withSizeKeepingCentre (half - 8, 46));
            orTone.setBounds (lower);
        }
        typed (piType, { &piTone }, inner (piA));
        typed (baType, { &baTone }, inner (baA));
        typed (leType, { &leTone }, inner (leA));
        typed (paType, { &paTone }, inner (paA));
    }

private:
    APVTS& s;
    Knob kick, snare, tom, hat, kitDecay, percTune, percDecay;
    Combo skType;
    Knob skTone, skMute;
    Combo piType; Knob piTone;
    Combo orType, orRot; Knob orTone;
    Combo baType; Knob baTone;
    Combo leType; Knob leTone;
    Combo paType; Knob paTone;
    juce::Rectangle<int> kitA, percA, skA, piA, orA, baA, leA, paA;
};

//==============================================================================
// PADS PAGE
//==============================================================================
class PadButton : public juce::Component
{
public:
    PadButton (juce::MidiKeyboardState& ks, int midiNote, juce::Colour c) : state (ks), note (midiNote), colour (c)
    {
        name = rf::drumMap (note).name;
        noteText = juce::MidiMessage::getMidiNoteName (note, true, true, 3) + "  #" + juce::String (note);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (RFLook::panelHi.interpolatedWith (colour, 0.18f + 0.6f * flash));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (colour.withAlpha (0.9f));
        g.fillRoundedRectangle (r.removeFromLeft (4.0f), 2.0f);
        auto t = getLocalBounds().reduced (10, 4);
        g.setColour (RFLook::text);
        g.setFont (rfFont (13.0f, true));
        g.drawFittedText (name, t.removeFromTop (t.getHeight() * 6 / 10), juce::Justification::bottomLeft, 1);
        g.setColour (RFLook::dim);
        g.setFont (rfFont (11.0f));
        g.drawText (noteText, t, juce::Justification::topLeft);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const float vel = juce::jlimit (0.3f, 1.0f, 0.55f + 0.45f * (1.0f - (float) e.y / (float) juce::jmax (1, getHeight())));
        state.noteOn (10, note, vel);
        flash = 1.0f;
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { state.noteOff (10, note, 0.0f); }
    int getNote() const { return note; }
    bool decay()
    {
        if (flash <= 0.0f) return false;
        flash = juce::jmax (0.0f, flash - 0.12f);
        return true;
    }

private:
    juce::MidiKeyboardState& state;
    int note;
    juce::Colour colour;
    juce::String name, noteText;
public:
    float flash = 0.0f;
};

class PadsPage : public juce::Component, private juce::Timer, private juce::MidiKeyboardState::Listener
{
public:
    explicit PadsPage (RiddimForgeProcessor& p) : state (p.keyboardState)
    {
        for (int n = 35; n <= 94; ++n)
        {
            const auto& m = rf::drumMap (n);
            if (m.type < 0) continue;
            juce::Colour c = RFLook::gold;
            if (n >= 84 && n <= 87) c = RFLook::red;
            else if (n >= 93) c = RFLook::green;
            else if (n >= 88 || n == 58) c = juce::Colour (0xffb06cf0);
            else if (n >= 60) c = RFLook::green;
            else if (n == 42 || n == 44 || n == 46 || n == 49 || n == 51 || n == 52 || n == 53 || n == 55 || n == 57 || n == 59) c = juce::Colour (0xffe6d38a);
            pads.push_back (std::make_unique<PadButton> (state, n, c));
            addAndMakeVisible (*pads.back());
        }
        hint.setText ("Click a pad to play it (click lower on the pad for softer hits). Same sounds on MIDI channel 10 - "
                      "gold = kit, cream = cymbals, green = percussion & timbales, red = nyabinghi, purple = dub FX.",
                      juce::dontSendNotification);
        hint.setFont (rfFont (13.0f));
        hint.setColour (juce::Label::textColourId, RFLook::dim);
        addAndMakeVisible (hint);
        state.addListener (this);
        startTimerHz (30);
    }
    ~PadsPage() override { state.removeListener (this); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        hint.setBounds (r.removeFromBottom (24));
        r.removeFromBottom (6);
        const int cols = 10;
        const int rows = ((int) pads.size() + cols - 1) / cols;
        const int w = r.getWidth() / cols, h = r.getHeight() / juce::jmax (1, rows);
        for (size_t i = 0; i < pads.size(); ++i)
            pads[i]->setBounds (r.getX() + (int) (i % cols) * w, r.getY() + (int) (i / cols) * h, w, h);
    }

private:
    void handleNoteOn (juce::MidiKeyboardState*, int ch, int note, float) override
    {
        if (ch != 10) return;
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<PadsPage> (this), note]
        {
            if (safe == nullptr) return;
            for (auto& p : safe->pads)
                if (p->getNote() == note) p->flash = 1.0f;
        });
    }
    void handleNoteOff (juce::MidiKeyboardState*, int, int, float) override {}
    void timerCallback() override { for (auto& p : pads) if (p->decay()) p->repaint(); }

    juce::MidiKeyboardState& state;
    std::vector<std::unique_ptr<PadButton>> pads;
    juce::Label hint;
};

//==============================================================================
// DRUMS PAGE (multi-mic kit, Handy Drums style)
//==============================================================================
class MicStrip : public juce::Component
{
public:
    MicStrip (RiddimForgeProcessor& p, int bus)
        : proc (p), idx (bus),
          mute (p.apvts, ids::micMute (bus), "M", juce::Colour (0xffff7a1a)),
          solo (p.apvts, ids::micSolo (bus), "S", juce::Colour (0xffffd23a))
    {
        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 14);
        fader.setColour (juce::Slider::rotarySliderFillColourId, bus >= rf::BusOverheads ? RFLook::green : RFLook::gold);
        fader.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        fader.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff7dff8a));
        fader.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0b0a08));
        fAtt = std::make_unique<APVTS::SliderAttachment> (p.apvts, ids::micLvl (bus), fader);
        for (auto* b : { &mute, &solo }) b->setColour (juce::TextButton::textColourOnId, juce::Colour (0xff14110f));
        fader.setDoubleClickReturnValue (true, bus == rf::BusRoom ? -4.0 : 0.0);
        pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        pan.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        pan.setColour (juce::Slider::rotarySliderFillColourId, RFLook::text.withAlpha (0.8f));
        pan.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        pAtt = std::make_unique<APVTS::SliderAttachment> (p.apvts, ids::micPan (bus), pan);
        pan.setDoubleClickReturnValue (true, 0.0);
        for (auto* c : std::initializer_list<juce::Component*> { &fader, &pan, &mute, &solo }) addAndMakeVisible (c);
    }
    void paint (juce::Graphics& g) override
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2622), 0, 0, juce::Colour (0xff1a1714), (float) getWidth(), 0, false));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setColour (juce::Colour (0xff3d362f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (RFLook::text);
        g.setFont (rfFont (11.0f, true));
        g.drawFittedText (juce::String (rf::micBusName (idx)).toUpperCase(), getLocalBounds().removeFromTop (30).reduced (2, 2),
                          juce::Justification::centred, 2);
        const float m = juce::jlimit (0.0f, 1.5f, proc.engine.getSampler().busMeter[idx].load());
        peak = juce::jmax (m, peak * 0.97f);
        drawLedMeter (g, meter.toFloat(), m, peak);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (3);
        r.removeFromTop (28);
        auto ms = r.removeFromBottom (22);
        mute.setBounds (ms.removeFromLeft (ms.getWidth() / 2).reduced (1, 0));
        solo.setBounds (ms.reduced (1, 0));
        r.removeFromBottom (2);
        pan.setBounds (r.removeFromBottom (36));
        meter = r.removeFromRight (5).withTrimmedBottom (16).withTrimmedTop (2);
        fader.setBounds (r);
    }

private:
    RiddimForgeProcessor& proc;
    int idx;
    juce::Slider fader, pan;
    std::unique_ptr<APVTS::SliderAttachment> fAtt, pAtt;
    Toggle mute, solo;
    juce::Rectangle<int> meter;
    float peak = 0.0f;
};

class DrumsPage : public juce::Component,
                  public juce::FileDragAndDropTarget,
                  private juce::Timer,
                  private juce::ChangeListener
{
public:
    explicit DrumsPage (RiddimForgeProcessor& p) : proc (p), kit (p.apvts, ids::drumKit, "Drum Kit")
    {
        addAndMakeVisible (kit);
        desc.setFont (rfFont (12.5f));
        desc.setColour (juce::Label::textColourId, RFLook::dim);
        addAndMakeVisible (desc);
        for (int b = 0; b < rf::kNumMicBuses; ++b)
        {
            strips.push_back (std::make_unique<MicStrip> (p, b));
            addAndMakeVisible (*strips.back());
        }
        for (int s = 0; s < rf::numDrumSlots(); ++s)
        {
            auto b = std::make_unique<juce::TextButton> (rf::drumSlotName (s));
            b->setClickingTogglesState (false);
            b->onClick = [this, s] { select (s); audition (s); };
            addAndMakeVisible (*b);
            slotButtons.push_back (std::move (b));
        }
        slotTitle.setFont (rfFont (16.0f, true));
        slotTitle.setColour (juce::Label::textColourId, RFLook::gold);
        addAndMakeVisible (slotTitle);
        source.setFont (rfFont (12.0f));
        source.setColour (juce::Label::textColourId, RFLook::text);
        source.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (source);
        loadBtn.onClick = [this] { chooseFile(); };
        resetBtn.onClick = [this] { proc.clearUserSample (selected); refresh(); };
        playBtn.onClick = [this] { audition (selected); };
        for (auto* b : { &loadBtn, &resetBtn, &playBtn }) addAndMakeVisible (b);
        hint.setText ("Drop a WAV/AIFF onto a drum button to replace that sound. Settings and replaced samples are saved with your song.",
                      juce::dontSendNotification);
        hint.setFont (rfFont (11.5f));
        hint.setColour (juce::Label::textColourId, RFLook::dim);
        hint.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (hint);
        proc.addChangeListener (this);
        proc.apvts.addParameterListener (ids::drumKit, &kitListener);
        kitListener.fn = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<DrumsPage> (this)] { if (safe) safe->refresh(); }); };
        select (0);
        startTimerHz (30);
    }
    ~DrumsPage() override
    {
        proc.removeChangeListener (this);
        proc.apvts.removeParameterListener (ids::drumKit, &kitListener);
    }

    void paint (juce::Graphics& g) override
    {
        drawSection (g, mixerArea, "Mic Mixer");
        drawSection (g, slotArea, "Drums", RFLook::green);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto top = r.removeFromTop (48);
        kit.setBounds (top.removeFromLeft (220));
        top.removeFromLeft (14);
        desc.setBounds (top.withTrimmedTop (16));
        r.removeFromTop (8);
        slotArea = r.removeFromRight (318);
        r.removeFromRight (10);
        mixerArea = r;

        auto m = mixerArea.reduced (10).withTrimmedTop (26);
        const int w = m.getWidth() / rf::kNumMicBuses;
        for (auto& s : strips) { s->setBounds (m.removeFromLeft (w).reduced (2, 0)); }

        auto sa = slotArea.reduced (10).withTrimmedTop (26);
        const int rows = (rf::numDrumSlots() + 1) / 2;
        auto grid = sa.removeFromTop (rows * 25);
        const int bw = grid.getWidth() / 2;
        for (int i = 0; i < (int) slotButtons.size(); ++i)
            slotButtons[(size_t) i]->setBounds (grid.getX() + (i % 2) * bw, grid.getY() + (i / 2) * 25, bw - 4, 23);
        sa.removeFromTop (8);
        slotTitle.setBounds (sa.removeFromTop (22));
        source.setBounds (sa.removeFromTop (32));
        auto knobs = sa.removeFromTop (78);
        if (tune) layoutRow ({ tune.get(), level.get(), decay.get() }, knobs, 4);
        sa.removeFromTop (4);
        auto br = sa.removeFromTop (28);
        layoutRow ({ &loadBtn, &resetBtn, &playBtn }, br, 6);
        sa.removeFromTop (4);
        hint.setBounds (sa);
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files) if (kitloader::isSupportedAudioFile (f)) return true;
        return false;
    }
    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        int target = selected;
        for (int i = 0; i < (int) slotButtons.size(); ++i)
            if (slotButtons[(size_t) i]->getBounds().contains (x, y)) target = i;
        for (auto& f : files)
            if (kitloader::isSupportedAudioFile (f)) { loadInto (target, juce::File (f)); break; }
    }

private:
    struct KitListener : juce::AudioProcessorValueTreeState::Listener
    {
        std::function<void()> fn;
        void parameterChanged (const juce::String&, float) override { if (fn) fn(); }
    } kitListener;

    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void timerCallback() override { for (auto& s : strips) s->repaint(); }

    int firstNote (int s) const
    {
        const auto& n = rf::drumSlotNotes (s);
        return n.empty() ? 38 : n.front();
    }
    void audition (int s)
    {
        const bool ghost = juce::String (rf::drumSlotKey (s)) == "snare_ghost";
        proc.keyboardState.noteOn (10, firstNote (s), ghost ? 0.22f : 0.85f);
        proc.keyboardState.noteOff (10, firstNote (s), 0.0f);
    }
    void select (int s)
    {
        selected = s;
        tune = std::make_unique<Knob> (proc.apvts, ids::slTune (s), "Tune");
        level = std::make_unique<Knob> (proc.apvts, ids::slLvl (s), "Level");
        decay = std::make_unique<Knob> (proc.apvts, ids::slDec (s), "Decay", RFLook::green);
        for (auto* k : { tune.get(), level.get(), decay.get() }) addAndMakeVisible (k);
        refresh();
        resized();
    }
    void chooseFile()
    {
        chooser = std::make_unique<juce::FileChooser> (juce::String ("Replace ") + rf::drumSlotName (selected),
            juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.caf;*.m4a");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) loadInto (selected, fc.getResult()); });
    }
    void loadInto (int s, const juce::File& f)
    {
        const auto err = proc.loadUserSample (s, f);
        if (err.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load sample", err);
        select (s);
        audition (s);
    }
    void refresh()
    {
        const auto* k = proc.currentKit();
        desc.setText (! proc.kitsReady() ? juce::String ("Loading kits...")
                      : (k ? juce::String (k->description) : juce::String ("Synthesized drums (no samples)")), juce::dontSendNotification);
        for (int s = 0; s < (int) slotButtons.size(); ++s)
        {
            auto& b = *slotButtons[(size_t) s];
            const bool user = proc.userSampleName (s).isNotEmpty();
            const bool inKit = k != nullptr && k->findArtic (rf::drumSlotKey (s)) >= 0;
            juce::Colour c = user ? RFLook::gold.darker (0.2f) : (inKit ? RFLook::green.darker (0.35f) : RFLook::panelHi);
            if (s == selected) c = c.brighter (0.35f);
            b.setColour (juce::TextButton::buttonColourId, c);
            b.setColour (juce::TextButton::buttonOnColourId, c);
            b.setToggleState (true, juce::dontSendNotification);
            b.setColour (juce::TextButton::textColourOnId, user ? RFLook::bg : RFLook::text);
        }
        const auto user = proc.userSampleName (selected);
        const bool inKit = k != nullptr && k->findArtic (rf::drumSlotKey (selected)) >= 0;
        slotTitle.setText (rf::drumSlotName (selected), juce::dontSendNotification);
        juce::String srcText;
        if (user.isNotEmpty()) srcText = "Your sample: " + user;
        else if (inKit)
        {
            const auto& a = k->artics[(size_t) k->findArtic (rf::drumSlotKey (selected))];
            srcText = juce::String (k->name) + ": " + juce::String ((int) a.samples.size()) + " samples (velocity layers + round robin)";
        }
        else srcText = "Not in this kit - plays the synth sound. Load a sample to replace it.";
        source.setText (srcText, juce::dontSendNotification);
        resetBtn.setEnabled (user.isNotEmpty());
        repaint();
    }

    RiddimForgeProcessor& proc;
    Combo kit;
    juce::Label desc, slotTitle, source, hint;
    std::vector<std::unique_ptr<MicStrip>> strips;
    std::vector<std::unique_ptr<juce::TextButton>> slotButtons;
    std::unique_ptr<Knob> tune, level, decay;
    juce::TextButton loadBtn { "LOAD SAMPLE..." }, resetBtn { "RESET" }, playBtn { "PLAY" };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> mixerArea, slotArea;
    int selected = 0;
};

//==============================================================================
// GENERATOR PAGE (Drum Monkey / Unison-style pattern generator)
//==============================================================================
class PhrasePreview : public juce::Component
{
public:
    PhrasePreview (RiddimForgeProcessor& p, int gp) : proc (p), part (gp) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff15120f));
        g.fillRoundedRectangle (r, 4.0f);
        const auto* ph = proc.getPhrase();
        if (ph == nullptr || ! ph->has[(size_t) part] || ! proc.isGenerated (part))
        {
            g.setColour (RFLook::dim);
            g.setFont (rfFont (11.5f));
            g.drawText ("Style pattern (press Generate)", r, juce::Justification::centred);
            return;
        }
        const int steps = ph->steps();
        const float sw = r.getWidth() / (float) steps;
        for (int b = 0; b <= ph->bars; ++b)
        {
            g.setColour (b % 4 == 0 ? juce::Colour (0xff4a4036) : juce::Colour (0xff2c2620));
            g.drawVerticalLine ((int) (r.getX() + (float) b * 16 * sw), r.getY(), r.getBottom());
        }
        const auto& ev = ph->events[(size_t) part];
        auto rowFor = [&] (const rf::GenEvent& e) -> float
        {
            switch (e.kind)
            {
                case rf::GKDrum:
                {
                    const int n = e.note;
                    if (n == 36 || n == 35) return 0.88f;
                    if (n == 37 || n == 38 || n == 40) return 0.66f;
                    if (n == 42 || n == 44 || n == 46) return 0.16f;
                    if (n == 49 || n == 55 || n == 57) return 0.05f;
                    if (n >= 41 && n <= 50) return 0.5f;
                    return 0.32f;
                }
                case rf::GKBass:  return 0.85f - 0.06f * (float) juce::jmin (e.note, 9);
                case rf::GKLead:  return juce::jlimit (0.05f, 0.95f, 0.6f - (float) e.note / 30.0f);
                case rf::GKChordTone: return 0.7f - 0.15f * (float) e.extra;
                default: return e.note > 60 ? 0.35f : 0.55f;
            }
        };
        const juce::Colour col = part == rf::GPDrums ? RFLook::gold : (part == rf::GPBass ? RFLook::red : RFLook::green);
        for (const auto& e : ev)
        {
            const float x = r.getX() + ((float) e.step + e.offset) * sw;
            const float y = r.getY() + rowFor (e) * (r.getHeight() - 4.0f);
            const float w = e.kind == rf::GKDrum ? juce::jmax (2.0f, sw * 0.7f) : juce::jmax (2.0f, sw * e.gate);
            g.setColour (col.withAlpha (0.35f + 0.65f * e.vel));
            g.fillRect (x, y, w, 3.5f);
        }
        const long long abs = proc.engine.uiAbsStep.load();
        if (abs >= 0)
        {
            const float x = r.getX() + (float) (abs % steps) * sw;
            g.setColour (RFLook::text.withAlpha (0.7f));
            g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        }
    }

private:
    RiddimForgeProcessor& proc;
    int part;
};

class GenRow : public juce::Component
{
public:
    GenRow (RiddimForgeProcessor& p, int gp)
        : proc (p), part (gp),
          type (p.apvts, ids::genType (gp), rf::genPartName (gp)),
          cx (p.apvts, ids::genCx (gp), "Complexity", RFLook::gold, true),
          on (p.apvts, partIds (gp, true), "ON", RFLook::green),
          solo (p.apvts, partIds (gp, false), "SOLO", juce::Colour (0xffffd23a)),
          drag (p, [&p] { return p.generatorBars(); }, [gp] { return RiddimForgeProcessor::partMaskForGen (gp); }, "DRAG", rf::genPartName (gp), true),
          preview (p, gp)
    {
        genBtn.setColour (juce::TextButton::buttonOnColourId, RFLook::gold);
        genBtn.onClick = [this] { proc.generate (part, true); };
        clrBtn.onClick = [this] { proc.clearGenerated (part); };
        for (auto* c : std::initializer_list<juce::Component*> { &type, &cx, &genBtn, &on, &solo, &clrBtn, &drag, &preview }) addAndMakeVisible (c);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (RFLook::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 4);
        type.setBounds (r.removeFromLeft (170).withSizeKeepingCentre (170, 46));
        r.removeFromLeft (10);
        cx.setBounds (r.removeFromLeft (130).withSizeKeepingCentre (130, 46));
        r.removeFromLeft (6);
        auto btns = r.removeFromLeft (250);
        auto top = btns.removeFromTop (btns.getHeight() / 2).reduced (0, 2);
        genBtn.setBounds (top.removeFromLeft (112));
        top.removeFromLeft (4);
        on.setBounds (top.removeFromLeft ((top.getWidth() - 4) / 2));
        top.removeFromLeft (4);
        solo.setBounds (top);
        auto bot = btns.reduced (0, 2);
        clrBtn.setBounds (bot.removeFromLeft (112));
        bot.removeFromLeft (4);
        drag.setBounds (bot);
        r.removeFromLeft (8);
        preview.setBounds (r.reduced (0, 2));
    }
    void repaintPreview() { preview.repaint(); }

private:
    RiddimForgeProcessor& proc;
    int part;
    Combo type;
    Knob cx;
    static std::vector<juce::String> partIds (int gp, bool onParam)
    {
        std::vector<juce::String> v;
        auto add = [&] (int part) { v.push_back (onParam ? ids::seq (part) : ids::solo (part)); };
        if (gp == rf::GPDrums) { add (rf::PartDrums); add (rf::PartPerc); }
        else add (rf::genPartToRiddimPart (gp));
        return v;
    }
    juce::TextButton genBtn { "GENERATE" }, clrBtn { "USE STYLE" };
    LinkedToggle on, solo;
    DragMidiBox drag;
    PhrasePreview preview;
};

class GeneratorPage : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    explicit GeneratorPage (RiddimForgeProcessor& p)
        : proc (p), bars (p.apvts, ids::genBars, "Length"), fills (p.apvts, ids::genFillEvery, "Drum Fills"),
          fillInt (p.apvts, ids::genFillInt, "Fill Intensity", RFLook::red, true),
          playAll (p.apvts, ids::genOn, "GENERATED PARTS ON", RFLook::green),
          dragAll (p, [&p] { return p.generatorBars(); }, [&p] { return p.generatedPartsMask(); }, "DRAG ALL MIDI", "Generated", true)
    {
        genAll.setColour (juce::TextButton::buttonOnColourId, RFLook::gold);
        genAll.onClick = [this] { proc.generate (-1, true); };
        clrAll.onClick = [this] { proc.clearGenerated (-1); };
        for (auto* c : std::initializer_list<juce::Component*> { &bars, &fills, &fillInt, &playAll, &genAll, &clrAll, &dragAll }) addAndMakeVisible (c);
        for (int gp = 0; gp < rf::kNumGenParts; ++gp)
        {
            rows.push_back (std::make_unique<GenRow> (p, gp));
            addAndMakeVisible (*rows.back());
        }
        proc.addChangeListener (this);
        startTimerHz (20);
    }
    ~GeneratorPage() override { proc.removeChangeListener (this); }

    void paint (juce::Graphics& g) override { drawSection (g, topArea, "Pattern Generator"); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        topArea = r.removeFromTop (112);
        auto t = topArea.reduced (12).withTrimmedTop (24);
        bars.setBounds (t.removeFromLeft (110).withSizeKeepingCentre (110, 46)); t.removeFromLeft (8);
        fills.setBounds (t.removeFromLeft (150).withSizeKeepingCentre (150, 46)); t.removeFromLeft (8);
        fillInt.setBounds (t.removeFromLeft (140).withSizeKeepingCentre (140, 46)); t.removeFromLeft (12);
        auto b = t.removeFromLeft (170);
        genAll.setBounds (b.removeFromTop (b.getHeight() / 2).reduced (0, 2));
        clrAll.setBounds (b.reduced (0, 2));
        t.removeFromLeft (8);
        auto b2 = t.removeFromLeft (170);
        playAll.setBounds (b2.removeFromTop (b2.getHeight() / 2).reduced (0, 2));
        dragAll.setBounds (b2.reduced (0, 2));
        r.removeFromTop (8);
        const int h = r.getHeight() / rf::kNumGenParts;
        for (auto& row : rows) row->setBounds (r.removeFromTop (h).reduced (0, 3));
    }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { for (auto& row : rows) row->repaintPreview(); }
    void timerCallback() override
    {
        const long long a = proc.engine.uiAbsStep.load();
        if (a != lastAbs) { lastAbs = a; for (auto& row : rows) row->repaintPreview(); }
    }

    RiddimForgeProcessor& proc;
    Combo bars, fills;
    Knob fillInt;
    Toggle playAll;
    juce::TextButton genAll { "GENERATE ALL" }, clrAll { "ALL BACK TO STYLE" };
    DragMidiBox dragAll;
    std::vector<std::unique_ptr<GenRow>> rows;
    juce::Rectangle<int> topArea;
    long long lastAbs = -2;
};

//==============================================================================
// EDITOR
//==============================================================================
RiddimForgeEditor::RiddimForgeEditor (RiddimForgeProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&look);

    playButton = std::make_unique<LinkedToggle> (p.apvts, std::vector<juce::String> { ids::preview }, "PLAY", RFLook::gold);
    addAndMakeVisible (*playButton);
    riddimButton.setClickingTogglesState (true);
    riddimButton.setColour (juce::TextButton::buttonOnColourId, RFLook::green);
    riddimAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, ids::riddimOn, riddimButton);
    addAndMakeVisible (riddimButton);

    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (ids::runMode)))
        clockBox.addItemList (c->choices, 1);
    clockAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, ids::runMode, clockBox);
    addAndMakeVisible (clockBox);

    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 22);
    bpmSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    bpmAtt = std::make_unique<APVTS::SliderAttachment> (p.apvts, ids::freeBpm, bpmSlider);
    addAndMakeVisible (bpmSlider);
    bpmLabel.setText ("FREE BPM", juce::dontSendNotification);
    bpmLabel.setFont (rfFont (12.0f, true));
    bpmLabel.setColour (juce::Label::textColourId, RFLook::dim);
    addAndMakeVisible (bpmLabel);

    tabs.setTabBarDepth (38);
    tabs.addTab ("Riddim", RFLook::bg, new RiddimPage (p), true);
    tabs.addTab ("Drums",  RFLook::bg, new DrumsPage (p), true);
    tabs.addTab ("Generator", RFLook::bg, new GeneratorPage (p), true);
    tabs.addTab ("Mixer",  RFLook::bg, new MixerPage (p), true);
    tabs.addTab ("Sounds", RFLook::bg, new SoundsPage (p), true);
    tabs.addTab ("Pads",   RFLook::bg, new PadsPage (p), true);
    addAndMakeVisible (tabs);

    keyboard.setMidiChannel (1);
    keyboard.setOctaveForMiddleC (3);
    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (16.0f);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, RFLook::gold.withAlpha (0.7f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, RFLook::gold.withAlpha (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, RFLook::panelHi);
    addAndMakeVisible (keyboard);
    keyboardLabel.setFont (rfFont (12.0f, true));
    keyboardLabel.setColour (juce::Label::textColourId, RFLook::dim);
    keyboardLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (keyboardLabel);

    setResizable (true, true);
    setResizeLimits (1100, 700, 1700, 1150);
    setSize (1180, 760);
    startTimerHz (30);
}

RiddimForgeEditor::~RiddimForgeEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RiddimForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (RFLook::bg);

    // header
    auto head = getLocalBounds().removeFromTop (78);
    g.setColour (RFLook::panel);
    g.fillRect (head);
    const juce::Colour stripe[3] = { RFLook::red, RFLook::gold, RFLook::green };
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (stripe[i]);
        g.fillRect (0, head.getBottom() - 6 + i * 2, getWidth(), 2);
    }
    g.setColour (RFLook::gold);
    g.setFont (rfFont (30.0f, true));
    g.drawText ("RIDDIM", 18, 10, 140, 34, juce::Justification::centredLeft);
    g.setColour (RFLook::text);
    g.drawText ("FORGE", 134, 10, 140, 34, juce::Justification::centredLeft);
    g.setColour (RFLook::dim);
    g.setFont (rfFont (12.0f));
    g.drawText ("reggae drums / percussion / skank / bubble / bass / keys", 18, 44, 360, 18, juce::Justification::centredLeft);

    // step LEDs
    const int step = proc.engine.uiStep.load();
    const bool running = proc.engine.uiRunning.load();
    auto lr = ledArea;
    const int lw = lr.getWidth() / 16;
    for (int i = 0; i < 16; ++i)
    {
        auto cell = juce::Rectangle<int> (lr.getX() + i * lw, lr.getY(), lw, lr.getHeight()).reduced (2, 0).toFloat();
        juce::Colour c = (i % 4 == 0) ? juce::Colour (0xff4a4036) : juce::Colour (0xff302a24);
        if (running && i == step) c = (i == 8) ? RFLook::red : RFLook::gold;
        g.setColour (c);
        g.fillRoundedRectangle (cell, 3.0f);
    }
    g.setColour (RFLook::dim);
    g.setFont (rfFont (11.0f, true));
    g.drawText (running ? "RIDDIM RUNNING" : (proc.engine.params.riddimOn ? "PRESS PLAY (OR PLAY YOUR DAW)" : "RIDDIM OFF"),
                lr.withY (lr.getBottom() + 2).withHeight (16), juce::Justification::centredLeft);

    // chord
    rf::Chord c { proc.engine.uiChordRoot.load(), proc.engine.uiChordQuality.load() };
    g.setColour (RFLook::panelHi);
    g.fillRoundedRectangle (chordArea.toFloat(), 8.0f);
    g.setColour (RFLook::dim);
    g.setFont (rfFont (10.5f, true));
    g.drawText ("CHORD", chordArea.withHeight (16).translated (0, 3), juce::Justification::centred);
    g.setColour (RFLook::gold);
    g.setFont (rfFont (24.0f, true));
    g.drawText (rf::chordName (c), chordArea.withTrimmedTop (14), juce::Justification::centred);
}

void RiddimForgeEditor::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (78).reduced (14, 10).withTrimmedBottom (6);
    head.removeFromLeft (380);

    auto right = head.removeFromRight (450);
    playButton->setBounds (right.removeFromLeft (84).withSizeKeepingCentre (84, 34));
    right.removeFromLeft (8);
    riddimButton.setBounds (right.removeFromLeft (90).withSizeKeepingCentre (90, 34));
    right.removeFromLeft (8);
    clockBox.setBounds (right.removeFromLeft (110).withSizeKeepingCentre (110, 30));
    right.removeFromLeft (8);
    auto bpmA = right;
    bpmLabel.setBounds (bpmA.removeFromTop (16));
    bpmSlider.setBounds (bpmA.removeFromTop (28));

    head.removeFromRight (14);
    chordArea = head.removeFromRight (86);
    head.removeFromRight (14);
    ledArea = head.withHeight (22).withY (head.getY() + 6);

    auto kb = r.removeFromBottom (92);
    keyboardLabel.setBounds (kb.removeFromTop (18).reduced (14, 0));
    keyboard.setBounds (kb.reduced (10, 2).withTrimmedBottom (4));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 50.5f); // 50 white keys from C0 to C7
    tabs.setBounds (r.reduced (6, 4));
}

void RiddimForgeEditor::timerCallback()
{
    const int step = proc.engine.uiStep.load();
    rf::Chord c { proc.engine.uiChordRoot.load(), proc.engine.uiChordQuality.load() };
    const auto chord = juce::String (rf::chordName (c)) + (proc.engine.uiRunning.load() ? "r" : "s");
    if (step != lastStep || chord != lastChord)
    {
        lastStep = step;
        lastChord = chord;
        repaint (getLocalBounds().removeFromTop (78));
    }

    playButton->setButtonText (playButton->getToggleState() ? "STOP" : "PLAY");
    static const char* targets[] = { "Drums & Perc", "Skank Guitar", "Piano", "Organ", "Bass", "Lead", "Pad" };
    const int t = juce::jlimit (0, 6, juce::roundToInt (proc.apvts.getRawParameterValue (ids::ch1Target)->load()));
    keyboardLabel.setText (juce::String ("KEYBOARD / MIDI CH 1 PLAYS: ") + targets[t] + "   (change on the Riddim tab)",
                           juce::dontSendNotification);
}
