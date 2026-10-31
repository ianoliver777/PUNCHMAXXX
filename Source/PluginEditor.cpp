#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
const Colour red       { 0xffe0141f };
const Colour redGlow   { 0xffff2a35 };
const Colour valueCol  { 0xffd6d6d6 };
const Colour whiteCol  { 0xfff2f2f2 };

Font uiFont (float size, bool bold = false)
{
    return Font (FontOptions (size, bold ? Font::bold : Font::plain));
}

// piecewise-linear dB -> y using the tick positions baked into the artwork
float mapDb (float db, const float* dbs, const float* ys, int n)
{
    if (db >= dbs[0]) return ys[0];
    for (int i = 1; i < n; ++i)
        if (db >= dbs[i])
            return jmap (db, dbs[i - 1], dbs[i], ys[i - 1], ys[i]);
    return ys[n - 1];
}

const float scaleDb[]  = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f };
const float graphY[]   = { 176.0f, 203.0f, 229.0f, 256.0f, 294.0f, 336.0f, 375.0f };
const float meterY[]   = { 194.0f, 223.0f, 254.0f, 285.0f, 316.0f, 354.0f, 386.0f };
constexpr float graphL = 326.0f, graphR = 1496.0f;
constexpr float meterTop = 190.0f, meterBottom = 392.0f, meterW = 30.0f;
constexpr int historySize = 600;
}

//==============================================================================
namespace pmx
{
PunchLookAndFeel::PunchLookAndFeel()
{
    setColour (PopupMenu::backgroundColourId,            Colour (0xff0c0c0c));
    setColour (PopupMenu::textColourId,                  whiteCol);
    setColour (PopupMenu::headerTextColourId,            Colour (0xff8a8a8a));
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff9e1118));
    setColour (PopupMenu::highlightedTextColourId,       Colours::white);
    setColour (AlertWindow::backgroundColourId,          Colour (0xff111111));
    setColour (AlertWindow::textColourId,                whiteCol);
    setColour (AlertWindow::outlineColourId,             Colour (0xff3a3a3a));
    setColour (TextEditor::backgroundColourId,           Colour (0xff050505));
    setColour (TextEditor::textColourId,                 whiteCol);
    setColour (TextEditor::outlineColourId,              Colour (0xff4a4a4a));
    setColour (TextEditor::focusedOutlineColourId,       red);
    setColour (TextButton::buttonColourId,               Colour (0xff1c1c1c));
    setColour (TextButton::textColourOffId,              whiteCol);
}

Font PunchLookAndFeel::getPopupMenuFont() { return uiFont (15.0f); }

void PunchLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (findColour (PopupMenu::backgroundColourId));
    g.setColour (Colour (0xff3a3a3a));
    g.drawRect (0, 0, w, h, 1);
}

void PunchLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, Slider& s)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float r  = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  c  = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool  bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const bool  dimmed  = ! s.isEnabled() || (bool) s.getProperties().getWithDefault ("dimmed", false);

    // --- arc track + red value arc
    const float ra = r * 0.885f, arcW = r * 0.05f;
    {
        Path track; track.addCentredArc (c.x, c.y, ra, ra, 0.0f, startAngle, endAngle, true);
        g.setColour (Colour (0xff1b1b1b));
        g.strokePath (track, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));

        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        if (std::abs (angle - from) > 0.01f)
        {
            Path arc; arc.addCentredArc (c.x, c.y, ra, ra, 0.0f, jmin (from, angle), jmax (from, angle), true);
            const auto col = dimmed ? red.withSaturation (0.35f).withBrightness (0.35f) : red;
            g.setColour (col.withAlpha (0.18f));
            g.strokePath (arc, PathStrokeType (arcW * 3.2f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (col.withAlpha (0.35f));
            g.strokePath (arc, PathStrokeType (arcW * 1.8f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (dimmed ? col : redGlow);
            g.strokePath (arc, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));
        }
        // small red end caps (matches the artwork)
        for (float a : { startAngle, endAngle })
        {
            const auto p1 = c.getPointOnCircumference (ra - arcW * 1.3f, a);
            const auto p2 = c.getPointOnCircumference (ra + arcW * 1.3f, a);
            g.setColour (dimmed ? Colour (0xff5a1a1d) : red);
            g.drawLine ({ p1, p2 }, arcW * 0.8f);
        }
    }

    // --- drop shadow + bezel
    const float rb = r * 0.80f;
    {
        ColourGradient shadow (Colours::black.withAlpha (0.75f), c.x, c.y + r * 0.08f,
                               Colours::transparentBlack, c.x, c.y + rb * 1.25f, true);
        g.setGradientFill (shadow);
        g.fillEllipse (Rectangle<float> (rb * 2.5f, rb * 2.5f).withCentre (c.translated (0, r * 0.06f)));

        ColourGradient bezel (Colour (0xff5c5c5c), c.x, c.y - rb, Colour (0xff050505), c.x, c.y + rb, false);
        bezel.addColour (0.45, Colour (0xff1c1c1c));
        g.setGradientFill (bezel);
        g.fillEllipse (Rectangle<float> (rb * 2.0f, rb * 2.0f).withCentre (c));
    }

    // --- brushed metal body
    const float rk = r * 0.70f;
    {
        auto body = Rectangle<float> (rk * 2.0f, rk * 2.0f).withCentre (c);
        ColourGradient base (Colour (0xff3e3e3e), c.x - rk * 0.35f, c.y - rk * 0.55f,
                             Colour (0xff0f0f0f), c.x + rk * 0.5f, c.y + rk, true);
        g.setGradientFill (base);
        g.fillEllipse (body);

        // concentric "brushed" rings
        for (int i = 1; i < 14; ++i)
        {
            const float rr = rk * (float) i / 14.0f;
            g.setColour (Colours::white.withAlpha (i % 2 == 0 ? 0.025f : 0.012f));
            g.drawEllipse (Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c), jmax (0.5f, rk * 0.012f));
        }
        // specular sweep
        ColourGradient spec (Colours::white.withAlpha (0.16f), c.x, c.y - rk,
                             Colours::white.withAlpha (0.0f), c.x, c.y + rk * 0.1f, false);
        g.setGradientFill (spec);
        g.fillEllipse (body.reduced (rk * 0.06f));
        // conic glints (cheap: two soft wedges)
        for (float a : { -0.75f, 2.4f })
        {
            Path wedge; wedge.addPieSegment (body.reduced (rk * 0.05f), a - 0.25f, a + 0.25f, 0.0f);
            g.setColour (Colours::white.withAlpha (0.05f));
            g.fillPath (wedge);
        }
        // rim highlight
        g.setColour (Colours::white.withAlpha (0.22f));
        Path rim; rim.addCentredArc (c.x, c.y, rk, rk, 0.0f, -2.2f, 2.2f, true);
        g.strokePath (rim, PathStrokeType (jmax (0.8f, rk * 0.025f)));
        g.setColour (Colours::black.withAlpha (0.7f));
        g.drawEllipse (body, jmax (0.8f, rk * 0.02f));
    }

    // --- pointer
    {
        const auto p1 = c.getPointOnCircumference (rk * 0.52f, angle);
        const auto p2 = c.getPointOnCircumference (rk * 0.93f, angle);
        const float pw = jmax (1.5f, r * 0.055f);
        g.setColour (Colours::white.withAlpha (0.15f));
        g.drawLine ({ p1, p2 }, pw * 2.6f);
        Path ptr; ptr.startNewSubPath (p1); ptr.lineTo (p2);
        g.setColour (dimmed ? Colour (0xff8a8a8a) : Colours::white);
        g.strokePath (ptr, PathStrokeType (pw, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

void HitButton::paintButton (Graphics& g, bool over, bool down)
{
    if (! over && ! down) return;
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Colours::white.withAlpha (down ? 0.10f : 0.05f));
    g.fillRoundedRectangle (b, b.getHeight() * corner);
}

void PowerButton::paintButton (Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.25f;
    const bool on = getToggleState();
    const Colour col = on ? Colours::white : Colour (0xff555555);

    if (over || down)
    {
        g.setColour (Colours::white.withAlpha (down ? 0.08f : 0.04f));
        g.fillRoundedRectangle (b.reduced (b.getHeight() * 0.1f), b.getHeight() * 0.2f);
    }

    Path icon;
    icon.addCentredArc (c.x, c.y + r * 0.08f, r, r, 0.0f, 0.65f, MathConstants<float>::twoPi - 0.65f, true);
    icon.startNewSubPath (c.x, c.y - r * 1.15f);
    icon.lineTo (c.x, c.y - r * 0.05f);

    if (on)
    {
        g.setColour (Colours::white.withAlpha (0.18f));
        g.strokePath (icon, PathStrokeType (r * 0.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (col);
    g.strokePath (icon, PathStrokeType (jmax (1.2f, r * 0.2f), PathStrokeType::curved, PathStrokeType::rounded));
}
} // namespace pmx

//==============================================================================
PunchmaxxxAudioProcessorEditor::PunchmaxxxAudioProcessorEditor (PunchmaxxxAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    background = ImageCache::getFromMemory (BinaryData::background_png, BinaryData::background_pngSize);
    history.assign (historySize, { -100.0f, -100.0f, 0.0f });

    using namespace pmx::ids;
    setupKnob (knobs[0], thresh,       141.0f, 591.0f, 60.0f, 703.0f);
    setupKnob (knobs[1], ratio,        286.0f, 591.0f, 60.0f, 703.0f);
    setupKnob (knobs[2], compAttack,   430.0f, 591.0f, 60.0f, 703.0f);
    setupKnob (knobs[3], compRelease,  573.0f, 591.0f, 60.0f, 703.0f);
    setupKnob (knobs[4], transAttack,  797.0f, 587.0f, 70.0f, 711.0f, true);
    setupKnob (knobs[5], transSustain, 991.0f, 587.0f, 70.0f, 711.0f, true);
    setupKnob (knobs[6], ceiling,     1263.0f, 588.0f, 62.0f, 703.0f);
    setupKnob (knobs[7], drive,       1446.0f, 588.0f, 62.0f, 703.0f);
    setupKnob (knobs[8], mix,         1626.0f, 588.0f, 62.0f, 703.0f);
    setupKnob (knobs[9], output,      1682.0f, 816.0f, 42.0f, -1.0f);

    for (auto* b : { &compPower, &transPower, &maxPower }) addAndMakeVisible (b);
    compPowerAtt  = std::make_unique<ButtonAttachment> (proc.apvts, compOn,  compPower);
    transPowerAtt = std::make_unique<ButtonAttachment> (proc.apvts, transOn, transPower);
    maxPowerAtt   = std::make_unique<ButtonAttachment> (proc.apvts, maxOn,   maxPower);
    compPower.setTooltip ("Compressor on/off");
    transPower.setTooltip ("Transient on/off");
    maxPower.setTooltip ("Maximizer on/off");

    for (auto* b : { &prevBtn, &nextBtn, &presetNameBtn, &saveBtn, &saveAsBtn, &settingsBtn, &osBtn })
        addAndMakeVisible (b);

    prevBtn.onClick       = [this] { proc.presets.loadPrevious(); };
    nextBtn.onClick       = [this] { proc.presets.loadNext(); };
    presetNameBtn.onClick = [this] { showPresetMenu(); };
    saveBtn.onClick       = [this] { savePreset(); };
    saveAsBtn.onClick     = [this] { saveAsPreset(); };
    settingsBtn.onClick   = [this] { showSettingsMenu(); };
    osBtn.onClick         = [this] { showOversampleMenu(); };

    saveBtn.setTooltip ("Save preset");
    saveAsBtn.setTooltip ("Save preset as...");
    settingsBtn.setTooltip ("Preset options");

    constrainer.setFixedAspectRatio ((double) artW / (double) artH);
    constrainer.setSizeLimits (artW * 2 / 5, artH * 2 / 5, artW, artH);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (1064, 532);

    startTimerHz (60);
}

PunchmaxxxAudioProcessorEditor::~PunchmaxxxAudioProcessorEditor()
{
    stopTimer();
    if (nameDialog) nameDialog->exitModalState (0);
    setLookAndFeel (nullptr);
}

void PunchmaxxxAudioProcessorEditor::setupKnob (Knob& k, const char* id, float cx, float cy, float r, float valueY, bool bipolar)
{
    k.paramId = id; k.centre = { cx, cy }; k.radius = r; k.valueY = valueY;
    auto& s = k.slider;
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    s.setMouseDragSensitivity (260);
    s.setVelocityModeParameters (0.4, 1, 0.0, true, ModifierKeys::shiftModifier); // shift = fine
    s.getProperties().set ("bipolar", bipolar);
    s.setPopupMenuEnabled (false);
    addAndMakeVisible (s);
    k.attach = std::make_unique<SliderAttachment> (proc.apvts, id, s);

    if (auto* param = proc.apvts.getParameter (id))
    {
        auto* ranged = dynamic_cast<RangedAudioParameter*> (param);
        s.setDoubleClickReturnValue (true, ranged->convertFrom0to1 (ranged->getDefaultValue()));
        s.setTooltip (param->getName (64) + "  (double-click = reset, shift-drag = fine)");
    }
    s.onValueChange = [this, &k]
    {
        if (k.valueY > 0) repaintArt ({ k.centre.x - 70.0f, k.valueY - 16.0f, 140.0f, 32.0f });
        else              repaintArt ({ 1508.0f, 798.0f, 114.0f, 40.0f });
    };
}

Rectangle<int> PunchmaxxxAudioProcessorEditor::toScreen (Rectangle<float> a) const
{
    const float s = (float) getWidth() / (float) artW;
    return (a * s).getSmallestIntegerContainer();
}

void PunchmaxxxAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / (float) artW;
    const int target = jmin (artW, roundToInt (getWidth() * 2.0f));
    scaledBackground = background.rescaled (target, roundToInt (target * (float) artH / (float) artW), Graphics::highResamplingQuality);

    for (auto& k : knobs)
        k.slider.setBounds (toScreen (Rectangle<float> (k.radius * 2.0f, k.radius * 2.0f).withCentre (k.centre)));

    compPower.setBounds  (toScreen ({ 85.0f, 456.0f, 54.0f, 37.0f }));
    transPower.setBounds (toScreen ({ 711.0f, 452.0f, 54.0f, 37.0f }));
    maxPower.setBounds   (toScreen ({ 1169.0f, 456.0f, 54.0f, 37.0f }));

    prevBtn.setBounds       (toScreen ({ 1310.0f, 53.0f, 42.0f, 40.0f }));
    nextBtn.setBounds       (toScreen ({ 1514.0f, 53.0f, 42.0f, 40.0f }));
    presetNameBtn.setBounds (toScreen ({ 1354.0f, 53.0f, 158.0f, 40.0f }));
    saveBtn.setBounds       (toScreen ({ 1579.0f, 51.0f, 44.0f, 44.0f }));
    saveAsBtn.setBounds     (toScreen ({ 1635.0f, 51.0f, 44.0f, 44.0f }));
    settingsBtn.setBounds   (toScreen ({ 1691.0f, 51.0f, 44.0f, 44.0f }));
    osBtn.setBounds         (toScreen ({ 236.0f, 797.0f, 153.0f, 41.0f }));
    ignoreUnused (s);
}

//==============================================================================
void PunchmaxxxAudioProcessorEditor::paint (Graphics& g)
{
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (scaledBackground, getLocalBounds().toFloat());

    const float s = (float) getWidth() / (float) artW;
    g.addTransform (AffineTransform::scale (s));

    // knob values
    g.setFont (uiFont (19.0f));
    for (auto& k : knobs)
    {
        const auto txt = proc.apvts.getParameter (k.paramId)->getCurrentValueAsText();
        if (k.valueY > 0)
        {
            g.setColour (valueCol);
            g.drawText (txt, Rectangle<float> (140.0f, 30.0f).withCentre ({ k.centre.x, k.valueY }), Justification::centred);
        }
        else
        {
            g.setColour (whiteCol);
            g.setFont (uiFont (21.0f));
            g.drawText (txt, Rectangle<float> (114.0f, 40.0f).withCentre ({ 1565.0f, 818.0f }), Justification::centred);
            g.setFont (uiFont (19.0f));
        }
    }

    // preset name
    {
        auto name = proc.presets.getCurrentName().toUpperCase();
        if (proc.presets.isDirty()) name << " *";
        g.setColour (Colours::white);
        g.setFont (uiFont (21.0f, true));
        g.drawFittedText (name, Rectangle<int> (1356, 54, 154, 38), Justification::centred, 1, 0.7f);
        lastPresetText = name;
    }

    // oversample
    g.setColour (whiteCol);
    g.setFont (uiFont (21.0f));
    g.drawText (proc.apvts.getParameter (pmx::ids::oversample)->getCurrentValueAsText(),
                Rectangle<float> (262.0f, 798.0f, 80.0f, 40.0f), Justification::centredLeft);

    drawMeter (g, 96.0f, inDisp, inPeak);
    drawMeter (g, 1604.0f, outDisp, outPeak);
    drawGraph (g);
}

void PunchmaxxxAudioProcessorEditor::drawMeter (Graphics& g, float x, float level, float peak)
{
    const auto bar = Rectangle<float> (x, meterTop, meterW, meterBottom - meterTop);
    g.setColour (Colour (0xff0a0a0a));
    g.fillRect (bar);

    const float yLevel = mapDb (level, scaleDb, meterY, 7);
    if (level > -60.0f)
    {
        auto fill = bar.withTop (jmin (yLevel, meterBottom - 1.0f));
        ColourGradient grad (Colour (0xffeaeaea), x, meterY[0], Colour (0xff3c3c3c), x, meterBottom, false);
        grad.addColour (0.04, Colour (0xffffffff));
        g.setGradientFill (grad);
        g.fillRect (fill.reduced (3.0f, 0.0f));

        const float yHot = mapDb (-3.0f, scaleDb, meterY, 7);
        if (yLevel < yHot)
        {
            g.setColour (red);
            g.fillRect (Rectangle<float> (x + 3.0f, yLevel, meterW - 6.0f, yHot - yLevel));
        }
    }
    if (peak > -60.0f)
    {
        const float yp = mapDb (peak, scaleDb, meterY, 7);
        g.setColour (peak > -0.3f ? redGlow : Colours::white.withAlpha (0.9f));
        g.fillRect (Rectangle<float> (x + 3.0f, yp - 1.0f, meterW - 6.0f, 2.5f));
    }
}

void PunchmaxxxAudioProcessorEditor::drawGraph (Graphics& g)
{
    const float w = graphR - graphL;
    const float dx = w / (float) (historySize - 1);
    const float yBottom = graphY[6];

    Path outFill, outLine, inLine, grLine, grFill;
    outFill.startNewSubPath (graphL, yBottom);
    grFill.startNewSubPath (graphL, graphY[0]);
    for (int i = 0; i < historySize; ++i)
    {
        const auto& p = history[(size_t) ((historyPos + i) % historySize)];
        const float x = graphL + dx * (float) i;
        const float yo = mapDb (p.outDb, scaleDb, graphY, 7);
        const float yi = mapDb (p.inDb,  scaleDb, graphY, 7);
        const float yg = mapDb (p.grDb,  scaleDb, graphY, 7);
        outFill.lineTo (x, yo);
        grFill.lineTo (x, yg);
        if (i == 0) { outLine.startNewSubPath (x, yo); inLine.startNewSubPath (x, yi); grLine.startNewSubPath (x, yg); }
        else        { outLine.lineTo (x, yo);          inLine.lineTo (x, yi);          grLine.lineTo (x, yg); }
    }
    outFill.lineTo (graphR, yBottom); outFill.closeSubPath();
    grFill.lineTo (graphR, graphY[0]); grFill.closeSubPath();

    g.saveState();
    g.reduceClipRegion (Rectangle<float> (graphL, graphY[0] - 4.0f, w, yBottom - graphY[0] + 6.0f).toNearestInt());

    ColourGradient og (Colours::white.withAlpha (0.20f), 0, graphY[0], Colours::white.withAlpha (0.02f), 0, yBottom, false);
    g.setGradientFill (og);
    g.fillPath (outFill);

    g.setColour (Colours::white.withAlpha (0.28f));
    g.strokePath (inLine, PathStrokeType (1.4f));

    g.setColour (Colours::white.withAlpha (0.12f));
    g.strokePath (outLine, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colours::white.withAlpha (0.95f));
    g.strokePath (outLine, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));

    g.setColour (red.withAlpha (0.16f));
    g.fillPath (grFill);
    g.setColour (redGlow);
    g.strokePath (grLine, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    g.restoreState();

    g.setFont (uiFont (14.0f));
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawText ("OUT", Rectangle<float> (1440.0f, 156.0f, 50.0f, 16.0f), Justification::centredRight);
    g.setColour (red.withAlpha (0.8f));
    g.drawText ("GR", Rectangle<float> (1385.0f, 156.0f, 50.0f, 16.0f), Justification::centredRight);
}

//==============================================================================
void PunchmaxxxAudioProcessorEditor::timerCallback()
{
    // meters (60 fps): fast attack, smooth 24 dB/s fall, 1.2 s peak hold
    auto ballistics = [] (float newLin, float& disp, float& peak, int& hold)
    {
        const float db = pmx::gainToDb (newLin);
        disp = db > disp ? db : jmax (db, disp - 24.0f / 60.0f);
        if (db >= peak) { peak = db; hold = 72; }
        else if (--hold <= 0) peak = jmax (-100.0f, peak - 30.0f / 60.0f);
        disp = jmax (disp, -100.0f);
    };
    ballistics (proc.inLevel.exchange (0.0f),  inDisp,  inPeak,  inPeakHold);
    ballistics (proc.outLevel.exchange (0.0f), outDisp, outPeak, outPeakHold);
    proc.grLevelDb.store (0.0f);

    int s1, n1, s2, n2;
    const int ready = proc.graphFifo.getNumReady();
    proc.graphFifo.prepareToRead (ready, s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) { history[(size_t) historyPos] = proc.graphData[(size_t) (s1 + i)]; historyPos = (historyPos + 1) % historySize; }
    for (int i = 0; i < n2; ++i) { history[(size_t) historyPos] = proc.graphData[(size_t) (s2 + i)]; historyPos = (historyPos + 1) % historySize; }
    proc.graphFifo.finishedRead (n1 + n2);

    repaintArt ({ 90.0f, 185.0f, 42.0f, 212.0f });
    repaintArt ({ 1598.0f, 185.0f, 42.0f, 212.0f });
    repaintArt ({ graphL - 2.0f, 150.0f, graphR - graphL + 4.0f, 232.0f });

    auto name = proc.presets.getCurrentName().toUpperCase() + (proc.presets.isDirty() ? " *" : "");
    if (name != lastPresetText) repaintArt ({ 1350.0f, 50.0f, 166.0f, 46.0f });

    const int osNow = (int) proc.apvts.getRawParameterValue (pmx::ids::oversample)->load();
    if (osNow != osLast) { osLast = osNow; repaintArt ({ 250.0f, 798.0f, 100.0f, 40.0f }); }

    const bool compOn = compPower.getToggleState(), transOn = transPower.getToggleState(), maxOn = maxPower.getToggleState();
    for (int i = 0; i < 9; ++i)
    {
        const bool on = i < 4 ? compOn : (i < 6 ? transOn : maxOn);
        auto& s = knobs[(size_t) i].slider;
        if ((bool) s.getProperties()["dimmed"] == on) { s.getProperties().set ("dimmed", ! on); s.repaint(); }
    }
}

//==============================================================================
void PunchmaxxxAudioProcessorEditor::showPresetMenu()
{
    auto& pm = proc.presets;
    pm.refreshUserPresets();
    PopupMenu menu;
    menu.addSectionHeader ("FACTORY");
    for (int i = 0; i < pm.getNumFactory(); ++i)
        menu.addItem (i + 1, pm.getNameAt (i), true, i == pm.getCurrentIndex());
    menu.addSectionHeader ("USER");
    if (pm.getUserFiles().isEmpty())
        menu.addItem (-1, "(no user presets yet)", false);
    for (int i = pm.getNumFactory(); i < pm.getNumTotal(); ++i)
        menu.addItem (i + 1, pm.getNameAt (i), true, i == pm.getCurrentIndex());
    menu.addSeparator();
    menu.addItem (10001, "Save");
    menu.addItem (10002, "Save As...");
    menu.addItem (10003, "Open Preset File...");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetNameBtn).withMinimumWidth (presetNameBtn.getWidth() + 40),
                        [safe = Component::SafePointer<PunchmaxxxAudioProcessorEditor> (this)] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 10001) safe->savePreset();
        else if (r == 10002) safe->saveAsPreset();
        else if (r == 10003) safe->openPresetFile();
        else if (r > 0) safe->proc.presets.loadIndex (r - 1);
        safe->repaint();
    });
}

