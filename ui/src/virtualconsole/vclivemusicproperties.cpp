/*
  Q Light Controller Plus
  vclivemusicproperties.cpp

  Licensed under the Apache License, Version 2.0
*/

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "vclivemusicproperties.h"
#include "inputselectionwidget.h"
#include "livemusicengine.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"
#include "vclivemusic.h"

#define KColumnName  0
#define KColumnRole  1

VCLiveMusicProperties::VCLiveMusicProperties(VCLiveMusic *widget, Doc *doc)
    : QDialog(widget)
    , m_widget(widget)
    , m_doc(doc)
    , m_currentIndex(-1)
    , m_updating(false)
{
    Q_ASSERT(widget != NULL);
    Q_ASSERT(doc != NULL);

    setWindowTitle(tr("Live Music properties"));
    resize(QSize(920, 640));
    buildUi();
    loadPresets();
}

VCLiveMusicProperties::~VCLiveMusicProperties()
{
    for (VCLiveMusicPreset *preset : m_presets)
        delete preset;
    m_presets.clear();
}

void VCLiveMusicProperties::buildUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    QWidget *left = new QWidget(splitter);
    QVBoxLayout *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 4, 0);
    leftLayout->addWidget(new QLabel(tr("Boundary presets"), left));
    m_presetList = new QListWidget(left);
    m_presetList->setMinimumWidth(190);
    leftLayout->addWidget(m_presetList, 1);

    QHBoxLayout *presetButtons = new QHBoxLayout();
    QPushButton *addButton = new QPushButton(tr("Add"), left);
    QPushButton *dupButton = new QPushButton(tr("Duplicate"), left);
    QPushButton *removeButton = new QPushButton(tr("Remove"), left);
    presetButtons->addWidget(addButton);
    presetButtons->addWidget(dupButton);
    presetButtons->addWidget(removeButton);
    leftLayout->addLayout(presetButtons);
    splitter->addWidget(left);

    m_tabs = new QTabWidget(splitter);

    // ---------------------------------------------------------------- show
    QWidget *showTab = new QWidget(m_tabs);
    QVBoxLayout *showLayout = new QVBoxLayout(showTab);

    QFormLayout *nameForm = new QFormLayout();
    m_nameEdit = new QLineEdit(showTab);
    nameForm->addRow(tr("Preset name"), m_nameEdit);
    showLayout->addLayout(nameForm);

    QGroupBox *musicGroup = new QGroupBox(tr("Music response"), showTab);
    QFormLayout *musicForm = new QFormLayout(musicGroup);
    m_styleCombo = new QComboBox(musicGroup);
    m_styleCombo->addItem(LiveMusic::styleName(LiveMusic::StyleAuto), LiveMusic::StyleAuto);
    m_styleCombo->addItem(LiveMusic::styleName(LiveMusic::StyleParty), LiveMusic::StyleParty);
    m_styleCombo->addItem(LiveMusic::styleName(LiveMusic::StyleEDM), LiveMusic::StyleEDM);
    m_styleCombo->addItem(LiveMusic::styleName(LiveMusic::StyleRock), LiveMusic::StyleRock);
    m_styleCombo->addItem(LiveMusic::styleName(LiveMusic::StylePop), LiveMusic::StylePop);
    m_styleCombo->addItem(LiveMusic::styleName(LiveMusic::StyleHipHop), LiveMusic::StyleHipHop);
    m_styleCombo->addItem(LiveMusic::styleName(LiveMusic::StyleMetal), LiveMusic::StyleMetal);
    m_styleCombo->addItem(LiveMusic::styleName(LiveMusic::StyleJazz), LiveMusic::StyleJazz);
    m_styleCombo->addItem(LiveMusic::styleName(LiveMusic::StyleReggae), LiveMusic::StyleReggae);
    m_styleCombo->addItem(LiveMusic::styleName(LiveMusic::StyleAcoustic), LiveMusic::StyleAcoustic);
    musicForm->addRow(tr("Music style"), m_styleCombo);

    m_changeBarsCombo = new QComboBox(musicGroup);
    m_changeBarsCombo->addItem(tr("Every bar"), 1);
    m_changeBarsCombo->addItem(tr("Every 2 bars"), 2);
    m_changeBarsCombo->addItem(tr("Every 4 bars"), 4);
    m_changeBarsCombo->addItem(tr("Every 8 bars"), 8);
    m_changeBarsCombo->addItem(tr("Every 16 bars"), 16);
    musicForm->addRow(tr("Change look"), m_changeBarsCombo);

    QWidget *energyRow = new QWidget(musicGroup);
    QHBoxLayout *energyLayout = new QHBoxLayout(energyRow);
    energyLayout->setContentsMargins(0, 0, 0, 0);
    m_energySlider = new QSlider(Qt::Horizontal, energyRow);
    m_energySlider->setRange(0, 100);
    m_energyLabel = new QLabel(energyRow);
    m_energyLabel->setMinimumWidth(38);
    energyLayout->addWidget(m_energySlider, 1);
    energyLayout->addWidget(m_energyLabel);
    musicForm->addRow(tr("Energy"), energyRow);
    showLayout->addWidget(musicGroup);

    QGroupBox *colorGroup = new QGroupBox(tr("Colour"), showTab);
    QFormLayout *colorForm = new QFormLayout(colorGroup);
    m_colorModeCombo = new QComboBox(colorGroup);
    m_colorModeCombo->addItem(LiveMusic::colorModeName(LiveMusic::ColorAuto), LiveMusic::ColorAuto);
    m_colorModeCombo->addItem(LiveMusic::colorModeName(LiveMusic::ColorRainbow), LiveMusic::ColorRainbow);
    m_colorModeCombo->addItem(LiveMusic::colorModeName(LiveMusic::ColorDuotone), LiveMusic::ColorDuotone);
    m_colorModeCombo->addItem(LiveMusic::colorModeName(LiveMusic::ColorWarm), LiveMusic::ColorWarm);
    m_colorModeCombo->addItem(LiveMusic::colorModeName(LiveMusic::ColorCool), LiveMusic::ColorCool);
    m_colorModeCombo->addItem(LiveMusic::colorModeName(LiveMusic::ColorSingle), LiveMusic::ColorSingle);
    colorForm->addRow(tr("Colour mode"), m_colorModeCombo);

    QWidget *colorRow = new QWidget(colorGroup);
    QHBoxLayout *colorLayout = new QHBoxLayout(colorRow);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    m_primaryButton = new QPushButton(tr("Primary"), colorRow);
    m_secondaryButton = new QPushButton(tr("Secondary"), colorRow);
    QPushButton *primaryAuto = new QPushButton(tr("Auto"), colorRow);
    QPushButton *secondaryAuto = new QPushButton(tr("Auto"), colorRow);
    primaryAuto->setMaximumWidth(54);
    secondaryAuto->setMaximumWidth(54);
    colorLayout->addWidget(m_primaryButton);
    colorLayout->addWidget(primaryAuto);
    colorLayout->addWidget(m_secondaryButton);
    colorLayout->addWidget(secondaryAuto);
    colorLayout->addStretch(1);
    colorForm->addRow(tr("Duotone colours"), colorRow);
    m_allowColor = new QCheckBox(tr("Use colour / colour wheels"), colorGroup);
    colorForm->addRow(QString(), m_allowColor);
    showLayout->addWidget(colorGroup);

    QGroupBox *movementGroup = new QGroupBox(tr("Movement"), showTab);
    QFormLayout *movementForm = new QFormLayout(movementGroup);
    m_movementCombo = new QComboBox(movementGroup);
    m_movementCombo->addItem(LiveMusic::movementStyleName(LiveMusic::MoveAuto), LiveMusic::MoveAuto);
    m_movementCombo->addItem(LiveMusic::movementStyleName(LiveMusic::MoveSweep), LiveMusic::MoveSweep);
    m_movementCombo->addItem(LiveMusic::movementStyleName(LiveMusic::MoveCircle), LiveMusic::MoveCircle);
    m_movementCombo->addItem(LiveMusic::movementStyleName(LiveMusic::MoveFan), LiveMusic::MoveFan);
    m_movementCombo->addItem(LiveMusic::movementStyleName(LiveMusic::MoveFigure8), LiveMusic::MoveFigure8);
    m_movementCombo->addItem(LiveMusic::movementStyleName(LiveMusic::MoveBallyhoo), LiveMusic::MoveBallyhoo);
    m_movementCombo->addItem(LiveMusic::movementStyleName(LiveMusic::MoveStatic), LiveMusic::MoveStatic);
    movementForm->addRow(tr("Pattern"), m_movementCombo);

    QWidget *rangeRow = new QWidget(movementGroup);
    QHBoxLayout *rangeLayout = new QHBoxLayout(rangeRow);
    rangeLayout->setContentsMargins(0, 0, 0, 0);
    m_movementRangeSlider = new QSlider(Qt::Horizontal, rangeRow);
    m_movementRangeSlider->setRange(0, 100);
    m_movementRangeLabel = new QLabel(rangeRow);
    m_movementRangeLabel->setMinimumWidth(38);
    rangeLayout->addWidget(m_movementRangeSlider, 1);
    rangeLayout->addWidget(m_movementRangeLabel);
    movementForm->addRow(tr("Range"), rangeRow);
    m_allowMovement = new QCheckBox(tr("Use pan / tilt movement"), movementGroup);
    movementForm->addRow(QString(), m_allowMovement);
    showLayout->addWidget(movementGroup);

    QGroupBox *effectsGroup = new QGroupBox(tr("Stage effects"), showTab);
    QGridLayout *effectsLayout = new QGridLayout(effectsGroup);
    m_allowStrobe = new QCheckBox(tr("Strobe"), effectsGroup);
    m_allowGobo = new QCheckBox(tr("Gobos"), effectsGroup);
    m_allowPrism = new QCheckBox(tr("Prism"), effectsGroup);
    m_allowBeam = new QCheckBox(tr("Beam / zoom / focus"), effectsGroup);
    m_allowSmoke = new QCheckBox(tr("Smoke / haze"), effectsGroup);
    effectsLayout->addWidget(m_allowStrobe, 0, 0);
    effectsLayout->addWidget(m_allowGobo, 0, 1);
    effectsLayout->addWidget(m_allowPrism, 0, 2);
    effectsLayout->addWidget(m_allowBeam, 1, 0);
    effectsLayout->addWidget(m_allowSmoke, 1, 1);
    showLayout->addWidget(effectsGroup);

    QGroupBox *limitGroup = new QGroupBox(tr("Boundaries"), showTab);
    QFormLayout *limitForm = new QFormLayout(limitGroup);
    QWidget *intensityRow = new QWidget(limitGroup);
    QHBoxLayout *intensityLayout = new QHBoxLayout(intensityRow);
    intensityLayout->setContentsMargins(0, 0, 0, 0);
    m_intensitySlider = new QSlider(Qt::Horizontal, intensityRow);
    m_intensitySlider->setRange(0, 100);
    m_intensityLabel = new QLabel(intensityRow);
    m_intensityLabel->setMinimumWidth(38);
    intensityLayout->addWidget(m_intensitySlider, 1);
    intensityLayout->addWidget(m_intensityLabel);
    limitForm->addRow(tr("Max intensity"), intensityRow);
    showLayout->addWidget(limitGroup);
    showLayout->addStretch(1);
    m_tabs->addTab(showTab, tr("Live show"));

    // ------------------------------------------------------------ fixtures
    QWidget *fixtureTab = new QWidget(m_tabs);
    QVBoxLayout *fixtureLayout = new QVBoxLayout(fixtureTab);
    fixtureLayout->addWidget(new QLabel(tr("Tick the fixtures and channels this preset may drive:"), fixtureTab));
    m_fixtureTree = new QTreeWidget(fixtureTab);
    m_fixtureTree->setColumnCount(2);
    m_fixtureTree->setHeaderLabels(QStringList() << tr("Fixture / channel") << tr("Role"));
    m_fixtureTree->header()->setSectionResizeMode(KColumnName, QHeaderView::Stretch);
    m_fixtureTree->header()->setSectionResizeMode(KColumnRole, QHeaderView::ResizeToContents);
    m_fixtureTree->setUniformRowHeights(true);
    fixtureLayout->addWidget(m_fixtureTree, 1);

    QHBoxLayout *treeButtons = new QHBoxLayout();
    QPushButton *showChannels = new QPushButton(tr("Show channels only"), fixtureTab);
    QPushButton *allChannels = new QPushButton(tr("All channels"), fixtureTab);
    QPushButton *clearChannels = new QPushButton(tr("Clear"), fixtureTab);
    treeButtons->addWidget(showChannels);
    treeButtons->addWidget(allChannels);
    treeButtons->addWidget(clearChannels);
    treeButtons->addStretch(1);
    fixtureLayout->addLayout(treeButtons);

    QGroupBox *fixtureLimits = new QGroupBox(tr("Selected fixture boundaries"), fixtureTab);
    QFormLayout *fixtureForm = new QFormLayout(fixtureLimits);
    m_fixtureIntensity = new QSpinBox(fixtureLimits);
    m_fixtureIntensity->setRange(0, 100);
    m_fixtureIntensity->setSuffix(" %");
    m_fixtureMovement = new QSpinBox(fixtureLimits);
    m_fixtureMovement->setRange(0, 100);
    m_fixtureMovement->setSuffix(" %");
    m_invertPan = new QCheckBox(tr("Invert pan"), fixtureLimits);
    m_invertTilt = new QCheckBox(tr("Invert tilt"), fixtureLimits);
    fixtureForm->addRow(tr("Max intensity"), m_fixtureIntensity);
    fixtureForm->addRow(tr("Movement range"), m_fixtureMovement);
    fixtureForm->addRow(QString(), m_invertPan);
    fixtureForm->addRow(QString(), m_invertTilt);
    fixtureLayout->addWidget(fixtureLimits);
    m_tabs->addTab(fixtureTab, tr("Fixtures && channels"));

    // --------------------------------------------------------------- input
    QWidget *inputTab = new QWidget(m_tabs);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputTab);
    QLabel *inputNote = new QLabel(tr("On/off, preset selection and keyboard shortcuts can all be driven by an external controller."), inputTab);
    inputNote->setWordWrap(true);
    inputLayout->addWidget(inputNote);

    m_enableInput = new InputSelectionWidget(m_doc, inputTab);
    m_enableInput->setTitle(tr("Live show on/off"));
    m_enableInput->setCustomFeedbackVisibility(true);
    m_enableInput->setInputSource(m_widget->enableInputSource());
    m_enableInput->setKeySequence(m_widget->enableKeySequence());
    m_enableInput->setWidgetPage(m_widget->page());
    inputLayout->addWidget(m_enableInput);

    m_presetInput = new InputSelectionWidget(m_doc, inputTab);
    m_presetInput->setTitle(tr("Selected preset"));
    m_presetInput->setCustomFeedbackVisibility(true);
    m_presetInput->setWidgetPage(m_widget->page());
    inputLayout->addWidget(m_presetInput);

    inputLayout->addStretch(1);
    m_tabs->addTab(inputTab, tr("External input"));

    splitter->addWidget(m_tabs);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(splitter, 1);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &VCLiveMusicProperties::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &VCLiveMusicProperties::reject);
    mainLayout->addWidget(buttonBox);

    connect(m_presetList, &QListWidget::currentRowChanged,
            this, &VCLiveMusicProperties::slotPresetSelected);
    connect(addButton, &QPushButton::clicked, this, &VCLiveMusicProperties::slotAddPreset);
    connect(dupButton, &QPushButton::clicked, this, &VCLiveMusicProperties::slotDuplicatePreset);
    connect(removeButton, &QPushButton::clicked, this, &VCLiveMusicProperties::slotRemovePreset);
    connect(m_nameEdit, &QLineEdit::textEdited, this, &VCLiveMusicProperties::slotNameEdited);

    connect(m_styleCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &VCLiveMusicProperties::slotGeneralChanged);
    connect(m_colorModeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &VCLiveMusicProperties::slotGeneralChanged);
    connect(m_movementCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &VCLiveMusicProperties::slotGeneralChanged);
    connect(m_changeBarsCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &VCLiveMusicProperties::slotGeneralChanged);
    connect(m_energySlider, &QSlider::valueChanged, this, &VCLiveMusicProperties::slotGeneralChanged);
    connect(m_intensitySlider, &QSlider::valueChanged, this, &VCLiveMusicProperties::slotGeneralChanged);
    connect(m_movementRangeSlider, &QSlider::valueChanged, this, &VCLiveMusicProperties::slotGeneralChanged);
    connect(m_allowColor, &QCheckBox::toggled, this, &VCLiveMusicProperties::slotGeneralChanged);
    connect(m_allowMovement, &QCheckBox::toggled, this, &VCLiveMusicProperties::slotGeneralChanged);
    connect(m_allowStrobe, &QCheckBox::toggled, this, &VCLiveMusicProperties::slotGeneralChanged);
    connect(m_allowGobo, &QCheckBox::toggled, this, &VCLiveMusicProperties::slotGeneralChanged);
    connect(m_allowPrism, &QCheckBox::toggled, this, &VCLiveMusicProperties::slotGeneralChanged);
    connect(m_allowBeam, &QCheckBox::toggled, this, &VCLiveMusicProperties::slotGeneralChanged);
    connect(m_allowSmoke, &QCheckBox::toggled, this, &VCLiveMusicProperties::slotGeneralChanged);
    connect(m_primaryButton, &QPushButton::clicked, this, &VCLiveMusicProperties::slotPrimaryColorClicked);
    connect(m_secondaryButton, &QPushButton::clicked, this, &VCLiveMusicProperties::slotSecondaryColorClicked);
    connect(primaryAuto, &QPushButton::clicked, this, &VCLiveMusicProperties::slotPrimaryClearClicked);
    connect(secondaryAuto, &QPushButton::clicked, this, &VCLiveMusicProperties::slotSecondaryClearClicked);

    connect(m_fixtureTree, &QTreeWidget::itemChanged,
            this, &VCLiveMusicProperties::slotFixtureItemChanged);
    connect(m_fixtureTree, &QTreeWidget::itemSelectionChanged,
            this, &VCLiveMusicProperties::slotFixtureSelectionChanged);
    connect(m_fixtureIntensity, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &VCLiveMusicProperties::slotFixtureControlsChanged);
    connect(m_fixtureMovement, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &VCLiveMusicProperties::slotFixtureControlsChanged);
    connect(m_invertPan, &QCheckBox::toggled, this, &VCLiveMusicProperties::slotFixtureControlsChanged);
    connect(m_invertTilt, &QCheckBox::toggled, this, &VCLiveMusicProperties::slotFixtureControlsChanged);
    connect(showChannels, &QPushButton::clicked, this, &VCLiveMusicProperties::slotSelectShowChannels);
    connect(allChannels, &QPushButton::clicked, this, &VCLiveMusicProperties::slotSelectAllChannels);
    connect(clearChannels, &QPushButton::clicked, this, &VCLiveMusicProperties::slotClearChannels);

    updateColorButtons();
}

