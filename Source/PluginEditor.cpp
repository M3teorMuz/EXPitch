#include "PluginEditor.h"

namespace
{
    const juce::Colour kValueText   (0xffa7aabe);
    const juce::Colour kPresetText  (0xffd0d2e2);

    juce::Font makeFont (float height)
    {
        return juce::Font (juce::FontOptions (expitch::typeface(), height, juce::Font::plain));
    }

    // Подбирает высоту шрифта так, чтобы эталонная строка имела ширину как в дизайне
    float fitFontHeight (const juce::String& referenceText, float targetWidth)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (makeFont (20.0f), referenceText, 0.0f, 0.0f);
        const float w = ga.getBoundingBox (0, -1, true).getWidth();
        return w > 1.0f ? 20.0f * targetWidth / w : 16.0f;
    }

    juce::Rectangle<int> toInt (juce::Rectangle<float> r) { return r.getSmallestIntegerContainer(); }
}

//==============================================================================
PitcherLookAndFeel::PitcherLookAndFeel()
{
    pointer = juce::ImageCache::getFromMemory (BinaryData::pointer_png, BinaryData::pointer_pngSize);

    setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff1b1c27));
    setColour (juce::PopupMenu::textColourId,                  juce::Colour (0xffd9dbea));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff6a50d9));
    setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
    setColour (juce::BubbleComponent::backgroundColourId,      juce::Colour (0xff292b3a));
    setColour (juce::BubbleComponent::outlineColourId,         juce::Colour (0xff77739d));
    setColour (juce::TooltipWindow::textColourId,              juce::Colour (0xffd9dbea));
}

juce::Font PitcherLookAndFeel::getPopupMenuFont() { return makeFont (19.0f); }

void PitcherLookAndFeel::drawRotarySlider (juce::Graphics& g, int, int, int, int, float sliderPos,
                                           float, float, juce::Slider& slider)
{
    if (! pointer.isValid())
        return;

    const auto* knob = dynamic_cast<KnobSlider*> (&slider);
    if (knob == nullptr)
        return;

    // угол: ноль строго вверх, диапазон +-134 градуса (как у референса -135..+132.9)
    const float angle = juce::degreesToRadians ((sliderPos - 0.5f) * 2.0f * 134.0f);

    // стрелка: холст 346x346 (@2x) -> 173x173 логических px; вращение вокруг центра диска
    const auto offset = slider.getBounds().toFloat().getPosition();
    const auto origin = knob->geom.pointerOrigin - offset;
    const auto pivot  = knob->geom.pivot - offset;

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImageTransformed (pointer,
                            juce::AffineTransform::scale (0.5f, 0.5f)
                                .translated (origin.x, origin.y)
                                .rotated (angle, pivot.x, pivot.y),
                            false);
}

//==============================================================================
KnobSlider::KnobSlider (expitch::KnobGeom g) : geom (g)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setMouseDragSensitivity (240);
    setPopupDisplayEnabled (true, true, nullptr);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

bool KnobSlider::hitTest (int x, int y)
{
    const auto c = geom.pivot - getBounds().toFloat().getPosition();
    const float dx = (float) x - c.x, dy = (float) y - c.y;
    const float rad = 78.0f;
    return dx * dx + dy * dy <= rad * rad;
}

//==============================================================================
ReadoutField::ReadoutField (juce::RangedAudioParameter& p, std::function<juce::String (float)> fmt,
                            float textLeftInPanel, float fontHeight)
    : param (p),
      attachment (p, [this] (float v) { value = v; repaint(); }),
      format (std::move (fmt)),
      textLeft (textLeftInPanel),
      fontH (fontHeight)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    attachment.sendInitialUpdate();
}

void ReadoutField::paint (juce::Graphics& g)
{
    g.setColour (kValueText);
    g.setFont (makeFont (fontH));
    const float left = textLeft - (float) getX();
    g.drawText (format (value), juce::Rectangle<float> (left, 0.0f, (float) getWidth() - left, (float) getHeight()),
                juce::Justification::centredLeft, false);
}