void PunchmaxxxAudioProcessorEditor::showSettingsMenu()
{
    PopupMenu menu;
    menu.addSectionHeader ("PUNCHMAXXX  v" JucePlugin_VersionString "  -  TRIPLE 7 AUDIO");
    menu.addItem (1, "New Preset (INIT)");
    menu.addItem (2, "Save As...");
    menu.addItem (3, "Open Preset File...");
    menu.addItem (4, "Export Preset File...");
    menu.addItem (5, "Delete Current User Preset", proc.presets.currentIsUser());
    menu.addSeparator();
    menu.addItem (6, "Show Preset Folder");
    menu.addItem (7, "Refresh Preset List");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&settingsBtn),
                        [safe = Component::SafePointer<PunchmaxxxAudioProcessorEditor> (this)] (int r)
    {
        if (safe == nullptr) return;
        switch (r)
        {
            case 1: safe->proc.presets.loadIndex (0); break;
            case 2: safe->saveAsPreset(); break;
            case 3: safe->openPresetFile(); break;
            case 4: safe->exportPresetFile(); break;
            case 5: safe->deleteCurrentPreset(); break;
            case 6: PresetManager::getUserPresetFolder().revealToUser(); break;
            case 7: safe->proc.presets.refreshUserPresets(); break;
            default: break;
        }
        safe->repaint();
    });
}

