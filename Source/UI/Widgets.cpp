#include "Widgets.h"
#include "../PluginProcessor.h"
#include "BinaryData.h"

namespace pmx
{
using namespace juce;

//==============================================================================
static Typeface::Ptr boldFace()
{
    static auto t = Typeface::createSystemTypefaceFor (BinaryData::OswaldBold_ttf, BinaryData::OswaldBold_ttfSize);
    return t;
}
static Typeface::Ptr mediumFace()
{
    static auto t = Typeface::createSystemTypefaceFor (BinaryData::OswaldMedium_ttf, BinaryData::OswaldMedium_ttfSize);
    return t;
}
// Oswald's ascent+descent is ~1.48 em, so scale: Fonts::bold (h) gives glyphs of em-size h
static constexpr float kOswaldLineScale = 1.48f;
Font Fonts::bold (float h)   { return Font (FontOptions (boldFace()).withHeight (h * kOswaldLineScale)); }
Font Fonts::medium (float h) { return Font (FontOptions (mediumFace()).withHeight (h * kOswaldLineScale)); }

//==============================================================================
PunchLookAndFeel::PunchLookAndFeel()
{
    setColourScheme ({ Colour (0xff1c1a18), Colour (0xff141210), Colour (0xff221f1c), Colour (0xff4a4440),
                       colours::label, colours::red, Colours::white, colours::red, colours::label });
    setColour (PopupMenu::backgroundColourId, Colour (0xff161412));
    setColour (PopupMenu::textColourId, colours::label);
    setColour (PopupMenu::headerTextColourId, colours::red);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::redDeep);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (AlertWindow::backgroundColourId, Colour (0xff171513));
    setColour (AlertWindow::textColourId, colours::label);
    setColour (AlertWindow::outlineColourId, colours::red);
    setColour (TextEditor::backgroundColourId, Colour (0xff0e0d0c));
    setColour (TextEditor::textColourId, Colours::white);
    setColour (TextEditor::outlineColourId, Colour (0xff4a4440));
    setColour (TextEditor::focusedOutlineColourId, colours::red);
    setColour (TextEditor::highlightColourId, colours::redDeep);
    setColour (CaretComponent::caretColourId, colours::red);
    setColour (TextButton::buttonColourId, Colour (0xff2a2622));
    setColour (TextButton::textColourOffId, colours::label);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff141210));
    setColour (TooltipWindow::textColourId, colours::label);
    setColour (TooltipWindow::outlineColourId, colours::redDeep);
}

Typeface::Ptr PunchLookAndFeel::getTypefaceForFont (const Font& f)
{
    if (f.getTypefaceName() == Font::getDefaultSansSerifFontName() || f.getTypefaceName().isEmpty())
        return f.isBold() ? boldFace() : mediumFace();
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void PunchLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (findColour (PopupMenu::backgroundColourId));
    g.setColour (colours::redDeep);
    g.drawRect (0, 0, w, h, 1);
}

void PunchLookAndFeel::drawPopupMenuSectionHeader (Graphics& g, const Rectangle<int>& area, const String& name)
{
    g.setFont (Fonts::bold (14.0f));
    g.setColour (colours::red);
    g.drawFittedText (name.toUpperCase(), area.reduced (12, 0), Justification::bottomLeft, 1);
}

void PunchLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setGradientFill (ColourGradient (Colour (0xff35302b), r.getX(), r.getY(), Colour (0xff1b1815), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (over || b.hasKeyboardFocus (false) ? colours::red : Colour (0xff4a4440));
    g.drawRoundedRectangle (r, 5.0f, 1.2f);
    if (down) { g.setColour (Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (r, 5.0f); }
}

//==============================================================================
static Point<float> polar (Point<float> c, float r, float angle)
{
    return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) };
}

KnobOverlay::KnobOverlay (const layout::KnobSpec& s, RangedAudioParameter& p) : spec (s), param (p)
{
    setSliderStyle (Slider::RotaryVerticalDrag);
    setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    setRotaryParameters (layout::startAngle + MathConstants<float>::twoPi, layout::endAngle + MathConstants<float>::twoPi, true);
    setMouseDragSensitivity (260);
    setVelocityModeParameters (0.5, 1, 0.05, true, ModifierKeys::shiftModifier);
    setDoubleClickReturnValue (true, (double) p.convertFrom0to1 (p.getDefaultValue()));
    setScrollWheelEnabled (true);
    setPopupMenuEnabled (false);
    setMouseCursor (MouseCursor::PointingHandCursor);

    radius = jmax (spec.arcR + spec.arcThick * 0.5f + 3.0f, spec.ptrOuter + 4.0f);
    const int half = (int) std::ceil (radius + 6.0f);
    setBounds ((int) std::floor (spec.cx) - half, (int) std::floor (spec.cy) - half, half * 2 + 1, half * 2 + 1);
}

