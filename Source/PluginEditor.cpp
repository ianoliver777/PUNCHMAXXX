#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;
namespace L = pmx::layout;

//==============================================================================
PunchmaxxxEditor::Canvas::Canvas (PunchmaxxxProcessor& p) : proc (p)
{
    background = ImageCache::getFromMemory (BinaryData::background_png, BinaryData::background_pngSize);
    setOpaque (true);
}

void PunchmaxxxEditor::Canvas::paint (Graphics& g)
{
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (background, getLocalBounds().toFloat());

    // FOCUS: live Hz readout (replaces the static "200") + the real minimum label
    const float hz = proc.apvts.getRawParameterValue (pmx::ID::focus)->load();
    g.setFont (pmx::Fonts::bold (18.0f));
    g.setColour (pmx::colours::red);
    g.drawText (String (roundToInt (hz)), Rectangle<float> (70.0f, 24.0f).withCentre (L::focusReadout), Justification::centred);
    g.setFont (pmx::Fonts::bold (16.0f));
    g.setColour (pmx::colours::label);
    g.drawText ("50", Rectangle<float> (40.0f, 22.0f).withCentre (L::focusMinLabel), Justification::centred);
}

//==============================================================================
PunchmaxxxEditor::PunchmaxxxEditor (PunchmaxxxProcessor& p)
    : AudioProcessorEditor (p), proc (p), canvas (p), scope (p), meter (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (canvas);
    canvas.setSize (L::width, L::height);

    canvas.addAndMakeVisible (scope);
    scope.setBounds (L::display);
    canvas.addAndMakeVisible (meter);
    meter.setBounds (L::meterL.getUnion (L::meterR).expanded (3));

    for (const auto& spec : L::knobs)
    {
        auto* param = proc.apvts.getParameter (spec.paramID);
        jassert (param != nullptr);
        auto k = std::make_unique<pmx::KnobOverlay> (spec, *param);
        knobAttachments.push_back (std::make_unique<SliderParameterAttachment> (*param, *k, nullptr));
        auto* raw = k.get();
        raw->onHotChanged = [this] { updateBubble(); };
        raw->onValueChange = [this, raw]
        {
            updateBubble();
            if (raw->getName() == pmx::ID::focus)
                canvas.repaint (Rectangle<float> (80.0f, 30.0f).withCentre (L::focusReadout).toNearestInt());
        };
        raw->onDragStart = [this] { updateBubble(); };
        raw->onDragEnd = [this] { updateBubble(); };
        raw->setName (spec.paramID);
        canvas.addAndMakeVisible (raw);
        knobs.push_back (std::move (k));
    }

    modeGroup = std::make_unique<pmx::ChoiceGroup> (*proc.apvts.getParameter (pmx::ID::mode), pmx::ChoiceGroup::Style::ModeButton,
                                                    L::modeButtons, 4, StringArray { "TIGHT", "PUNCH", "FAT", "CLEAN" });
    shapeGroup = std::make_unique<pmx::ChoiceGroup> (*proc.apvts.getParameter (pmx::ID::shape), pmx::ChoiceGroup::Style::ShapeButton,
                                                     L::shapeButtons, 4, StringArray { "SOFT", "SMOOTH", "HARD", "EXP" });
    canvas.addAndMakeVisible (*modeGroup);
    canvas.addAndMakeVisible (*shapeGroup);

    auto place = [this] (Button& b, Rectangle<int> r, std::function<void()> fn)
    {
        canvas.addAndMakeVisible (b);
        b.setBounds (r);
        b.setMouseCursor (MouseCursor::PointingHandCursor);
        b.onClick = std::move (fn);
    };
    place (importBtn, L::btnImport, [this] { importPreset(); });
    place (folderBtn, L::btnFolder, [this] { openPresetFolder(); });
    place (prevBtn, L::btnPrev, [this] { proc.presets.loadPrevious(); });
    place (nextBtn, L::btnNext, [this] { proc.presets.loadNext(); });
    place (saveBtn, L::btnSave, [this] { openSaveDialog(); });
    place (initBtn, L::btnInit, [this] { proc.presets.loadInit(); });
    place (menuBtn, L::btnMenu, [this] { showMainMenu(); });

    canvas.addAndMakeVisible (presetBox);
    presetBox.setBounds (L::presetBox);
    presetBox.setMouseCursor (MouseCursor::PointingHandCursor);
    presetBox.onClick = [this] { showPresetList(); };

    canvas.addChildComponent (bubble);

    refreshPresetBox();

    setResizable (true, true);
    setResizeLimits (720, 480, 1800, 1200);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) L::width / (double) L::height);
    const int w = proc.savedEditorWidth.load() > 0 ? proc.savedEditorWidth.load() : 1080;
    setSize (w, roundToInt ((float) w * (float) L::height / (float) L::width));

    lastTick = Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