void VCLiveMusicProperties::loadPresets()
{
    for (VCLiveMusicPreset *preset : m_presets)
        delete preset;
    m_presets.clear();

    for (VCLiveMusicPreset *preset : m_widget->presets())
        m_presets.append(new VCLiveMusicPreset(*preset));

    if (m_presets.isEmpty() && m_doc->fixtures().isEmpty() == false)
    {
        VCLiveMusicPreset *preset = defaultPreset();
        preset->m_name = tr("Full stage");
        m_presets.append(preset);
    }

    m_currentIndex = m_widget->currentPresetIndex();
    if (m_currentIndex < 0 || m_currentIndex >= m_presets.size())
        m_currentIndex = m_presets.isEmpty() ? -1 : 0;

    updatePresetList();
    loadPresetEditor();
}

VCLiveMusicPreset *VCLiveMusicProperties::currentPreset() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_presets.size())
        return NULL;
    return m_presets.at(m_currentIndex);
}

VCLiveMusicPreset *VCLiveMusicProperties::defaultPreset()
{
    VCLiveMusicPreset *preset = new VCLiveMusicPreset(nextPresetId());
    preset->m_name = tr("Preset %1").arg(m_presets.size() + 1);
    for (Fixture *fixture : m_doc->fixtures())
    {
        LiveMusic::FixtureConfig config;
        config.fixtureId = fixture->id();
        config.allChannels = true;
        preset->m_fixtures.append(config);
    }
    return preset;
}