void ReadoutField::mouseDown (const juce::MouseEvent&)
{
    startNorm = param.convertTo0to1 (value);
    attachment.beginGesture();
}

void ReadoutField::mouseDrag (const juce::MouseEvent& e)
{
    const float sens = e.mods.isShiftDown() ? 800.0f : 200.0f;      // пикселей на полный диапазон
    const float n = juce::jlimit (0.0f, 1.0f, startNorm - (float) e.getDistanceFromDragStartY() / sens);
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (n));
}

void ReadoutField::mouseUp (const juce::MouseEvent&)
{
    attachment.endGesture();
}

void ReadoutField::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

void ReadoutField::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float n = juce::jlimit (0.0f, 1.0f, param.convertTo0to1 (value) + w.deltaY * 0.05f);
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (n));
}

//==============================================================================
MenuZone::MenuZone (juce::RangedAudioParameter& p, juce::StringArray items, bool drawsText, float fontHeight)
    : param (p),
      attachment (p, [this] (float v) { index = juce::roundToInt (v); repaint(); }),
      menuItems (std::move (items)),
      drawText (drawsText),
      fontH (fontHeight)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

void MenuZone::paint (juce::Graphics& g)
{
    if (! drawText)
        return;

    const auto& names = shortNames.isEmpty() ? menuItems : shortNames;
    const juce::String text = names[juce::jlimit (0, names.size() - 1, index)];

    // автоподбор: длинные названия слегка уменьшаются, чтобы не залезть на стрелку
    float h = fontH;
    juce::GlyphArrangement ga;
    ga.addLineOfText (makeFont (h), text, 0.0f, 0.0f);
    const float w = ga.getBoundingBox (0, -1, true).getWidth();
    if (w > expitch::kPresetTextMaxW)
        h *= expitch::kPresetTextMaxW / w;

    g.setColour (kPresetText);
    g.setFont (makeFont (h));
    const float left = expitch::kPresetTextX - (float) getX();
    g.drawText (text, juce::Rectangle<float> (left, 0.0f, (float) getWidth() - left, (float) getHeight()),
                juce::Justification::centredLeft, false);
}

void MenuZone::mouseDown (const juce::MouseEvent&)
{
    juce::PopupMenu menu;
    for (int i = 0; i < menuItems.size(); ++i)
        menu.addItem (i + 1, menuItems[i], true, i == index);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                        [this, safe = juce::Component::SafePointer<MenuZone> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                attachment.setValueAsCompleteGesture ((float) (result - 1));
                        });
}