String KnobOverlay::valueText() const { return param.getCurrentValueAsText(); }

bool KnobOverlay::hitTest (int x, int y)
{
    const Point<float> c (spec.cx - (float) getX(), spec.cy - (float) getY());
    return c.getDistanceFrom ({ (float) x, (float) y }) <= radius + 4.0f;
}

void KnobOverlay::mouseEnter (const MouseEvent& e) { Slider::mouseEnter (e); hovering = true; if (onHotChanged) onHotChanged(); }
void KnobOverlay::mouseExit (const MouseEvent& e)  { Slider::mouseExit (e); hovering = false; if (onHotChanged) onHotChanged(); }

void KnobOverlay::paint (Graphics& g)
{
    const Point<float> c (spec.cx - (float) getX(), spec.cy - (float) getY());
    const float norm = jlimit (0.0f, 1.0f, (float) valueToProportionOfLength (getValue()));
    const float angle = layout::startAngle + norm * (layout::endAngle - layout::startAngle);

    // ---- value arc ----
    if (spec.arcThick > 0.0f)
    {
        const float from = spec.bipolar ? 0.0f : layout::startAngle;
        if (std::abs (angle - from) > 0.002f)
        {
            Path arc;
            arc.addCentredArc (c.x, c.y, spec.arcR, spec.arcR, 0.0f, jmin (from, angle), jmax (from, angle), true);

            g.setColour (colours::redHot.withAlpha (0.18f));
            g.strokePath (arc, PathStrokeType (spec.arcThick + 5.0f, PathStrokeType::curved, PathStrokeType::butt));
            g.setColour (colours::redHot.withAlpha (0.25f));
            g.strokePath (arc, PathStrokeType (spec.arcThick + 2.0f, PathStrokeType::curved, PathStrokeType::butt));

            g.setGradientFill (ColourGradient (Colour (0xffff2a22), c.x, c.y - spec.arcR, Colour (0xffb30d0f), c.x, c.y + spec.arcR, false));
            g.strokePath (arc, PathStrokeType (spec.arcThick, PathStrokeType::curved, PathStrokeType::butt));

            Path hl;
            hl.addCentredArc (c.x, c.y, spec.arcR + spec.arcThick * 0.25f, spec.arcR + spec.arcThick * 0.25f, 0.0f,
                              jmin (from, angle), jmax (from, angle), true);
            g.setColour (Colour (0x55ff9a90));
            g.strokePath (hl, PathStrokeType (1.0f));
        }
    }

    // ---- pointer ----
    const auto p1 = polar (c, spec.ptrOuter, angle);
    const auto p0 = polar (c, spec.ptrInner, angle);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawLine (Line<float> (p0.translated (1.0f, 1.2f), p1.translated (1.0f, 1.2f)), spec.ptrWidth + 1.0f);
    g.setColour (Colour (0xffc8121a));
    g.drawLine (Line<float> (p0, p1), spec.ptrWidth);
    g.setColour (Colour (0xffff4a3c));
    g.drawLine (Line<float> (p0, p1), jmax (0.8f, spec.ptrWidth * 0.35f));

    if (isHot())
    {
        g.setColour (Colours::white.withAlpha (isMouseButtonDown() ? 0.06f : 0.035f));
        g.fillEllipse (Rectangle<float> (spec.ptrOuter * 2.0f, spec.ptrOuter * 2.0f).withCentre (c));
    }
}