PunchmaxxxEditor::~PunchmaxxxEditor()
{
    stopTimer();
    if (dialog != nullptr) dialog->exitModalState (0);
    knobAttachments.clear();
    setLookAndFeel (nullptr);
}

void PunchmaxxxEditor::paint (Graphics& g) { g.fillAll (Colours::black); }

void PunchmaxxxEditor::resized()
{
    const float scale = (float) getWidth() / (float) L::width;
    canvas.setTransform (AffineTransform::scale (scale));
    proc.savedEditorWidth = getWidth();
}

Image PunchmaxxxEditor::renderSnapshot()
{
    return canvas.createComponentSnapshot (canvas.getLocalBounds(), true, 1.0f);
}

void PunchmaxxxEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const float dt = (float) jlimit (0.0, 0.2, (now - lastTick) * 0.001);
    lastTick = now;
    scope.pull();
    meter.pull (dt);
    if (proc.presets.getChangeCounter() != lastPresetCounter)
        refreshPresetBox();
}

void PunchmaxxxEditor::refreshPresetBox()
{
    lastPresetCounter = proc.presets.getChangeCounter();
    presetBox.setText (proc.presets.getCurrentName(), proc.presets.isModified(), proc.presets.isCurrentUser());
}

void PunchmaxxxEditor::updateBubble()
{
    for (auto& k : knobs)
    {
        if (k->isHot())
        {
            const auto c = k->centreInParent();
            bubble.show (k->valueText(), { c.x, c.y - k->outerRadius() - 2.0f });
            bubble.toFront (false);
            return;
        }
    }
    bubble.setVisible (false);
}

//==============================================================================
void PunchmaxxxEditor::showPresetList()
{
    auto& pm = proc.presets;
    pm.rescan();
    const auto& entries = pm.getEntries();
    const int current = pm.getCurrentIndex();

    PopupMenu menu;
    PopupMenu factory;
    bool hasUser = false;
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        const auto& e = entries[(size_t) i];
        const auto name = pmx::PresetManager::displayName (e.name);
        if (e.kind == pmx::PresetManager::Entry::Kind::Init)
            menu.addItem (i + 1, name, true, i == current);
        else if (e.kind == pmx::PresetManager::Entry::Kind::Factory)
        {
            if (i == 1) menu.addSectionHeader ("Factory");
            menu.addItem (i + 1, name, true, i == current);
        }
        else
        {
            if (! hasUser) { menu.addSectionHeader ("User"); hasUser = true; }
            menu.addItem (i + 1, name, true, i == current);
        }
    }
    if (! hasUser)
    {
        menu.addSectionHeader ("User");
        menu.addItem (9999, "Hit SAVE to make your own", false, false);
    }

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetBox).withMinimumWidth (presetBox.getWidth())
                            .withMaximumNumColumns (2).withStandardItemHeight (24),
                        [safe = Component::SafePointer<PunchmaxxxEditor> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->proc.presets.loadEntry (result - 1);
                        });
}

void PunchmaxxxEditor::showMainMenu()
{
    auto& pm = proc.presets;
    const auto shown = pmx::PresetManager::displayName (pm.getCurrentName());
    PopupMenu m;
    m.addSectionHeader ("Presets");
    m.addItem (1, "Save As New Preset...");
    m.addItem (2, "Overwrite \"" + shown + "\"", pm.isCurrentUser());
    m.addItem (3, "Delete \"" + shown + "\"", pm.isCurrentUser());
    m.addSeparator();
    m.addItem (4, "Load Preset File...");
    m.addItem (5, "Open Presets Folder");
    m.addItem (6, "Rescan Presets");
    m.addSectionHeader ("Window Size");
    const int sizes[] = { 75, 90, 100, 125, 150 };
    for (int i = 0; i < 5; ++i)
        m.addItem (100 + i, String (sizes[i]) + "%", true, std::abs (getWidth() - L::width * sizes[i] / 100) < 4);
    m.addSeparator();
    m.addItem (9, "About PUNCHMAXXX");

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&menuBtn),
                     [safe = Component::SafePointer<PunchmaxxxEditor> (this), sizes] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         auto& ed = *safe;
                         switch (r)
                         {
                             case 1: ed.openSaveDialog(); break;
                             case 2: ed.doSave (ed.proc.presets.getCurrentName()); break;
                             case 3: ed.confirmDelete(); break;
                             case 4: ed.importPreset(); break;
                             case 5: ed.openPresetFolder(); break;
                             case 6: ed.proc.presets.rescan(); break;
                             case 9:
                                 AlertWindow::showAsync (MessageBoxOptions().withTitle ("PUNCHMAXXX")
                                                             .withMessage (String ("Transient Drum Processor  v") + JucePlugin_VersionString
                                                                           + "\nby TRIPLE 7 AUDIO\n\nShift-drag for fine control, double-click a knob to reset.")
                                                             .withButton ("NICE").withAssociatedComponent (&ed), nullptr);
                                 break;
                             default:
                                 if (r >= 100 && r < 105)
                                     ed.setSize (L::width * sizes[r - 100] / 100, L::height * sizes[r - 100] / 100);
                         }
                     });
}

