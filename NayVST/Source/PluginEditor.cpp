#include "PluginEditor.h"
#include <cmath>

static juce::Font serifItalic (float size)
{
    return juce::Font (juce::FontOptions ("Times New Roman", size, juce::Font::italic));
}

//==============================================================================
NayLookAndFeel::NayLookAndFeel()
{
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b2b2e));
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd8d8dc));
    setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe6e6ea));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void NayLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();

    juce::ColourGradient grad (juce::Colour (0xffffe34d), r.getX(), r.getY(),
                               juce::Colour (0xffe8a800), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 7.0f);

    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 7.0f, 1.5f);

    // small "window" at the top: grey = off, blue = on
    auto slot = juce::Rectangle<float> (r.getWidth() * 0.55f, 9.0f)
                    .withCentre ({ r.getCentreX(), r.getY() + 14.0f });
    g.setColour (on ? juce::Colour (0xff2f8bff) : juce::Colour (0xff8a8a8a));
    g.fillRoundedRectangle (slot, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (slot, 2.0f, 1.0f);

    if (highlighted || down)
    {
        g.setColour (juce::Colours::white.withAlpha (down ? 0.3f : 0.15f));
        g.fillRoundedRectangle (r, 7.0f);
    }

    const float fs = b.getButtonText().length() > 4 ? 10.5f : 13.0f;
    g.setColour (juce::Colours::black);
    g.setFont (juce::Font (juce::FontOptions (fs, juce::Font::bold)));
    g.drawText (b.getButtonText(), r.withTrimmedTop (r.getHeight() * 0.55f), juce::Justification::centred);
}

void NayLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float pos, float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (3.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // tick marks
    g.setColour (juce::Colour (0xff8e8e93));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        g.drawLine (juce::Line<float> (c.getPointOnCircumference (r * 0.86f, a), c.getPointOnCircumference (r, a)), 1.2f);
    }

    // silver knob
    const float kr = r * 0.7f;
    juce::ColourGradient grad (juce::Colour (0xfff0f0f4), c.x - kr * 0.4f, c.y - kr * 0.5f,
                               juce::Colour (0xff35353b), c.x + kr * 0.8f, c.y + kr * 0.9f, true);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.5f);

    // pointer
    g.setColour (juce::Colours::black);
    g.drawLine (juce::Line<float> (c.getPointOnCircumference (kr * 0.25f, angle),
                                   c.getPointOnCircumference (kr * 0.95f, angle)), 2.5f);
}

void NayLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float minSliderPos, float maxSliderPos,
                                       juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearBarVertical && style != juce::Slider::LinearBar)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, s);
        return;
    }

    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (0.5f);
    const int v = (int) std::lround (s.getValue());

    g.setColour (juce::Colour (0xff0b0b0d));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colour (0xff55555a));
    g.drawRoundedRectangle (r, 4.0f, 1.2f);

    // note name (tiny, left)
    g.setColour (juce::Colour (0xff77777d));
    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    g.drawText (s.getName(), r.withTrimmedLeft (4.0f), juce::Justification::centredLeft);

    // value (centre)
    g.setColour (v == 0 ? juce::Colour (0xffe6e6ea) : juce::Colour (0xff6fe3dc));
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.drawText (juce::String (v), r, juce::Justification::centred);
}

//==============================================================================
void CentsBox::mouseDoubleClick (const juce::MouseEvent&)
{
    const float cur = param.convertFrom0to1 (param.getValue());
    const float target = std::abs (cur) > 0.5f ? 0.0f : -50.0f;

    param.beginChangeGesture();
    param.setValueNotifyingHost (param.convertTo0to1 (target));
    param.endChangeGesture();
}