//==============================================================================
ChoiceGroup::ChoiceGroup (RangedAudioParameter& p, Style s, const Rectangle<int>* r, int count, StringArray l)
    : param (p),
      attachment (p, [this] (float v) { selected = roundToInt (v); repaint(); }),
      style (s), labels (std::move (l))
{
    Rectangle<int> u = r[0];
    for (int i = 1; i < count; ++i) u = u.getUnion (r[i]);
    if (style == Style::ModeButton)
        u = u.getUnion (Rectangle<int> (12, 12).withCentre (layout::modeLed.toInt()));
    u = u.expanded (8);
    setBounds (u);
    for (int i = 0; i < count; ++i)
        rects.push_back (r[i].translated (-u.getX(), -u.getY()).toFloat());
    setMouseCursor (MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

int ChoiceGroup::indexAt (Point<float> p) const
{
    for (int i = 0; i < (int) rects.size(); ++i)
        if (rects[(size_t) i].contains (p)) return i;
    return -1;
}

void ChoiceGroup::mouseMove (const MouseEvent& e) { const int h = indexAt (e.position); if (h != hover) { hover = h; repaint(); } }
void ChoiceGroup::mouseExit (const MouseEvent&) { hover = -1; repaint(); }
void ChoiceGroup::mouseDown (const MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i >= 0 && i != selected)
        attachment.setValueAsCompleteGesture ((float) i);
}

static void glowRect (Graphics& g, Rectangle<float> r, float corner, Colour c)
{
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (c.withAlpha (0.07f * (float) (5 - i)));
        g.drawRoundedRectangle (r.expanded ((float) i * 1.2f), corner + (float) i, 2.0f);
    }
    g.setColour (c);
    g.drawRoundedRectangle (r, corner, 1.6f);
}

void ChoiceGroup::paint (Graphics& g)
{
    if (style == Style::ModeButton)
    {
        // the MODE LED is always lit (one mode is always active)
        const auto led = layout::modeLed - getPosition().toFloat();
        g.setGradientFill (ColourGradient (Colour (0xffff6a5a), led.x - 1.5f, led.y - 1.5f, Colour (0xff9a0a0a), led.x + 4.0f, led.y + 4.0f, true));
        g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (led));
        g.setColour (colours::redHot.withAlpha (0.25f));
        g.fillEllipse (Rectangle<float> (17.0f, 17.0f).withCentre (led));
    }

    for (int i = 0; i < (int) rects.size(); ++i)
    {
        auto r = rects[(size_t) i].reduced (1.5f);
        const bool sel = i == selected;

        if (style == Style::ModeButton && sel)
        {
            g.setGradientFill (ColourGradient (Colour (0xff6e1012), r.getX(), r.getY(), Colour (0xff2b0506), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (Colour (0x30ffffff));
            g.drawHorizontalLine ((int) r.getY() + 2, r.getX() + 6.0f, r.getRight() - 6.0f);
            glowRect (g, r, 6.0f, colours::redHot);

            g.setFont (Fonts::bold (20.0f));
            for (auto o : { Point<float> (-1, 0), Point<float> (1, 0), Point<float> (0, -1), Point<float> (0, 1) })
            {
                g.setColour (colours::redHot.withAlpha (0.45f));
                g.drawText (labels[i], r.translated (o.x, o.y + 1.0f), Justification::centred);
            }
            g.setColour (Colours::white);
            g.drawText (labels[i], r.translated (0.0f, 1.0f), Justification::centred);
        }
        else if (style == Style::ShapeButton && sel)
        {
            g.setColour (colours::redHot.withAlpha (0.20f));
            g.fillRoundedRectangle (r, 5.0f);
            glowRect (g, r, 5.0f, colours::redHot);
        }

        if (i == hover && ! sel)
        {
            g.setColour (Colours::white.withAlpha (0.07f));
            g.fillRoundedRectangle (r, 5.0f);
        }
    }
}

//==============================================================================
void HotspotButton::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    if (down)      { g.setColour (Colours::black.withAlpha (0.3f));  g.fillRoundedRectangle (r, 5.0f); }
    else if (over) { g.setColour (Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (r, 5.0f); }
}

//==============================================================================
void PresetNameBox::setText (const String& name, bool modified, bool user)
{
    if (name == text && modified == isModified && user == isUser) return;
    text = name; isModified = modified; isUser = user;
    repaint();
}

void PresetNameBox::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f, 4.0f);
    if (hover)
    {
        g.setColour (Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (r, 4.0f);
    }
    auto textArea = r.withTrimmedLeft (7.0f).withTrimmedRight (34.0f);
    g.setFont (Fonts::medium (17.5f));
    g.setColour (Colour (0xffe8e2dc));
    String s = PresetManager::displayName (text);
    if (isModified) s << " *";
    g.drawFittedText (s, textArea.toNearestInt(), Justification::centredLeft, 1, 0.85f);
}

//==============================================================================
ScopeDisplay::ScopeDisplay (PunchmaxxxProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    setOpaque (false);
}