void PunchmaxxxAudioProcessorEditor::showOversampleMenu()
{
    auto* param = proc.apvts.getParameter (pmx::ids::oversample);
    const int current = (int) proc.apvts.getRawParameterValue (pmx::ids::oversample)->load();
    PopupMenu menu;
    menu.addSectionHeader ("OVERSAMPLE");
    menu.addItem (1, "OFF", true, current == 0);
    menu.addItem (2, "2X", true, current == 1);
    menu.addItem (3, "4X  (cleanest)", true, current == 2);
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&osBtn),
                        [safe = Component::SafePointer<PunchmaxxxAudioProcessorEditor> (this), param] (int r)
    {
        if (safe == nullptr || r == 0) return;
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) (r - 1)));
        param->endChangeGesture();
        safe->repaint();
    });
}

void PunchmaxxxAudioProcessorEditor::savePreset()
{
    if (proc.presets.currentIsUser())
    {
        proc.presets.saveUserPreset (proc.presets.getCurrentName());
        repaint();
    }
    else
        saveAsPreset();
}

void PunchmaxxxAudioProcessorEditor::saveAsPreset()
{
    nameDialog = std::make_unique<AlertWindow> ("SAVE PRESET", "Name your preset:", MessageBoxIconType::NoIcon, this);
    nameDialog->setLookAndFeel (&lnf);
    auto suggested = proc.presets.currentIsUser() ? proc.presets.getCurrentName() : proc.presets.getCurrentName() + " (copy)";
    nameDialog->addTextEditor ("name", suggested);
    nameDialog->addButton ("SAVE", 1, KeyPress (KeyPress::returnKey));
    nameDialog->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));

    nameDialog->enterModalState (true, ModalCallbackFunction::create (
        [safe = Component::SafePointer<PunchmaxxxAudioProcessorEditor> (this)] (int result)
    {
        if (safe == nullptr || safe->nameDialog == nullptr) return;
        const auto name = safe->nameDialog->getTextEditorContents ("name").trim();
        safe->nameDialog.reset();
        if (result != 1 || name.isEmpty()) return;

        auto file = PresetManager::getUserPresetFolder().getChildFile (File::createLegalFileName (name) + PresetManager::extension);
        if (file.existsAsFile())
        {
            AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "OVERWRITE?",
                                          "A preset called \"" + name + "\" already exists. Replace it?",
                                          "REPLACE", "CANCEL", safe.getComponent(),
                                          ModalCallbackFunction::create ([safe, name] (int ok)
            {
                if (safe != nullptr && ok == 1) { safe->proc.presets.saveUserPreset (name); safe->repaint(); }
            }));
            return;
        }
        safe->proc.presets.saveUserPreset (name);
        safe->repaint();
    }), false);
}

