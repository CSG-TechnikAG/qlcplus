/*
  Q Light Controller Plus
  preview3d.h

  "3D Preview" main tab: a live 3D view of the room and stage. The geometry is
  read from the Stage Geometry model, which mirrors the Fixture Monitor setup
  fields, so the view adapts to size changes in real time.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef PREVIEW3D_H
#define PREVIEW3D_H

#include <QWidget>

#include "preview3drenderbackend.h"

#define KXMLQLCPreview3D          QStringLiteral("Preview3D")

class Preview3DView;
class TrussPanel;
class TrussScene;
class FixturePanel;
class FixtureScene;
class QLabel;
class QToolBar;
class QAction;
class QSplitter;
class QComboBox;
class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;
class Preview3DRenderBackend;

/** @addtogroup ui_preview3d UI
 * @{
 */

class Preview3D final : public QWidget
{
    Q_OBJECT

public:
    Preview3D(QWidget *parent, Doc *doc);
    ~Preview3D();

    /** The (single) 3D Preview tab, or NULL. */
    static Preview3D *instance();

    /** The truss model (for tests / other views). */
    TrussScene *trussScene() const { return m_trussScene; }
    /** The fixture model (for tests / other views). */
    FixtureScene *fixtureScene() const { return m_fixtureScene; }

    /** Start from scratch. */
    void clearContents();

    /*********************************************************************
     * Load & Save
     *********************************************************************/
    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;

private slots:
    void slotGeometryChanged();
    void slotFit();
    void slotReset();
    void slotGridToggled(bool on);
    void slotBeamPreviewToggled(bool on);
    void slotTrussToggled(bool on);
    void slotFixtureToggled(bool on);
    void slotTrussSceneChanged();
    void slotFixtureSceneChanged();
    void slotRendererChanged(int index);
    void slotSettings();
    void slotCapturePreview();

private:
    void refresh();
    void applyRenderSettings(const Preview3DRenderSettings &settings);
    void rebuildRenderBackend();
    void updateRenderStatus();
    QString diagnosticsText() const;

    static Preview3D *s_instance;
    Doc *m_doc;
    bool m_loading;
    Preview3DView *m_view;
    TrussPanel *m_trussPanel;
    TrussScene *m_trussScene;
    FixturePanel *m_fixturePanel;
    FixtureScene *m_fixtureScene;
    QSplitter *m_splitter;
    QLabel *m_infoLabel;
    QLabel *m_renderStatusLabel;
    QToolBar *m_toolbar;
    QAction *m_fitAction;
    QAction *m_resetAction;
    QAction *m_gridAction;
    QAction *m_beamPreviewAction;
    QAction *m_trussAction;
    QAction *m_fixtureAction;
    QAction *m_settingsAction;
    QComboBox *m_rendererCombo;
    Preview3DRenderSettings m_renderSettings;
    Preview3DRenderBackend *m_renderBackend;
    Preview3DRenderer m_activeRenderer;
    QString m_renderDiagnostic;
};

/** @} */

#endif // PREVIEW3D_H