quint8 VCLiveMusicProperties::nextPresetId() const
{
    quint8 id = VCLiveMusic::presetInputBaseId;
    QSet<quint8> used;
    for (VCLiveMusicPreset *preset : m_presets)
        used.insert(preset->m_id);
    while (used.contains(id) && id < 254)
        id++;
    return id;
}

void VCLiveMusicProperties::updatePresetList()
{
    m_updating = true;
    m_presetList->clear();
    for (VCLiveMusicPreset *preset : m_presets)
    {
        QListWidgetItem *item = new QListWidgetItem(preset->m_name, m_presetList);
        item->setToolTip(tr("Preset ID %1").arg(preset->m_id));
    }
    if (m_currentIndex >= 0 && m_currentIndex < m_presetList->count())
        m_presetList->setCurrentRow(m_currentIndex);
    m_updating = false;
}

void VCLiveMusicProperties::loadPresetEditor()
{
    VCLiveMusicPreset *preset = currentPreset();
    const bool enabled = (preset != NULL);
    m_tabs->setEnabled(enabled);
    if (preset == NULL)
        return;

    m_updating = true;
    m_nameEdit->setText(preset->m_name);
    m_styleCombo->setCurrentIndex(m_styleCombo->findData(preset->m_settings.style));
    m_colorModeCombo->setCurrentIndex(m_colorModeCombo->findData(preset->m_settings.colorMode));
    m_movementCombo->setCurrentIndex(m_movementCombo->findData(preset->m_settings.movementStyle));
    int changeIndex = m_changeBarsCombo->findData(preset->m_settings.changeBars);
    if (changeIndex < 0)
        changeIndex = m_changeBarsCombo->findData(4);
    m_changeBarsCombo->setCurrentIndex(changeIndex);
    m_energySlider->setValue(preset->m_settings.energy);
    m_intensitySlider->setValue(preset->m_settings.intensityLimit);
    m_movementRangeSlider->setValue(preset->m_settings.movementRange);
    m_allowColor->setChecked(preset->m_settings.allowColor);
    m_allowMovement->setChecked(preset->m_settings.allowMovement);
    m_allowStrobe->setChecked(preset->m_settings.allowStrobe);
    m_allowGobo->setChecked(preset->m_settings.allowGobo);
    m_allowPrism->setChecked(preset->m_settings.allowPrism);
    m_allowBeam->setChecked(preset->m_settings.allowBeam);
    m_allowSmoke->setChecked(preset->m_settings.allowSmoke);
    m_energyLabel->setText(QString::number(preset->m_settings.energy) + "%");
    m_intensityLabel->setText(QString::number(preset->m_settings.intensityLimit) + "%");
    m_movementRangeLabel->setText(QString::number(preset->m_settings.movementRange) + "%");

    m_presetInput->setInputSource(preset->m_inputSource);
    m_presetInput->setKeySequence(preset->m_keySequence);
    m_updating = false;

    updateColorButtons();
    fillFixtureTree();
    updateFixtureControls();
}

