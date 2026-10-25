/*
  Q Light Controller Plus
  songrequestdialog.cpp

  Licensed under the Apache License, Version 2.0
*/

#include <QCheckBox>
#include <QDataStream>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "songrequestdialog.h"
#include "songrequestclient.h"
#include "audioanalyzer.h"
#include "appicons.h"
#include "audio.h"
#include "doc.h"
#include "theme.h"

static QString songString(const QJsonObject &song, const QStringList &keys)
{
    for (const QString &key : keys)
    {
        const QJsonValue value = song.value(key);
        if (value.isString() && value.toString().isEmpty() == false)
            return value.toString();
    }
    return QString();
}

static QString songPath(const QJsonObject &song)
{
    return songString(song, { "filePath", "path", "localPath", "file", "filename" });
}

static QString songTitle(const QJsonObject &song)
{
    const QString title = songString(song, { "title", "name" });
    return title.isEmpty() ? songString(song, { "id" }) : title;
}

static QString songDuration(const QJsonObject &song)
{
    const QString display = songString(song, { "durationDisplay" });
    if (display.isEmpty() == false)
        return display;
    const double seconds = song.value("duration").toDouble();
    if (seconds <= 0.0)
        return QString();
    const int total = qRound(seconds);
    return QString("%1:%2").arg(total / 60, 2, 10, QChar('0'))
                            .arg(total % 60, 2, 10, QChar('0'));
}

SongRequestDialog::SongRequestDialog(Doc *doc, QWidget *parent)
    : AppDialog(tr("SongRequest Library"), parent)
    , m_doc(doc)
    , m_enabledCheck(nullptr)
    , m_urlEdit(nullptr)
    , m_tokenEdit(nullptr)
    , m_statusLabel(nullptr)
    , m_searchEdit(nullptr)
    , m_table(nullptr)
    , m_analyzeCheck(nullptr)
    , m_importButton(nullptr)
{
    setHeaderIcon("songrequest");
    setSubtitle(tr("Connect to SongRequest and import songs into QLC+ as audio functions."));
    resize(980, 650);

    QFormLayout *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(8);

    m_enabledCheck = new QCheckBox(tr("Enable SongRequest connection"), this);
    form->addRow(QString(), m_enabledCheck);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText("http://127.0.0.1:8730");
    form->addRow(tr("Server URL"), m_urlEdit);

    m_tokenEdit = new QLineEdit(this);
    m_tokenEdit->setEchoMode(QLineEdit::Password);
    m_tokenEdit->setPlaceholderText(tr("Optional bearer token"));
    form->addRow(tr("API token"), m_tokenEdit);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    form->addRow(tr("Status"), m_statusLabel);

    contentLayout()->addLayout(form);

    QHBoxLayout *searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->setSpacing(8);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Filter library by title, artist, album or genre"));
    searchRow->addWidget(m_searchEdit, 1);

    m_analyzeCheck = new QCheckBox(tr("Analyze imported songs"), this);
    m_analyzeCheck->setChecked(true);
    searchRow->addWidget(m_analyzeCheck);

    contentLayout()->addLayout(searchRow);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({ tr("Title"), tr("Artist"), tr("Album"),
                                         tr("Genre"), tr("Length"), tr("File") });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    contentLayout()->addWidget(m_table, 1);

    QPushButton *refreshButton = addFooterButton(tr("Refresh Library"), false);
    m_importButton = addFooterButton(tr("Import Selected"), true);
    QPushButton *saveButton = addFooterButton(tr("Save Connection"), false);

    connect(refreshButton, &QPushButton::clicked, this, &SongRequestDialog::slotRefresh);
    connect(m_importButton, &QPushButton::clicked, this, &SongRequestDialog::slotImport);
    connect(saveButton, &QPushButton::clicked, this, &SongRequestDialog::slotApplySettings);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SongRequestDialog::slotSearchChanged);

    SongRequestClient *client = SongRequestClient::instance();
    connect(client, &SongRequestClient::statusChanged,
            this, &SongRequestDialog::slotStatusChanged);
    connect(client, &SongRequestClient::libraryReady,
            this, &SongRequestDialog::slotLibraryReady);
    connect(client, &SongRequestClient::requestFailed,
            this, &SongRequestDialog::slotRequestFailed);

    m_enabledCheck->setChecked(client->enabled());
    m_urlEdit->setText(client->baseUrl());
    m_tokenEdit->setText(client->token());
    updateStatusLabel();

    if (client->isConfigured() == true)
    {
        slotRefresh();
    }
}