void PunchmaxxxEditor::openSaveDialog()
{
    auto& pm = proc.presets;
    String suggestion = pm.isCurrentUser() ? pm.getCurrentName() : String();
    if (pm.isCurrentFactory())
        suggestion = pm.getCurrentName() + " (mine)";

    dialog = std::make_unique<AlertWindow> ("SAVE PRESET", "Name it. Same name as one of yours = overwrite.", MessageBoxIconType::NoIcon, this);
    dialog->setLookAndFeel (&lnf);
    dialog->addTextEditor ("name", suggestion, "Preset name");
    dialog->addButton ("SAVE", 1, KeyPress (KeyPress::returnKey));
    dialog->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));
    if (auto* te = dialog->getTextEditor ("name"))
    {
        te->setInputRestrictions (64);
        te->setFont (pmx::Fonts::medium (16.0f));
        te->selectAll();
    }

    dialog->enterModalState (true, ModalCallbackFunction::create (
        [safe = Component::SafePointer<PunchmaxxxEditor> (this)] (int result)
        {
            if (safe == nullptr || safe->dialog == nullptr) return;
            const auto name = safe->dialog->getTextEditorContents ("name").trim();
            MessageManager::callAsync ([safe] { if (safe != nullptr) safe->dialog.reset(); });
            if (result != 1 || name.isEmpty()) return;

            if (safe->proc.presets.userPresetExists (name))
            {
                AlertWindow::showAsync (MessageBoxOptions().withTitle ("OVERWRITE?")
                                            .withMessage ("\"" + name + "\" already exists. Replace it with the current settings?")
                                            .withButton ("OVERWRITE").withButton ("CANCEL").withAssociatedComponent (safe.getComponent()),
                                        [safe, name] (int r) { if (safe != nullptr && r == 1) safe->doSave (name); });
            }
            else
            {
                safe->doSave (name);
            }
        }), false);
}

void PunchmaxxxEditor::doSave (const String& name)
{
    if (! proc.presets.saveUserPreset (name))
        AlertWindow::showAsync (MessageBoxOptions().withTitle ("COULDN'T SAVE")
                                    .withMessage ("Couldn't write to:\n" + proc.presets.getUserFolder().getFullPathName())
                                    .withButton ("OK").withAssociatedComponent (this), nullptr);
    refreshPresetBox();
}

void PunchmaxxxEditor::confirmDelete()
{
    const auto name = proc.presets.getCurrentName();
    AlertWindow::showAsync (MessageBoxOptions().withTitle ("DELETE PRESET?")
                                .withMessage ("Delete \"" + name + "\" from your user presets? Your current sound stays loaded.")
                                .withButton ("DELETE").withButton ("CANCEL").withAssociatedComponent (this),
                            [safe = Component::SafePointer<PunchmaxxxEditor> (this)] (int r)
                            {
                                if (safe != nullptr && r == 1) safe->proc.presets.deleteCurrentUserPreset();
                            });
}

void PunchmaxxxEditor::importPreset()
{
    chooser = std::make_unique<FileChooser> ("Load a PUNCHMAXXX preset", File::getSpecialLocation (File::userHomeDirectory),
                                             String ("*") + pmx::PresetManager::fileExtension);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe = Component::SafePointer<PunchmaxxxEditor> (this)] (const FileChooser& fc)
                          {
                              if (safe == nullptr) return;
                              const auto f = fc.getResult();
                              if (f.existsAsFile() && ! safe->proc.presets.importAndLoad (f))
                                  AlertWindow::showAsync (MessageBoxOptions().withTitle ("NOT A PUNCHMAXXX PRESET")
                                                              .withMessage (f.getFileName() + " couldn't be loaded.")
                                                              .withButton ("OK").withAssociatedComponent (safe.getComponent()), nullptr);
                          });
}

void PunchmaxxxEditor::openPresetFolder()
{
    auto dir = proc.presets.getUserFolder();
    dir.createDirectory();
    dir.startAsProcess();
}