void VCLiveMusicProperties::savePresetEditor()
{
    VCLiveMusicPreset *preset = currentPreset();
    if (preset == NULL)
        return;

    preset->m_name = m_nameEdit->text().trimmed();
    if (preset->m_name.isEmpty())
        preset->m_name = tr("Preset %1").arg(m_currentIndex + 1);
    preset->m_settings.style = m_styleCombo->currentData().toInt();
    preset->m_settings.colorMode = m_colorModeCombo->currentData().toInt();
    preset->m_settings.movementStyle = m_movementCombo->currentData().toInt();
    preset->m_settings.changeBars = m_changeBarsCombo->currentData().toInt();
    preset->m_settings.energy = m_energySlider->value();
    preset->m_settings.intensityLimit = m_intensitySlider->value();
    preset->m_settings.movementRange = m_movementRangeSlider->value();
    preset->m_settings.allowColor = m_allowColor->isChecked();
    preset->m_settings.allowMovement = m_allowMovement->isChecked();
    preset->m_settings.allowStrobe = m_allowStrobe->isChecked();
    preset->m_settings.allowGobo = m_allowGobo->isChecked();
    preset->m_settings.allowPrism = m_allowPrism->isChecked();
    preset->m_settings.allowBeam = m_allowBeam->isChecked();
    preset->m_settings.allowSmoke = m_allowSmoke->isChecked();
    preset->m_inputSource = m_presetInput->inputSource();
    preset->m_keySequence = m_presetInput->keySequence();

    saveFixtureTree();
}