void PunchmaxxxAudioProcessorEditor::openPresetFile()
{
    chooser = std::make_unique<FileChooser> ("Open PUNCHMAXXX preset", PresetManager::getUserPresetFolder(),
                                             String ("*") + PresetManager::extension);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe = Component::SafePointer<PunchmaxxxAudioProcessorEditor> (this)] (const FileChooser& fc)
    {
        if (safe == nullptr) return;
        auto f = fc.getResult();
        if (f.existsAsFile() && ! safe->proc.presets.loadFromFile (f))
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "PUNCHMAXXX", "That file isn't a valid PUNCHMAXXX preset.");
        safe->repaint();
    });
}

void PunchmaxxxAudioProcessorEditor::exportPresetFile()
{
    chooser = std::make_unique<FileChooser> ("Export PUNCHMAXXX preset",
                                             File::getSpecialLocation (File::userDesktopDirectory)
                                                 .getChildFile (File::createLegalFileName (proc.presets.getCurrentName()) + PresetManager::extension),
                                             String ("*") + PresetManager::extension);
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                          [safe = Component::SafePointer<PunchmaxxxAudioProcessorEditor> (this)] (const FileChooser& fc)
    {
        if (safe == nullptr) return;
        auto target = fc.getResult();
        if (target == File()) return;
        // save into the user folder, then copy out
        auto& pm = safe->proc.presets;
        const auto name = target.getFileNameWithoutExtension();
        if (pm.saveUserPreset (name))
            PresetManager::getUserPresetFolder().getChildFile (File::createLegalFileName (name) + PresetManager::extension)
                .copyFileTo (target.withFileExtension (PresetManager::extension));
        safe->repaint();
    });
}

void PunchmaxxxAudioProcessorEditor::deleteCurrentPreset()
{
    if (! proc.presets.currentIsUser()) return;
    const auto name = proc.presets.getCurrentName();
    AlertWindow::showOkCancelBox (MessageBoxIconType::WarningIcon, "DELETE PRESET",
                                  "Delete \"" + name + "\"? This can't be undone.", "DELETE", "CANCEL", this,
                                  ModalCallbackFunction::create ([safe = Component::SafePointer<PunchmaxxxAudioProcessorEditor> (this)] (int ok)
    {
        if (safe != nullptr && ok == 1)
        {
            safe->proc.presets.deleteUserPreset (safe->proc.presets.getCurrentIndex());
            safe->repaint();
        }
    }));
}
