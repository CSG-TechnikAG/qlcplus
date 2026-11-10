/*
  Q Light Controller Plus
  vclivemusicproperties.h

  Configuration dialog for the Live Music virtual console widget.

  Licensed under the Apache License, Version 2.0
*/

#ifndef VCLIVEMUSICPROPERTIES_H
#define VCLIVEMUSICPROPERTIES_H

#include <QColor>
#include <QDialog>
#include <QList>

#include "vclivemusicpreset.h"

class VCLiveMusic;
class InputSelectionWidget;
class QTreeWidgetItem;
class QTreeWidget;
class QListWidget;
class QTabWidget;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QSlider;
class QLabel;
class QPushButton;
class QSpinBox;
class Doc;

/** @addtogroup ui_vc_props
 * @{
 */

class VCLiveMusicProperties final : public QDialog
{
    Q_OBJECT

public:
    VCLiveMusicProperties(VCLiveMusic *widget, Doc *doc);
    ~VCLiveMusicProperties();

    void accept() override;

private slots:
    void slotPresetSelected(int row);
    void slotAddPreset();
    void slotDuplicatePreset();
    void slotRemovePreset();
    void slotNameEdited(const QString &text);
    void slotGeneralChanged();
    void slotPrimaryColorClicked();
    void slotSecondaryColorClicked();
    void slotPrimaryClearClicked();
    void slotSecondaryClearClicked();
    void slotFixtureItemChanged(QTreeWidgetItem *item, int column);
    void slotFixtureSelectionChanged();
    void slotFixtureControlsChanged();
    void slotSelectShowChannels();
    void slotSelectAllChannels();
    void slotClearChannels();

private:
    void buildUi();
    void loadPresets();
    void updatePresetList();
    void loadPresetEditor();
    void savePresetEditor();
    void fillFixtureTree();
    void saveFixtureTree();
    void updateFixtureControls();
    LiveMusic::FixtureConfig *selectedFixtureConfig();
    quint8 nextPresetId() const;
    VCLiveMusicPreset *currentPreset() const;
    VCLiveMusicPreset *defaultPreset();
    static QString channelRole(const LiveMusic::ChannelInfo &info);
    void updateColorButtons();

private:
    VCLiveMusic *m_widget;
    Doc *m_doc;
    QList<VCLiveMusicPreset *> m_presets;
    int m_currentIndex;
    bool m_updating;

    QListWidget *m_presetList;
    QLineEdit *m_nameEdit;
    QTabWidget *m_tabs;
    QComboBox *m_styleCombo;
    QComboBox *m_colorModeCombo;
    QComboBox *m_movementCombo;
    QComboBox *m_changeBarsCombo;
    QSlider *m_energySlider;
    QSlider *m_intensitySlider;
    QSlider *m_movementRangeSlider;
    QLabel *m_energyLabel;
    QLabel *m_intensityLabel;
    QLabel *m_movementRangeLabel;
    QCheckBox *m_allowColor;
    QCheckBox *m_allowMovement;
    QCheckBox *m_allowStrobe;
    QCheckBox *m_allowGobo;
    QCheckBox *m_allowPrism;
    QCheckBox *m_allowBeam;
    QCheckBox *m_allowSmoke;
    QPushButton *m_primaryButton;
    QPushButton *m_secondaryButton;

    QTreeWidget *m_fixtureTree;
    QSpinBox *m_fixtureIntensity;
    QSpinBox *m_fixtureMovement;
    QCheckBox *m_invertPan;
    QCheckBox *m_invertTilt;

    InputSelectionWidget *m_enableInput;
    InputSelectionWidget *m_presetInput;
};

/** @} */

#endif // VCLIVEMUSICPROPERTIES_H