void VCLiveMusicProperties::slotPresetSelected(int row)
{
    if (m_updating || row == m_currentIndex)
        return;
    savePresetEditor();
    if (row >= 0 && row < m_presets.size())
        m_currentIndex = row;
    loadPresetEditor();
}

void VCLiveMusicProperties::slotAddPreset()
{
    savePresetEditor();
    VCLiveMusicPreset *preset = defaultPreset();
    if (currentPreset() != NULL)
        preset->m_settings = currentPreset()->m_settings;
    m_presets.append(preset);
    m_currentIndex = m_presets.size() - 1;
    updatePresetList();
    loadPresetEditor();
}

void VCLiveMusicProperties::slotDuplicatePreset()
{
    VCLiveMusicPreset *preset = currentPreset();
    if (preset == NULL)
        return;
    savePresetEditor();
    VCLiveMusicPreset *copy = new VCLiveMusicPreset(*preset);
    copy->m_id = nextPresetId();
    copy->m_name = tr("%1 copy").arg(preset->m_name);
    copy->m_inputSource.clear();
    copy->m_keySequence = QKeySequence();
    m_presets.append(copy);
    m_currentIndex = m_presets.size() - 1;
    updatePresetList();
    loadPresetEditor();
}

void VCLiveMusicProperties::slotRemovePreset()
{
    if (currentPreset() == NULL)
        return;
    delete m_presets.takeAt(m_currentIndex);
    if (m_presets.isEmpty())
        m_currentIndex = -1;
    else if (m_currentIndex >= m_presets.size())
        m_currentIndex = m_presets.size() - 1;
    updatePresetList();
    loadPresetEditor();
}