void ScopeDisplay::pull()
{
    auto& fifo = proc.displayFifo;
    const int ready = fifo.getNumReady();
    if (ready <= 0) return;
    const auto scope = fifo.read (ready);
    auto take = [this] (const DisplayFrame& f)
    {
        inHist[(size_t) writePos] = f.inPeak;
        outHist[(size_t) writePos] = f.outPeak;
        gainHist[(size_t) writePos] = f.gainDb;
        writePos = (writePos + 1) % columns;
    };
    for (int i = 0; i < scope.blockSize1; ++i) take (proc.displayFrames[(size_t) (scope.startIndex1 + i)]);
    for (int i = 0; i < scope.blockSize2; ++i) take (proc.displayFrames[(size_t) (scope.startIndex2 + i)]);
    repaint();
}

void ScopeDisplay::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const int scale = jmax (1, roundToInt (g.getInternalContext().getPhysicalPixelScaleFactor()));

    if (gridCache.isNull() || gridCache.getWidth() != getWidth() * scale)
    {
        gridCache = Image (Image::ARGB, getWidth() * scale, getHeight() * scale, true);
        Graphics gg (gridCache);
        gg.addTransform (AffineTransform::scale ((float) scale));
        gg.setGradientFill (ColourGradient (Colour (0xff141614), 0, 0, Colour (0xff0b0c0b), 0, b.getHeight(), false));
        gg.fillRoundedRectangle (b, 3.0f);
        for (float x = b.getX() + 12.0f; x < b.getRight(); x += 19.5f)
        {
            gg.setColour (Colour (0xff1f2a25).withAlpha (0.8f));
            gg.drawVerticalLine (roundToInt (x), 2.0f, b.getHeight() - 2.0f);
        }
        for (float y = 13.0f; y < b.getHeight(); y += 19.0f)
            gg.drawHorizontalLine (roundToInt (y), 2.0f, b.getWidth() - 2.0f);
        // inner shadow
        for (int i = 0; i < 6; ++i)
        {
            gg.setColour (Colours::black.withAlpha (0.12f));
            gg.drawRoundedRectangle (b.reduced ((float) i), 3.0f, 1.0f);
        }
    }
    g.drawImage (gridCache, b);

    const float midY = b.getCentreY() + 8.0f;
    const float halfH = b.getHeight() * 0.5f - 22.0f;
    const float dx = b.getWidth() / (float) columns;

    g.setColour (Colour (0xff3a443f));
    g.drawHorizontalLine (roundToInt (midY), 4.0f, b.getRight() - 4.0f);

    auto amp = [halfH] (float v) { return std::sqrt (jlimit (0.0f, 1.5f, v)) * halfH; };

    Path inPath, outPath, gainPath;
    for (int pass = 0; pass < 2; ++pass)
    {
        auto& hist = pass == 0 ? inHist : outHist;
        auto& path = pass == 0 ? inPath : outPath;
        path.startNewSubPath (b.getX() + 2.0f, midY);
        for (int i = 0; i < columns; ++i)
            path.lineTo (2.0f + (float) i * dx, midY - amp (hist[(size_t) ((writePos + i) % columns)]));
        for (int i = columns - 1; i >= 0; --i)
            path.lineTo (2.0f + (float) i * dx, midY + amp (hist[(size_t) ((writePos + i) % columns)]));
        path.closeSubPath();
    }
    g.setColour (Colour (0xff5d6661).withAlpha (0.55f));
    g.fillPath (inPath);
    g.setColour (Colour (0xffd9d5d0).withAlpha (0.62f));
    g.fillPath (outPath);

    for (int i = 0; i < columns; ++i)
    {
        const float gdb = jlimit (-24.0f, 24.0f, gainHist[(size_t) ((writePos + i) % columns)]);
        const float y = midY - gdb / 24.0f * halfH;
        const float x = 2.0f + (float) i * dx;
        if (i == 0) gainPath.startNewSubPath (x, y); else gainPath.lineTo (x, y);
    }
    g.setColour (colours::redHot.withAlpha (0.25f));
    g.strokePath (gainPath, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (colours::redHot);
    g.strokePath (gainPath, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));

    // section labels, as in the art
    g.setFont (Fonts::bold (16.5f));
    auto label = [&] (const String& t, float centreX)
    {
        const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), t);
        const float y = 10.0f;
        g.setColour (Colour (0xff0d0e0d));
        g.fillRect (centreX - tw * 0.5f - 6.0f, y, tw + 12.0f, 20.0f);
        g.setColour (colours::red);
        g.drawText (t, Rectangle<float> (tw + 2.0f, 20.0f).withCentre ({ centreX, y + 10.0f }), Justification::centred);
        g.setColour (colours::red.withAlpha (0.7f));
        g.drawHorizontalLine (roundToInt (y + 10.0f), centreX - tw * 0.5f - 17.0f, centreX - tw * 0.5f - 9.0f);
        g.drawHorizontalLine (roundToInt (y + 10.0f), centreX + tw * 0.5f + 9.0f, centreX + tw * 0.5f + 17.0f);
    };
    label ("TRANSIENT", 352.0f - (float) getX());
    label ("BODY", 633.0f - (float) getX());
}