//==============================================================================
PitcherAudioProcessorEditor::PitcherAudioProcessorEditor (PitcherAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    panelImg     = juce::ImageCache::getFromMemory (BinaryData::panel_png,      BinaryData::panel_pngSize);
    toggleOffImg = juce::ImageCache::getFromMemory (BinaryData::toggle_off_png, BinaryData::toggle_off_pngSize);
    toggleOnImg  = juce::ImageCache::getFromMemory (BinaryData::toggle_on_png,  BinaryData::toggle_on_pngSize);

    // ручки
    setupKnob (pitchKnob,   ids::pitch,   " st", 2);
    setupKnob (formantKnob, ids::formant, " st", 2);
    setupKnob (fineKnob,    ids::fine,    " ct", 1);
    pitchAtt   = std::make_unique<SliderAtt> (proc.apvts, ids::pitch,   pitchKnob);
    formantAtt = std::make_unique<SliderAtt> (proc.apvts, ids::formant, formantKnob);
    fineAtt    = std::make_unique<SliderAtt> (proc.apvts, ids::fine,    fineKnob);

    // шрифты подбираются по ширине эталонных строк из дизайна ("100%" и "Default")
    const float valueFont  = fitFontHeight ("100%", 42.5f);
    const float presetFont = fitFontHeight ("Default", 55.0f);

    auto percent = [] (float v) { return juce::String (juce::roundToInt (v)) + "%"; };
    auto decibel = [] (float v) { return (std::abs (v) < 0.05f ? juce::String ("0.0") : juce::String (v, 1)) + " dB"; };

    pitchMix    = std::make_unique<ReadoutField> (*proc.apvts.getParameter (ids::mix),    percent, expitch::kValueTextX_Pitch,    valueFont);
    formantMix  = std::make_unique<ReadoutField> (*proc.apvts.getParameter (ids::mix),    percent, expitch::kValueTextX_Formanto, valueFont);
    outputField = std::make_unique<ReadoutField> (*proc.apvts.getParameter (ids::output), decibel, expitch::kValueTextX_Fine,     valueFont);

    // меню
    juce::StringArray voiceNames, voiceShort;
    for (int i = 0; i < pitcher::kNumVoices; ++i)
    {
        juce::String n (pitcher::voiceDef (i).name);
        voiceNames.add (n);
        voiceShort.add (n.replace (" voice", ""));
    }
    presetMenu  = std::make_unique<MenuZone> (*proc.apvts.getParameter (ids::voice), voiceNames, true, presetFont);
    presetMenu->setDisplayNames (voiceShort);
    qualityMenu = std::make_unique<MenuZone> (*proc.apvts.getParameter (ids::quality),
                                              juce::StringArray { "Live (low latency)", "Smooth 2048", "Smooth 4096 (max quality)" }, false, 0.0f);
    rangeMenu   = std::make_unique<MenuZone> (*proc.apvts.getParameter (ids::range),
                                              juce::StringArray { "Low (bass/male)", "Mid", "High (female/child)" }, false, 0.0f);

    for (auto* c : { (juce::Component*) pitchMix.get(), (juce::Component*) formantMix.get(), (juce::Component*) outputField.get(),
                     (juce::Component*) presetMenu.get(), (juce::Component*) qualityMenu.get(), (juce::Component*) rangeMenu.get() })
        addAndMakeVisible (c);

    pitchMix->setBounds    (toInt (expitch::valuePitchMix()));
    formantMix->setBounds  (toInt (expitch::valueFormantoMix()));
    outputField->setBounds (toInt (expitch::valueFineOutput()));
    presetMenu->setBounds  (toInt (expitch::presetBox()));
    qualityMenu->setBounds (toInt (expitch::qualityBox()));
    rangeMenu->setBounds   (toInt (expitch::rangeBox()));

    // переключатель Preserve formants
    preserveAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (ids::preserve),
                                                               [this] (float v) { preserveOn = v > 0.5f; repaint (toInt (expitch::toggleRect())); });
    preserveAtt->sendInitialUpdate();

    setResizable (false, false);
    setSize (960, 512);
}

PitcherAudioProcessorEditor::~PitcherAudioProcessorEditor()
{
    pitchAtt.reset(); formantAtt.reset(); fineAtt.reset();
    setLookAndFeel (nullptr);
}

void PitcherAudioProcessorEditor::setupKnob (KnobSlider& k, const char*, const juce::String& suffix, int decimals)
{
    k.setTextValueSuffix (suffix);
    k.setNumDecimalPlacesToDisplay (decimals);
    k.setLookAndFeel (&laf);
    addAndMakeVisible (k);
    // квадрат вокруг диска (центр = пивот), с запасом на повёрнутую стрелку
    k.setBounds (juce::Rectangle<float> (k.geom.pivot.x - 90.0f, k.geom.pivot.y - 90.0f, 180.0f, 180.0f).getSmallestIntegerContainer());
}

void PitcherAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14151d));                 // за скруглёнными углами панели

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    if (panelImg.isValid())
        g.drawImage (panelImg, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

    // состояние переключателя: на подложке его нет, рисуем нужное (как в дизайне)
    const auto& img = preserveOn ? toggleOnImg : toggleOffImg;
    if (img.isValid())
    {
        g.drawImage (img, expitch::toggleRect(), juce::RectanglePlacement::stretchToFit);
    }
}

void PitcherAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (expitch::toggleHit().contains (e.position))
        preserveAtt->setValueAsCompleteGesture (preserveOn ? 0.0f : 1.0f);
}
