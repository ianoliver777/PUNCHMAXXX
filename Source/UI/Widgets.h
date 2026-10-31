#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Layout.h"

class PunchmaxxxProcessor;

namespace pmx
{
namespace colours
{
    inline const juce::Colour red      { 0xffe3191c };
    inline const juce::Colour redHot   { 0xffff3b30 };
    inline const juce::Colour redDeep  { 0xff7a0a0c };
    inline const juce::Colour label    { 0xffe5dcd5 };
    inline const juce::Colour screen   { 0xff0f100e };
    inline const juce::Colour panel    { 0xff1b1917 };
}

struct Fonts
{
    static juce::Font bold (float h);
    static juce::Font medium (float h);
};

class PunchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PunchLookAndFeel();
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    juce::Font getPopupMenuFont() override { return Fonts::medium (15.0f); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    juce::Font getAlertWindowTitleFont() override { return Fonts::bold (19.0f); }
    juce::Font getAlertWindowMessageFont() override { return Fonts::medium (14.0f); }
    juce::Font getAlertWindowFont() override { return Fonts::medium (13.5f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return Fonts::bold (14.0f); }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
};

//==============================================================================
/** Draws the live parts of a knob (value arc + pointer) on top of the static art. */
class KnobOverlay : public juce::Slider
{
public:
    KnobOverlay (const layout::KnobSpec& s, juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    juce::Point<float> centreInParent() const { return { spec.cx, spec.cy }; }
    float outerRadius() const { return radius; }
    juce::String valueText() const;
    bool isHot() const { return hovering || isMouseButtonDown(); }
    std::function<void()> onHotChanged;

private:
    const layout::KnobSpec spec;
    juce::RangedAudioParameter& param;
    float radius = 0.0f;
    bool hovering = false;
};

//==============================================================================
/** A row of radio buttons drawn over the art and bound to a choice parameter. */
class ChoiceGroup : public juce::Component
{
public:
    enum class Style { ModeButton, ShapeButton };
    ChoiceGroup (juce::RangedAudioParameter& p, Style style, const juce::Rectangle<int>* rects, int count,
                 juce::StringArray labels);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override { return indexAt ({ (float) x, (float) y }) >= 0; }
    int getSelected() const { return selected; }

private:
    int indexAt (juce::Point<float>) const;
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    Style style;
    std::vector<juce::Rectangle<float>> rects;  // local coords
    juce::StringArray labels;
    int selected = 0, hover = -1;
};

//==============================================================================
/** Invisible button sitting on a button that's painted in the art: adds hover/press feedback. */
class HotspotButton : public juce::Button
{
public:
    explicit HotspotButton (const juce::String& tooltip) : juce::Button (tooltip) { setTooltip (tooltip); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

//==============================================================================
class PresetNameBox : public juce::Component
{
public:
    std::function<void()> onClick;
    void setText (const juce::String& name, bool modified, bool user);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }

private:
    juce::String text;
    bool isModified = false, isUser = false, hover = false;
};

//==============================================================================
/** The black screen: scrolling input/output waveform and the gain the shaper is applying. */
class ScopeDisplay : public juce::Component
{
public:
    explicit ScopeDisplay (PunchmaxxxProcessor& p);
    void paint (juce::Graphics&) override;
    void pull();   // called from the editor timer

private:
    PunchmaxxxProcessor& proc;
    static constexpr int columns = 600;
    std::array<float, columns> inHist {}, outHist {}, gainHist {};
    int writePos = 0;
    juce::Image gridCache;
};

//==============================================================================
class LedMeter : public juce::Component
{
public:
    explicit LedMeter (PunchmaxxxProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    void pull (float dtSeconds);

private:
    void drawColumn (juce::Graphics&, juce::Rectangle<int> r, float levelDb, float peakDb);
    PunchmaxxxProcessor& proc;
    float levelDb[2] { -100.0f, -100.0f }, peakDb[2] { -100.0f, -100.0f }, peakHold[2] {};
};

//==============================================================================
class ValueBubble : public juce::Component
{
public:
    ValueBubble() { setInterceptsMouseClicks (false, false); setAlwaysOnTop (true); }
    void show (const juce::String& text, juce::Point<float> anchorTop);
    void paint (juce::Graphics&) override;

private:
    juce::String text;
};
} // namespace pmx
