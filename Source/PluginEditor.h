#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"

// Интерфейс EXPitch: панель 960x512, дизайн из EXPitch_Reference_Pack (Assets/).
// Подложка (panel.png) содержит всё статичное; поверх рисуются только живые элементы:
// стрелки ручек, значения Mix/Output, название пресета, состояние переключателя.
namespace expitch
{
    // координаты в логических пикселях панели 960x512 (из assets_coordinates.json)
    constexpr float kW = 960.0f, kH = 512.0f;

    // Ручка = центр диска (пивот вращения) + положение холста стрелки при угле 0.
    // Значения подогнаны по эталонному кадру EXPitch_panel_1920x1024_2x.png (совпадение до 0.25 px).
    struct KnobGeom
    {
        juce::Point<float> pivot;         // центр диска ручки
        juce::Point<float> pointerOrigin; // левый верхний угол холста стрелки (173x173) при угле 0
    };
    inline KnobGeom geomPitch()    { return { { 199.0f, 215.0f }, { 113.0f, 122.0f } }; }
    inline KnobGeom geomFormanto() { return { { 484.0f, 214.5f }, { 396.5f, 122.0f } }; }
    inline KnobGeom geomFine()     { return { { 764.5f, 215.0f }, { 679.0f, 122.0f } }; }

    inline juce::Rectangle<float> valuePitchMix()    { return { 214.108f, 307.2f, 55.768f, 35.79f }; }
    inline juce::Rectangle<float> valueFormantoMix() { return { 497.925f, 307.2f, 58.755f, 35.79f }; }
    inline juce::Rectangle<float> valueFineOutput()  { return { 782.739f, 307.2f, 65.726f, 35.79f }; }

    inline juce::Rectangle<float> presetBox()   { return { 781.743f, 14.913f, 145.394f, 49.709f }; }
    inline juce::Rectangle<float> qualityBox()  { return { 212.116f, 364.862f, 251.95f, 65.616f }; }
    inline juce::Rectangle<float> rangeBox()    { return { 498.921f, 364.862f, 251.95f, 65.616f }; }
    inline juce::Rectangle<float> toggleRect()  { return { 356.515f, 446.384f, 68.714f, 43.744f }; }
    inline juce::Rectangle<float> toggleHit()   { return { 350.0f, 440.0f, 262.0f, 56.0f }; }   // переключатель + подпись

    // левый край и центр по вертикали текста в оригинале
    constexpr float kValueTextX_Pitch = 222.0f, kValueTextX_Formanto = 505.0f, kValueTextX_Fine = 788.5f;
    constexpr float kValueTextCenterY = 323.5f;
    constexpr float kPresetTextX = 807.7f, kPresetTextCenterY = 39.7f;
    constexpr float kPresetTextMaxW = 70.0f;

    inline const char* typeface() { return "Segoe UI"; }
}

class PitcherLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PitcherLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    juce::Font getPopupMenuFont() override;

    juce::Image pointer;
};

// Ручка: вращается только стрелка (дуга и корпус запечены в подложке)
class KnobSlider : public juce::Slider
{
public:
    explicit KnobSlider (expitch::KnobGeom geometry);
    bool hitTest (int x, int y) override;       // круглая зона вокруг диска

    expitch::KnobGeom geom;                     // координаты в системе панели 960x512
};

// Текстовое значение под ручкой: тянуть мышью вверх/вниз, двойной клик = сброс, колесо
class ReadoutField : public juce::Component
{
public:
    ReadoutField (juce::RangedAudioParameter& p, std::function<juce::String (float)> fmt,
                  float textLeftInPanel, float fontHeight);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    std::function<juce::String (float)> format;
    float value = 0.0f, startNorm = 0.0f, textLeft = 0.0f, fontH = 16.0f;
};

// Невидимая кнопка-зона, открывающая меню выбора (Quality / Voice range / Default-пресет)
class MenuZone : public juce::Component
{
public:
    MenuZone (juce::RangedAudioParameter& p, juce::StringArray items, bool drawsText, float fontHeight);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setDisplayNames (juce::StringArray names) { shortNames = std::move (names); }

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::StringArray menuItems, shortNames;
    bool drawText;
    float fontH;
    int index = 0;
};

class PitcherAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PitcherAudioProcessorEditor (PitcherAudioProcessor&);
    ~PitcherAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

    void setupKnob (KnobSlider&, const char* paramId, const juce::String& suffix, int decimals);

    PitcherAudioProcessor& proc;
    PitcherLookAndFeel laf;

    juce::Image panelImg, toggleOffImg, toggleOnImg;

    KnobSlider pitchKnob { expitch::geomPitch() }, formantKnob { expitch::geomFormanto() }, fineKnob { expitch::geomFine() };
    std::unique_ptr<SliderAtt> pitchAtt, formantAtt, fineAtt;

    std::unique_ptr<ReadoutField> pitchMix, formantMix, outputField;
    std::unique_ptr<MenuZone> presetMenu, qualityMenu, rangeMenu;

    std::unique_ptr<juce::ParameterAttachment> preserveAtt;
    bool preserveOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitcherAudioProcessorEditor)
};