//==============================================================================
NayAudioProcessorEditor::NayAudioProcessorEditor (NayAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    setSize (720, 442);

    // 12 fine-tune boxes
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    for (int i = 0; i < 12; ++i)
    {
        auto* param = proc.apvts.getParameter ("off" + juce::String (i));
        boxes[(size_t) i] = std::make_unique<CentsBox> (*param);

        auto& b = *boxes[(size_t) i];
        b.setName (names[i]);
        b.setSliderStyle (juce::Slider::LinearBarVertical);
        b.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        b.setSliderSnapsToMousePosition (false);
        b.setMouseDragSensitivity (300);
        addAndMakeVisible (b);

        boxAtt[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            proc.apvts, "off" + juce::String (i), b);
    }

    // mono/poly + legato
    addAndMakeVisible (monoBtn);
    addAndMakeVisible (legatoBtn);
    legatoBtn.setButtonText ("LEGATO");
    monoAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "mono", monoBtn);
    legatoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "legato", legatoBtn);
    monoBtn.onStateChange = [this] { monoBtn.setButtonText (monoBtn.getToggleState() ? "MONO" : "POLY"); };
    monoBtn.onStateChange();

    // knobs
    struct KD { const char* id; const char* name; };
    static const KD kd[11] = { { "rvLevel", "Reverb" },      { "delayLevel", "Delay" },
                               { "rvSize", "Rev Size" },     { "delayTime", "Dly Time" },
                               { "delayFb", "Dly FB" },      { "glide", "Glide" },
                               { "vibDepth", "Vib Depth" },  { "vibRate", "Vib Speed" },
                               { "attack", "Attack" },       { "release", "Release" },
                               { "gain", "Gain" } };

    for (size_t k = 0; k < 11; ++k)
    {
        auto& kn = knobs[k];
        kn.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        kn.slider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, 56, 14);

        if (k < 2) // dB knobs: show "Off" at the bottom
        {
            kn.slider.textFromValueFunction = [] (double v) { return v <= -59.9 ? juce::String ("Off") : juce::String (v, 1); };
            kn.slider.valueFromTextFunction = [] (const juce::String& t) { return t.trim().equalsIgnoreCase ("off") ? -60.0 : t.getDoubleValue(); };
        }

        addAndMakeVisible (kn.slider);

        kn.label.setText (kd[k].name, juce::dontSendNotification);
        kn.label.setJustificationType (juce::Justification::centred);
        kn.label.setColour (juce::Label::textColourId, juce::Colour (0xffe6e6ea));
        kn.label.setFont (serifItalic (k < 5 ? 15.0f : 14.0f));
        addAndMakeVisible (kn.label);

        kn.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, kd[k].id, kn.slider);
    }

    // prev / next arrows
    juce::Path left, right;
    left.addTriangle (1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.5f);
    right.addTriangle (0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f);
    prevButton.setShape (left, false, true, true);
    nextButton.setShape (right, false, true, true);
    prevButton.onClick = [this] { stepGroup (-1); };
    nextButton.onClick = [this] { stepGroup (1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    addAndMakeVisible (addButton);
    addButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Pick a sample folder (or a folder of instrument folders)", juce::File(), "");
        juce::Component::SafePointer<NayAudioProcessorEditor> safe (this);

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr)
                                      return;

                                  const auto dir = fc.getResult();
                                  if (dir.isDirectory())
                                      safe->proc.addFolder (dir);

                                  safe->refreshGroups();
                              });
    };

    addAndMakeVisible (renameButton);
    renameButton.onClick = [this]
    {
        const int idx = proc.getActiveGroup();
        if (idx < 0)
            return;

        auto* w = new juce::AlertWindow ("Rename instrument", "Enter a name:", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", proc.getGroupName (idx));
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<NayAudioProcessorEditor> safe (this);
        w->enterModalState (true,
                            juce::ModalCallbackFunction::create ([safe, w, idx] (int result)
                            {
                                if (result == 1 && safe != nullptr)
                                {
                                    safe->proc.renameGroup (idx, w->getTextEditorContents ("name"));
                                    safe->refreshGroups();
                                }
                            }),
                            true);
    };

    info.setColour (juce::Label::textColourId, juce::Colour (0xffa8a8ae));
    info.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (info);

    refreshGroups();
}