//==============================================================================
static float meterYToDb (float y)
{
    const auto& s = layout::meterScale;
    const int n = (int) std::size (s);
    if (y <= s[0].y) return s[0].db + (s[0].y - y) * (6.0f / 26.0f);
    for (int i = 0; i < n - 1; ++i)
        if (y <= s[i + 1].y)
            return jmap (y, s[i].y, s[i + 1].y, s[i].db, s[i + 1].db);
    return -100.0f;
}

void LedMeter::pull (float dt)
{
    const float v[2] = { proc.meterL.exchange (0.0f), proc.meterR.exchange (0.0f) };
    bool changed = false;
    for (int c = 0; c < 2; ++c)
    {
        const float db = Decibels::gainToDecibels (v[c], -100.0f);
        const float fallen = levelDb[c] - 30.0f * dt;
        const float nl = jmax (db, fallen, -100.0f);
        if (db >= peakDb[c]) { peakDb[c] = db; peakHold[c] = 1.2f; }
        else if ((peakHold[c] -= dt) <= 0.0f) peakDb[c] = jmax (-100.0f, peakDb[c] - 20.0f * dt);
        changed |= std::abs (nl - levelDb[c]) > 0.05f;
        levelDb[c] = nl;
    }
    if (changed || peakHold[0] > 0.0f || peakHold[1] > 0.0f) repaint();
}

void LedMeter::drawColumn (Graphics& g, Rectangle<int> r, float lvl, float peak)
{
    constexpr int segments = 20;
    const float segH = (float) r.getHeight() / (float) segments;
    int peakSeg = -1;
    for (int i = 0; i < segments; ++i)
    {
        const float top = (float) r.getY() + (float) i * segH;
        const float db = meterYToDb (top + segH * 0.5f + (float) getY());
        if (peak >= db && peakSeg < 0) peakSeg = i;
    }
    for (int i = 0; i < segments; ++i)
    {
        const float top = (float) r.getY() + (float) i * segH;
        const float db = meterYToDb (top + segH * 0.5f + (float) getY());
        const bool lit = lvl >= db;
        if (! lit && i != peakSeg) continue;

        Colour c = db > 0.0f ? colours::redHot : db > -6.0f ? colours::red : Colour (0xffded8d2);
        if (! lit) c = c.withAlpha (0.55f);
        auto seg = Rectangle<float> ((float) r.getX() + 1.0f, top + 1.0f, (float) r.getWidth() - 2.0f, segH - 2.6f);
        g.setColour (c.withAlpha (c.getFloatAlpha() * 0.22f));
        g.fillRoundedRectangle (seg.expanded (1.5f), 2.0f);
        g.setGradientFill (ColourGradient (c.brighter (0.25f), seg.getX(), seg.getY(), c.darker (0.25f), seg.getRight(), seg.getBottom(), false));
        g.fillRoundedRectangle (seg, 1.0f);
    }
}

void LedMeter::paint (Graphics& g)
{
    drawColumn (g, layout::meterL.translated (-getX(), -getY()), levelDb[0], peakDb[0]);
    drawColumn (g, layout::meterR.translated (-getX(), -getY()), levelDb[1], peakDb[1]);
}

//==============================================================================
void ValueBubble::show (const String& t, Point<float> anchorTop)
{
    text = t;
    const int w = jmax (54, (int) GlyphArrangement::getStringWidth (Fonts::bold (15.0f), text) + 20);
    setBounds (Rectangle<int> (w, 26).withCentre (anchorTop.toInt()).translated (0, -15));
    setVisible (true);
    repaint();
}

void ValueBubble::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Colour (0xf0121110));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (colours::red);
    g.drawRoundedRectangle (r, 5.0f, 1.3f);
    g.setFont (Fonts::bold (15.0f));
    g.setColour (Colours::white);
    g.drawText (text, r, Justification::centred);
}
} // namespace pmx
