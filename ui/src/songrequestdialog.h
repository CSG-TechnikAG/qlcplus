/*
  Q Light Controller Plus
  songrequestdialog.h

  SongRequest connection settings and library importer.

  Licensed under the Apache License, Version 2.0
*/

#ifndef SONGREQUESTDIALOG_H
#define SONGREQUESTDIALOG_H

#include <QJsonArray>
#include <QPointer>

#include "appdialog.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class QPushButton;
class Doc;

/** @addtogroup ui UI
 *  @{
 */

class SongRequestDialog final : public AppDialog
{
    Q_OBJECT

public:
    explicit SongRequestDialog(Doc *doc, QWidget *parent = nullptr);

private slots:
    void slotStatusChanged();
    void slotLibraryReady(const QJsonArray &songs);
    void slotRequestFailed(const QString &message);
    void slotRefresh();
    void slotApplySettings();
    void slotImport();
    void slotSearchChanged(const QString &text);

private:
    void populateTable();
    void startAnalysis(class Audio *audio);
    void updateStatusLabel();

    Doc *m_doc;
    QCheckBox *m_enabledCheck;
    QLineEdit *m_urlEdit;
    QLineEdit *m_tokenEdit;
    QLabel *m_statusLabel;
    QLineEdit *m_searchEdit;
    QTableWidget *m_table;
    QCheckBox *m_analyzeCheck;
    QPushButton *m_importButton;
    QJsonArray m_songs;
};

/** @} */

#endif // SONGREQUESTDIALOG_H