void VCLiveMusicProperties::slotNameEdited(const QString &text)
{
    VCLiveMusicPreset *preset = currentPreset();
    if (preset == NULL || m_updating)
        return;
    preset->m_name = text;
    if (m_currentIndex >= 0 && m_currentIndex < m_presetList->count())
    {
        m_updating = true;
        m_presetList->item(m_currentIndex)->setText(text);
        m_updating = false;
    }
}

void VCLiveMusicProperties::slotGeneralChanged()
{
    if (m_updating || currentPreset() == NULL)
        return;

    m_energyLabel->setText(QString::number(m_energySlider->value()) + "%");
    m_intensityLabel->setText(QString::number(m_intensitySlider->value()) + "%");
    m_movementRangeLabel->setText(QString::number(m_movementRangeSlider->value()) + "%");
    savePresetEditor();
}

void VCLiveMusicProperties::updateColorButtons()
{
    VCLiveMusicPreset *preset = currentPreset();
    if (preset == NULL)
        return;

    const QColor primary = preset->m_settings.primaryColor;
    const QColor secondary = preset->m_settings.secondaryColor;
    m_primaryButton->setText(primary.isValid() ? primary.name() : tr("Auto"));
    m_secondaryButton->setText(secondary.isValid() ? secondary.name() : tr("Auto"));
    m_primaryButton->setStyleSheet(primary.isValid()
            ? QString("background-color: %1; color: %2;").arg(primary.name(),
                    (primary.lightness() > 140 ? "#000000" : "#ffffff")) : QString());
    m_secondaryButton->setStyleSheet(secondary.isValid()
            ? QString("background-color: %1; color: %2;").arg(secondary.name(),
                    (secondary.lightness() > 140 ? "#000000" : "#ffffff")) : QString());
}

void VCLiveMusicProperties::slotPrimaryColorClicked()
{
    VCLiveMusicPreset *preset = currentPreset();
    if (preset == NULL)
        return;
    QColor color = QColorDialog::getColor(preset->m_settings.primaryColor.isValid()
                                          ? preset->m_settings.primaryColor : QColor(Qt::white),
                                          this, tr("Primary colour"));
    if (color.isValid())
    {
        preset->m_settings.primaryColor = color;
        updateColorButtons();
    }
}

void VCLiveMusicProperties::slotSecondaryColorClicked()
{
    VCLiveMusicPreset *preset = currentPreset();
    if (preset == NULL)
        return;
    QColor color = QColorDialog::getColor(preset->m_settings.secondaryColor.isValid()
                                          ? preset->m_settings.secondaryColor : QColor(Qt::cyan),
                                          this, tr("Secondary colour"));
    if (color.isValid())
    {
        preset->m_settings.secondaryColor = color;
        updateColorButtons();
    }
}