NayAudioProcessorEditor::~NayAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void NayAudioProcessorEditor::refreshGroups()
{
    info.setText (proc.getSampleInfo(), juce::dontSendNotification);
    repaint();
}

void NayAudioProcessorEditor::stepGroup (int delta)
{
    const int n = proc.getNumGroups();
    if (n == 0)
        return;

    int cur = proc.getActiveGroup();
    if (cur < 0)
        cur = delta > 0 ? -1 : 0;

    proc.selectGroup ((cur + delta + n) % n);
    refreshGroups();
}

void NayAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! lcdRect.contains (e.getPosition()) || proc.getNumGroups() == 0)
        return;

    juce::PopupMenu m;
    for (int i = 0; i < proc.getNumGroups(); ++i)
        m.addItem (i + 1, proc.getGroupName (i), true, i == proc.getActiveGroup());

    juce::Component::SafePointer<NayAudioProcessorEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
    {
        if (r > 0 && safe != nullptr)
        {
            safe->proc.selectGroup (r - 1);
            safe->refreshGroups();
        }
    });
}

//==============================================================================
static void drawPanel (juce::Graphics& g, juce::Rectangle<int> r)
{
    auto f = r.toFloat();
    g.setColour (juce::Colour (0xff141417).withAlpha (0.88f));
    g.fillRoundedRectangle (f, 8.0f);
    g.setColour (juce::Colour (0xffa4a4aa));
    g.drawRoundedRectangle (f.reduced (0.75f), 8.0f, 1.5f);
    g.setColour (juce::Colour (0xff000000).withAlpha (0.6f));
    g.drawRoundedRectangle (f.reduced (3.0f), 6.0f, 1.0f);
}

void NayAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (bgImage.isValid())
        g.drawImageAt (bgImage, 0, 0);
    else
        g.fillAll (juce::Colour (0xff1c1c1f));

    const float W = (float) getWidth();

    // title
    const auto titleArea = juce::Rectangle<float> (0.0f, 6.0f, W, 50.0f);
    g.setFont (serifItalic (38.0f));
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawText ("Gelbo Sampler", titleArea.translated (2.0f, 2.0f), juce::Justification::centred);
    g.setColour (juce::Colour (0xffd9d9df));
    g.drawText ("Gelbo Sampler", titleArea, juce::Justification::centred);

    g.setColour (juce::Colour (0xff6b6b72));
    g.drawLine (16.0f, 34.0f, W * 0.5f - 120.0f, 34.0f, 1.2f);
    g.drawLine (W * 0.5f + 120.0f, 34.0f, W - 16.0f, 34.0f, 1.2f);

    drawPanel (g, leftPanel);
    drawPanel (g, qtPanel);
    drawPanel (g, bottomPanel);

    // LCD display
    auto lcd = lcdRect.toFloat();
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (lcd.expanded (3.0f), 6.0f);
    juce::ColourGradient lg (juce::Colour (0xffb4f2ec), lcd.getX(), lcd.getY(),
                             juce::Colour (0xff5cc3be), lcd.getX(), lcd.getBottom(), false);
    g.setGradientFill (lg);
    g.fillRoundedRectangle (lcd, 4.0f);
    g.setColour (juce::Colour (0xffa4a4aa));
    g.drawRoundedRectangle (lcd.expanded (3.0f), 6.0f, 2.0f);

    const bool has = proc.getActiveGroup() >= 0;
    g.setColour (juce::Colour (0xff10302f));
    g.setFont (juce::Font (juce::FontOptions (has ? 18.0f : 14.0f, juce::Font::bold)));
    g.drawText (has ? proc.getGroupName (proc.getActiveGroup()).toUpperCase()
                    : juce::String ("NO INSTRUMENT - CLICK ADD FOLDER"),
                lcdRect.reduced (10, 0), juce::Justification::centred);

    // Prev / Next captions
    g.setColour (juce::Colour (0xffe6e6ea));
    g.setFont (serifItalic (13.0f));
    g.drawText ("Prev", prevButton.getBounds().translated (0, 30).withWidth (30).withX (prevButton.getX() - 4),
                juce::Justification::centredLeft);
    g.drawText ("Next", nextButton.getBounds().translated (0, 30).withWidth (30).withX (nextButton.getX() - 2),
                juce::Justification::centredLeft);

    // caption of the cents panel
    g.setColour (juce::Colour (0xff9a9aa2));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("FINE TUNE (cents)  -  drag, or double-click for quarter-tone flat",
                qtPanel.withHeight (22).translated (0, 4), juce::Justification::centred);
}