void SongRequestDialog::updateStatusLabel()
{
    SongRequestClient *client = SongRequestClient::instance();
    Theme *theme = Theme::instance();
    QColor color = theme->color(Theme::TextDim);
    QString text = client->statusText();

    switch (client->status())
    {
    case SongRequestClient::Connected:
        color = theme->color(Theme::Success);
        break;
    case SongRequestClient::Connecting:
        color = theme->color(Theme::Warning);
        break;
    case SongRequestClient::Disconnected:
        color = theme->color(Theme::Danger);
        break;
    case SongRequestClient::Disabled:
    default:
        break;
    }

    if (client->lastError().isEmpty() == false)
        text += QString(" — %1").arg(client->lastError());

    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(QString("color: %1;").arg(color.name()));
}

void SongRequestDialog::slotStatusChanged()
{
    updateStatusLabel();
    if (SongRequestClient::instance()->status() == SongRequestClient::Connected
        && m_songs.isEmpty() == true)
    {
        slotRefresh();
    }
}

void SongRequestDialog::slotLibraryReady(const QJsonArray &songs)
{
    m_songs = songs;
    m_table->clearContents();
    populateTable();
    m_statusLabel->setText(tr("Loaded %1 library song(s).").arg(songs.size()));
    m_statusLabel->setStyleSheet(QString("color: %1;")
                                 .arg(Theme::instance()->color(Theme::Success).name()));
}

void SongRequestDialog::slotRequestFailed(const QString &message)
{
    m_statusLabel->setText(message);
    m_statusLabel->setStyleSheet(QString("color: %1;")
                                 .arg(Theme::instance()->color(Theme::Danger).name()));
}

void SongRequestDialog::slotRefresh()
{
    SongRequestClient *client = SongRequestClient::instance();
    if (client->isConfigured() == false)
    {
        slotRequestFailed(tr("Enable and configure the SongRequest connection first."));
        return;
    }
    if (client->status() != SongRequestClient::Connected)
    {
        client->connectNow();
        return;
    }
    client->fetchLibrary(m_searchEdit->text());
}

void SongRequestDialog::slotApplySettings()
{
    SongRequestClient *client = SongRequestClient::instance();
    client->configure(m_enabledCheck->isChecked(), m_urlEdit->text(), m_tokenEdit->text());
    updateStatusLabel();
}

void SongRequestDialog::slotSearchChanged(const QString &text)
{
    Q_UNUSED(text)
    populateTable();
}

void SongRequestDialog::populateTable()
{
    const QString filter = m_searchEdit->text().trimmed().toLower();
    QVector<int> visible;
    visible.reserve(m_songs.size());

    for (int i = 0; i < m_songs.size(); ++i)
    {
        const QJsonObject song = m_songs.at(i).toObject();
        if (filter.isEmpty() == false)
        {
            const QString haystack = QString("%1 %2 %3 %4 %5")
                .arg(songTitle(song), songString(song, { "artist" }),
                     songString(song, { "album" }), songString(song, { "genre" }),
                     songString(song, { "message" })).toLower();
            if (haystack.contains(filter) == false)
                continue;
        }
        visible.append(i);
    }

    m_table->setRowCount(visible.size());
    for (int row = 0; row < visible.size(); ++row)
    {
        const int songIndex = visible.at(row);
        const QJsonObject song = m_songs.at(songIndex).toObject();
        const QString path = songPath(song);
        const QString title = songTitle(song);
        const QString artist = songString(song, { "artist" });
        const QString album = songString(song, { "album" });
        const QString genre = songString(song, { "genre" });
        const QString duration = songDuration(song);

        const QStringList values = { title, artist, album, genre, duration, path };
        for (int col = 0; col < values.size(); ++col)
        {
            QTableWidgetItem *item = new QTableWidgetItem(values.at(col));
            item->setData(Qt::UserRole, songIndex);
            if (col == 0)
                item->setToolTip(songString(song, { "sourceUrl", "thumbnailUrl" }));
            if (path.isEmpty() || QFileInfo::exists(path) == false)
                item->setForeground(Theme::instance()->color(Theme::Danger));
            m_table->setItem(row, col, item);
        }
    }
    m_importButton->setEnabled(visible.isEmpty() == false);
}