void VCLiveMusicProperties::slotPrimaryClearClicked()
{
    if (currentPreset() == NULL)
        return;
    currentPreset()->m_settings.primaryColor = QColor();
    updateColorButtons();
}

void VCLiveMusicProperties::slotSecondaryClearClicked()
{
    if (currentPreset() == NULL)
        return;
    currentPreset()->m_settings.secondaryColor = QColor();
    updateColorButtons();
}

QString VCLiveMusicProperties::channelRole(const LiveMusic::ChannelInfo &info)
{
    QString role = QLCChannel::groupToString(info.group);
    if (info.colour != QLCChannel::NoColour)
        role += " · " + QLCChannel::colourToString(info.colour);
    if (info.preset != QLCChannel::Custom)
        role += " · " + QLCChannel::presetToString(info.preset);
    return role;
}

void VCLiveMusicProperties::fillFixtureTree()
{
    VCLiveMusicPreset *preset = currentPreset();
    if (preset == NULL)
        return;

    m_updating = true;
    m_fixtureTree->clear();

    LiveMusic::LiveMusicEngine engine(m_doc);

    for (Fixture *fixture : m_doc->fixtures())
    {
        QTreeWidgetItem *fixtureItem = new QTreeWidgetItem(m_fixtureTree);
        fixtureItem->setText(KColumnName, fixture->name());
        fixtureItem->setData(KColumnName, Qt::UserRole, fixture->id());
        fixtureItem->setFlags(fixtureItem->flags() | Qt::ItemIsUserCheckable);

        const LiveMusic::FixtureConfig *config = preset->fixtureConfig(fixture->id());
        fixtureItem->setCheckState(KColumnName,
                                   config != NULL ? Qt::Checked : Qt::Unchecked);
        fixtureItem->setText(KColumnRole, tr("%1 channels").arg(fixture->channels()));

        const QList<LiveMusic::ChannelInfo> infos = engine.channelInfos(fixture->id());
        for (const LiveMusic::ChannelInfo &info : infos)
        {
            QTreeWidgetItem *channelItem = new QTreeWidgetItem(fixtureItem);
            channelItem->setText(KColumnName, tr("%1: %2").arg(info.index).arg(info.name));
            channelItem->setData(KColumnName, Qt::UserRole, info.index);
            channelItem->setText(KColumnRole, channelRole(info));
            channelItem->setFlags(channelItem->flags() | Qt::ItemIsUserCheckable);
            bool checked = false;
            if (config != NULL)
            {
                checked = config->allChannels
                        || config->allowedChannels.contains(info.index);
            }
            channelItem->setCheckState(KColumnName, checked ? Qt::Checked : Qt::Unchecked);
        }
        fixtureItem->setExpanded(config != NULL);
    }

    m_updating = false;
}

void VCLiveMusicProperties::saveFixtureTree()
{
    VCLiveMusicPreset *preset = currentPreset();
    if (preset == NULL)
        return;

    QList<LiveMusic::FixtureConfig> newConfigs;
    for (int i = 0; i < m_fixtureTree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *fixtureItem = m_fixtureTree->topLevelItem(i);
        if (fixtureItem->checkState(KColumnName) != Qt::Checked)
            continue;

        const quint32 fixtureId = fixtureItem->data(KColumnName, Qt::UserRole).toUInt();
        LiveMusic::FixtureConfig config;
        const LiveMusic::FixtureConfig *old = preset->fixtureConfig(fixtureId);
        if (old != NULL)
            config = *old;
        else
            config.fixtureId = fixtureId;

        QSet<quint32> allowed;
        bool allChecked = true;
        int childCount = 0;
        for (int c = 0; c < fixtureItem->childCount(); c++)
        {
            QTreeWidgetItem *channelItem = fixtureItem->child(c);
            childCount++;
            if (channelItem->checkState(KColumnName) == Qt::Checked)
                allowed.insert(channelItem->data(KColumnName, Qt::UserRole).toUInt());
            else
                allChecked = false;
        }
        config.allChannels = (childCount > 0 && allChecked);
        config.allowedChannels = config.allChannels ? QSet<quint32>() : allowed;
        newConfigs.append(config);
    }

    preset->m_fixtures = newConfigs;
}

void VCLiveMusicProperties::slotFixtureItemChanged(QTreeWidgetItem *item, int column)
{
    if (m_updating || column != KColumnName || currentPreset() == NULL)
        return;

    m_updating = true;
    if (item->parent() == NULL)
    {
        // Fixture include checkbox: mirror the state to its channels.
        const Qt::CheckState state = item->checkState(KColumnName);
        for (int i = 0; i < item->childCount(); i++)
            item->child(i)->setCheckState(KColumnName, state);
    }
    else
    {
        QTreeWidgetItem *parent = item->parent();
        bool anyChecked = false;
        for (int i = 0; i < parent->childCount(); i++)
        {
            if (parent->child(i)->checkState(KColumnName) == Qt::Checked)
            {
                anyChecked = true;
                break;
            }
        }
        parent->setCheckState(KColumnName, anyChecked ? Qt::Checked : Qt::Unchecked);
    }
    m_updating = false;

    saveFixtureTree();
    updateFixtureControls();
}