void NayAudioProcessorEditor::resized()
{
    const int w = getWidth(), h = getHeight();

    // dark brushed-metal background
    bgImage = juce::Image (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), false);
    {
        juce::Graphics bg (bgImage);
        bg.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2c30), 0.0f, 0.0f,
                                                  juce::Colour (0xff101012), 0.0f, (float) h, false));
        bg.fillAll();

        juce::Random rnd (1234);
        for (int i = 0; i < (w * h) / 6; ++i)
        {
            bg.setColour (juce::Colours::white.withAlpha (rnd.nextFloat() * 0.07f));
            bg.fillRect (rnd.nextInt (w), rnd.nextInt (h), 1, 1);
        }

        bg.setColour (juce::Colour (0xff7a7a80));
        bg.drawRect (0, 0, w, h, 3);
    }

    leftPanel   = { 16, 64, 196, 214 };
    qtPanel     = { 254, 146, 450, 132 };
    bottomPanel = { 16, 290, 688, 136 };
    lcdRect     = { 254, 70, 418, 40 };

    auto place = [] (Knob& k, int x, int y, int sw, int sh, int lh)
    {
        k.slider.setBounds (x, y, sw, sh);
        k.label.setBounds (x - 6, y + sh, sw + 12, lh);
    };

    // reverb + delay (big), then three small ones
    place (knobs[0], 24, 72, 82, 92, 18);
    place (knobs[1], 114, 72, 82, 92, 18);
    place (knobs[2], 22, 192, 60, 68, 16);
    place (knobs[3], 86, 192, 60, 68, 16);
    place (knobs[4], 150, 192, 60, 68, 16);

    // LCD row
    prevButton.setBounds (224, 76, 22, 28);
    nextButton.setBounds (680, 76, 22, 28);
    renameButton.setBounds (254, 116, 80, 22);
    addButton.setBounds (340, 116, 112, 22);
    info.setBounds (458, 116, 246, 22);

    // 12 fine-tune boxes: 7 naturals in the lower row, 5 sharps (2 + 3) above
    const int bw = 48, bh = 26, step = 62;
    const int x0 = qtPanel.getX() + (qtPanel.getWidth() - (6 * step + bw)) / 2;
    const int topY = qtPanel.getY() + 34, botY = topY + bh + 26;

    static const int whitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };
    for (int i = 0; i < 7; ++i)
        boxes[(size_t) whitePc[i]]->setBounds (x0 + i * step, botY, bw, bh);

    static const int blackPc[5]    = { 1, 3, 6, 8, 10 };
    static const int blackAfter[5] = { 0, 1, 3, 4, 5 };
    for (int j = 0; j < 5; ++j)
        boxes[(size_t) blackPc[j]]->setBounds (x0 + blackAfter[j] * step + step / 2, topY, bw, bh);

    // bottom panel: MONO/POLY, LEGATO, then six knobs
    const int slot = 84;
    const int sx = bottomPanel.getX() + 12;
    monoBtn.setBounds (sx + 11, bottomPanel.getY() + 46, 54, 54);
    legatoBtn.setBounds (sx + slot + 11, bottomPanel.getY() + 46, 54, 54);

    for (int k = 5; k < 11; ++k)
        place (knobs[(size_t) k], sx + (k - 3) * slot + 4, bottomPanel.getY() + 8, 76, 92, 18);

    (void) h;
}