void SongRequestDialog::slotImport()
{
    const QList<QTableWidgetItem *> selected = m_table->selectedItems();
    if (selected.isEmpty() == true)
    {
        QMessageBox::information(this, tr("Import Songs"),
                                 tr("Select one or more library songs first."));
        return;
    }

    QSet<int> rows;
    for (QTableWidgetItem *item : selected)
        rows.insert(item->row());

    int imported = 0;
    QStringList missing;
    for (int row : rows)
    {
        QTableWidgetItem *first = m_table->item(row, 0);
        if (first == nullptr)
            continue;
        const int songIndex = first->data(Qt::UserRole).toInt();
        if (songIndex < 0 || songIndex >= m_songs.size())
            continue;
        const QJsonObject song = m_songs.at(songIndex).toObject();
        const QString path = songPath(song);
        if (path.isEmpty() || QFileInfo::exists(path) == false)
        {
            missing << songTitle(song);
            continue;
        }

        Audio *audio = new Audio(m_doc);
        if (audio->setSourceFileName(path) == false)
        {
            missing << songTitle(song);
            delete audio;
            continue;
        }

        const QString artist = songString(song, { "artist" });
        const QString title = songTitle(song);
        audio->setName(artist.isEmpty() ? title : QString("%1 - %2").arg(artist, title));
        audio->setArtist(artist);
        audio->setAlbum(songString(song, { "album" }));
        audio->setGenre(songString(song, { "genre" }));
        audio->setSongRequestId(songString(song, { "songId", "id" }));

        if (m_doc->addFunction(audio) == false)
        {
            delete audio;
            continue;
        }
        ++imported;
        if (m_analyzeCheck->isChecked() == true)
            startAnalysis(audio);
    }

    m_doc->setModified();
    if (missing.isEmpty() == false)
    {
        QMessageBox::warning(this, tr("Import Songs"),
                             tr("Imported %1 song(s). These files could not be imported:\n%2")
                             .arg(imported).arg(missing.join('\n')));
    }
    else
    {
        m_statusLabel->setText(tr("Imported %1 song(s) as Audio functions.").arg(imported));
        m_statusLabel->setStyleSheet(QString("color: %1;")
                                     .arg(Theme::instance()->color(Theme::Success).name()));
    }
}

void SongRequestDialog::startAnalysis(Audio *audio)
{
    AudioAnalyzer *analyzer = AudioAnalyzer::create(m_doc, audio);
    if (analyzer == nullptr)
        return;

    QPointer<Audio> audioGuard(audio);
    Doc *doc = m_doc;
    connect(analyzer, &AudioAnalyzer::analysisReady, audio,
            [audioGuard, doc](quint32 id, double bpm, QByteArray waveform,
                              QByteArray beats, QByteArray triggers)
    {
        Q_UNUSED(id)
        if (audioGuard.isNull())
            return;
        QVector<quint32> beatPositions;
        QDataStream beatStream(&beats, QIODevice::ReadOnly);
        while (beatStream.atEnd() == false)
        {
            quint32 value;
            beatStream >> value;
            beatPositions.append(value);
        }
        QVector<quint32> triggerPositions;
        QDataStream triggerStream(&triggers, QIODevice::ReadOnly);
        while (triggerStream.atEnd() == false)
        {
            quint32 value;
            triggerStream >> value;
            triggerPositions.append(value);
        }
        audioGuard->setAnalysis(bpm, waveform, beatPositions, triggerPositions);
        doc->setModified();
    });

    connect(analyzer, &AudioAnalyzer::analysisFailed, this,
            [this](quint32 id, const QString &reason)
    {
        Q_UNUSED(id)
        m_statusLabel->setText(reason);
        m_statusLabel->setStyleSheet(QString("color: %1;")
                                     .arg(Theme::instance()->color(Theme::Warning).name()));
    });
    connect(analyzer, &QThread::finished, analyzer, &QObject::deleteLater);
    analyzer->start();
}