void VCLiveMusicProperties::slotFixtureSelectionChanged()
{
    updateFixtureControls();
}

LiveMusic::FixtureConfig *VCLiveMusicProperties::selectedFixtureConfig()
{
    VCLiveMusicPreset *preset = currentPreset();
    if (preset == NULL)
        return NULL;
    QTreeWidgetItem *item = m_fixtureTree->currentItem();
    if (item == NULL)
        return NULL;
    QTreeWidgetItem *top = item->parent() != NULL ? item->parent() : item;
    if (top->checkState(KColumnName) != Qt::Checked)
        return NULL;
    const quint32 fixtureId = top->data(KColumnName, Qt::UserRole).toUInt();
    return preset->fixtureConfig(fixtureId);
}

void VCLiveMusicProperties::updateFixtureControls()
{
    LiveMusic::FixtureConfig *config = selectedFixtureConfig();
    m_updating = true;
    m_fixtureIntensity->setEnabled(config != NULL);
    m_fixtureMovement->setEnabled(config != NULL);
    m_invertPan->setEnabled(config != NULL);
    m_invertTilt->setEnabled(config != NULL);
    if (config != NULL)
    {
        m_fixtureIntensity->setValue(config->intensityLimit);
        m_fixtureMovement->setValue(config->movementRange);
        m_invertPan->setChecked(config->invertPan);
        m_invertTilt->setChecked(config->invertTilt);
    }
    m_updating = false;
}

void VCLiveMusicProperties::slotFixtureControlsChanged()
{
    if (m_updating)
        return;
    LiveMusic::FixtureConfig *config = selectedFixtureConfig();
    if (config == NULL)
        return;
    config->intensityLimit = m_fixtureIntensity->value();
    config->movementRange = m_fixtureMovement->value();
    config->invertPan = m_invertPan->isChecked();
    config->invertTilt = m_invertTilt->isChecked();
}

void VCLiveMusicProperties::slotSelectShowChannels()
{
    QTreeWidgetItem *item = m_fixtureTree->currentItem();
    if (item == NULL)
        return;
    QTreeWidgetItem *top = item->parent() != NULL ? item->parent() : item;
    const quint32 fixtureId = top->data(KColumnName, Qt::UserRole).toUInt();

    LiveMusic::LiveMusicEngine engine(m_doc);
    const QMap<QString, QList<quint32> > roles = engine.defaultRoles(fixtureId);
    QSet<quint32> showChannels;
    for (const QList<quint32> &list : roles)
        for (quint32 channel : list)
            showChannels.insert(channel);

    m_updating = true;
    top->setCheckState(KColumnName, Qt::Checked);
    for (int i = 0; i < top->childCount(); i++)
    {
        const quint32 channel = top->child(i)->data(KColumnName, Qt::UserRole).toUInt();
        top->child(i)->setCheckState(KColumnName,
                showChannels.contains(channel) ? Qt::Checked : Qt::Unchecked);
    }
    m_updating = false;
    saveFixtureTree();
    updateFixtureControls();
}

void VCLiveMusicProperties::slotSelectAllChannels()
{
    QTreeWidgetItem *item = m_fixtureTree->currentItem();
    if (item == NULL)
        return;
    QTreeWidgetItem *top = item->parent() != NULL ? item->parent() : item;
    m_updating = true;
    top->setCheckState(KColumnName, Qt::Checked);
    for (int i = 0; i < top->childCount(); i++)
        top->child(i)->setCheckState(KColumnName, Qt::Checked);
    m_updating = false;
    saveFixtureTree();
    updateFixtureControls();
}

void VCLiveMusicProperties::slotClearChannels()
{
    QTreeWidgetItem *item = m_fixtureTree->currentItem();
    if (item == NULL)
        return;
    QTreeWidgetItem *top = item->parent() != NULL ? item->parent() : item;
    m_updating = true;
    top->setCheckState(KColumnName, Qt::Unchecked);
    for (int i = 0; i < top->childCount(); i++)
        top->child(i)->setCheckState(KColumnName, Qt::Unchecked);
    m_updating = false;
    saveFixtureTree();
    updateFixtureControls();
}

void VCLiveMusicProperties::accept()
{
    savePresetEditor();

    m_widget->resetPresets();
    for (VCLiveMusicPreset *preset : m_presets)
        m_widget->addPreset(*preset);
    m_widget->setCurrentPresetIndex(m_currentIndex);

    m_widget->setInputSource(m_enableInput->inputSource(),
                             VCLiveMusic::enableInputSourceId);
    m_widget->setEnableKeySequence(m_enableInput->keySequence());

    QDialog::accept();
}
