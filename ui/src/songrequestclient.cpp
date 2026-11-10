/*
  Q Light Controller Plus
  songrequestclient.cpp

  Licensed under the Apache License, Version 2.0
*/

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include "songrequestclient.h"

#define SETTINGS_ENABLED "SongRequest/enabled"
#define SETTINGS_URL     "SongRequest/url"
#define SETTINGS_TOKEN   "SongRequest/token"
#define DEFAULT_URL      "http://127.0.0.1:8730"

SongRequestClient *SongRequestClient::s_instance = NULL;

SongRequestClient *SongRequestClient::instance()
{
    if (s_instance == NULL)
        s_instance = new SongRequestClient();
    return s_instance;
}

SongRequestClient::SongRequestClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_pollTimer(new QTimer(this))
    , m_playerTimer(new QTimer(this))
    , m_statusReply(NULL)
    , m_libraryReply(NULL)
    , m_playerReply(NULL)
    , m_analysisReply(NULL)
    , m_enabled(false)
    , m_baseUrl(DEFAULT_URL)
    , m_status(Disabled)
    , m_liveTracking(false)
    , m_livePollInterval(250)
    , m_analysisStage(0)
{
    qRegisterMetaType<LiveMusic::Playback>("LiveMusic::Playback");
    qRegisterMetaType<LiveMusic::Song>("LiveMusic::Song");

    m_pollTimer->setInterval(5000);
    connect(m_pollTimer, &QTimer::timeout, this, &SongRequestClient::slotPoll);

    m_playerTimer->setInterval(m_livePollInterval);
    connect(m_playerTimer, &QTimer::timeout, this, &SongRequestClient::slotLivePoll);
}

void SongRequestClient::loadSettings()
{
    QSettings settings;
    m_enabled = settings.value(SETTINGS_ENABLED, false).toBool();
    m_baseUrl = settings.value(SETTINGS_URL, DEFAULT_URL).toString().trimmed();
    m_token = settings.value(SETTINGS_TOKEN).toString();

    if (isConfigured() == true)
    {
        m_status = Connecting;
        m_pollTimer->start();
        QTimer::singleShot(0, this, &SongRequestClient::connectNow);
    }
    else
    {
        m_status = Disabled;
        m_pollTimer->stop();
    }
    emit statusChanged();
}

void SongRequestClient::configure(bool enabled, const QString &url, const QString &token)
{
    if (m_statusReply != NULL)
    {
        m_statusReply->abort();
        m_statusReply = NULL;
    }
    if (m_libraryReply != NULL)
    {
        m_libraryReply->abort();
        m_libraryReply = NULL;
    }
    if (m_playerReply != NULL)
    {
        m_playerReply->abort();
        m_playerReply = NULL;
    }
    if (m_analysisReply != NULL)
    {
        m_analysisReply->abort();
        m_analysisReply = NULL;
    }

    m_enabled = enabled;
    m_baseUrl = url.trimmed();
    m_token = token;

    QSettings settings;
    settings.setValue(SETTINGS_ENABLED, m_enabled);
    settings.setValue(SETTINGS_URL, m_baseUrl);
    settings.setValue(SETTINGS_TOKEN, m_token);

    if (isConfigured() == false)
    {
        m_status = Disabled;
        m_lastError.clear();
        m_pollTimer->stop();
        m_playerTimer->stop();
        emit statusChanged();
        return;
    }

    m_status = Connecting;
    m_lastError.clear();
    m_pollTimer->start();
    if (m_liveTracking)
        m_playerTimer->start(m_livePollInterval);
    emit statusChanged();
    QTimer::singleShot(0, this, &SongRequestClient::connectNow);
}

bool SongRequestClient::enabled() const
{
    return m_enabled;
}

bool SongRequestClient::isConfigured() const
{
    return m_enabled && m_baseUrl.isEmpty() == false;
}

QString SongRequestClient::baseUrl() const
{
    return m_baseUrl;
}

QString SongRequestClient::token() const
{
    return m_token;
}

SongRequestClient::Status SongRequestClient::status() const
{
    return m_status;
}

QString SongRequestClient::statusText() const
{
    switch (m_status)
    {
    case Connected: return tr("SongRequest connected");
    case Connecting: return tr("Connecting to SongRequest...");
    case Disconnected: return tr("SongRequest disconnected");
    case Disabled:
    default: return tr("SongRequest not configured");
    }
}

QString SongRequestClient::lastError() const
{
    return m_lastError;
}

void SongRequestClient::connectNow()
{
    requestStatus(true);
}

void SongRequestClient::requestStatus(bool showConnecting)
{
    if (isConfigured() == false)
    {
        if (m_status != Disabled)
        {
            m_status = Disabled;
            emit statusChanged();
        }
        return;
    }
    if (m_statusReply != NULL)
        return;

    if (showConnecting == true && m_status != Connected)
    {
        m_status = Connecting;
        emit statusChanged();
    }

    QString base = m_baseUrl;
    while (base.endsWith('/'))
        base.chop(1);
    QNetworkRequest request(QUrl(base + "/api/v1/status"));
    request.setTransferTimeout(8000);
    if (m_token.isEmpty() == false)
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    m_statusReply = m_network->get(request);
    connect(m_statusReply, &QNetworkReply::finished,
            this, &SongRequestClient::slotStatusFinished);
}

void SongRequestClient::slotStatusFinished()
{
    if (sender() != m_statusReply || m_statusReply == NULL)
        return;

    QNetworkReply *reply = m_statusReply;
    m_statusReply = NULL;

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || httpStatus >= 400)
    {
        m_status = Disconnected;
        m_lastError = reply->error() == QNetworkReply::NoError
                    ? tr("SongRequest returned HTTP %1").arg(httpStatus)
                    : reply->errorString();
    }
    else
    {
        m_status = Connected;
        m_lastError.clear();
    }

    reply->deleteLater();
    emit statusChanged();
}

void SongRequestClient::slotPoll()
{
    if (isConfigured() == true)
        requestStatus(false);
}

void SongRequestClient::fetchLibrary(const QString &query)
{
    if (isConfigured() == false)
    {
        emit requestFailed(tr("SongRequest is not configured."));
        return;
    }
    if (m_libraryReply != NULL)
    {
        m_libraryReply->abort();
        m_libraryReply = NULL;
    }

    QString base = m_baseUrl;
    while (base.endsWith('/'))
        base.chop(1);
    QUrl url(base + "/api/v1/songs");
    QUrlQuery q;
    q.addQueryItem("source", "library");
    q.addQueryItem("limit", "1000");
    if (query.trimmed().isEmpty() == false)
        q.addQueryItem("q", query.trimmed());
    url.setQuery(q);

    QNetworkRequest request(url);
    request.setTransferTimeout(15000);
    if (m_token.isEmpty() == false)
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    m_libraryReply = m_network->get(request);
    connect(m_libraryReply, &QNetworkReply::finished,
            this, &SongRequestClient::slotLibraryFinished);
}

void SongRequestClient::slotLibraryFinished()
{
    if (sender() != m_libraryReply || m_libraryReply == NULL)
        return;

    QNetworkReply *reply = m_libraryReply;
    m_libraryReply = NULL;
    const QByteArray body = reply->readAll();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError || httpStatus >= 400)
    {
        m_lastError = reply->error() == QNetworkReply::NoError
                    ? tr("SongRequest returned HTTP %1").arg(httpStatus)
                    : reply->errorString();
        emit requestFailed(m_lastError);
        reply->deleteLater();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        emit requestFailed(tr("Invalid SongRequest library response: %1").arg(parseError.errorString()));
        reply->deleteLater();
        return;
    }

    QJsonArray songs;
    if (json.isArray())
        songs = json.array();
    else if (json.isObject())
        songs = songsFromResponse(json.object());

    reply->deleteLater();
    emit libraryReady(songs);
}

void SongRequestClient::startLiveTracking()
{
    if (m_liveTracking && m_playerTimer->isActive())
        return;
    m_liveTracking = true;
    m_liveSongKey.clear();
    if (isConfigured() == false)
    {
        LiveMusic::Playback playback;
        emit playerChanged(playback);
        return;
    }
    m_playerTimer->start(m_livePollInterval);
    QTimer::singleShot(0, this, &SongRequestClient::requestPlayer);
}

void SongRequestClient::stopLiveTracking()
{
    m_liveTracking = false;
    m_playerTimer->stop();
    if (m_playerReply != NULL)
    {
        m_playerReply->abort();
        m_playerReply = NULL;
    }
    if (m_analysisReply != NULL)
    {
        m_analysisReply->abort();
        m_analysisReply = NULL;
    }
}

bool SongRequestClient::liveTracking() const
{
    return m_liveTracking;
}

void SongRequestClient::setLivePollInterval(int milliseconds)
{
    m_livePollInterval = qBound(100, milliseconds, 2000);
    m_playerTimer->setInterval(m_livePollInterval);
}

LiveMusic::Playback SongRequestClient::playerState() const
{
    return m_playerState;
}

void SongRequestClient::slotLivePoll()
{
    requestPlayer();
}

void SongRequestClient::requestPlayer()
{
    if (isConfigured() == false || m_liveTracking == false)
        return;
    if (m_playerReply != NULL)
        return;

    QString base = m_baseUrl;
    while (base.endsWith('/'))
        base.chop(1);

    QNetworkRequest request(QUrl(base + "/api/v1/player"));
    request.setTransferTimeout(3000);
    if (m_token.isEmpty() == false)
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    m_playerReply = m_network->get(request);
    connect(m_playerReply, &QNetworkReply::finished,
            this, &SongRequestClient::slotPlayerFinished);
}

void SongRequestClient::slotPlayerFinished()
{
    if (sender() != m_playerReply || m_playerReply == NULL)
        return;

    QNetworkReply *reply = m_playerReply;
    m_playerReply = NULL;
    const QByteArray body = reply->readAll();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError || httpStatus >= 400)
    {
        m_lastError = reply->error() == QNetworkReply::NoError
                    ? tr("SongRequest returned HTTP %1").arg(httpStatus)
                    : reply->errorString();
        reply->deleteLater();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || json.isObject() == false)
    {
        reply->deleteLater();
        return;
    }

    const QJsonObject root = json.object();
    m_playerState = playerFromJson(root);
    emit playerChanged(m_playerState);

    const QJsonObject current = root.value("current").toObject();
    if (current.isEmpty())
        m_liveSongKey.clear();
    const QString key = songKeyFromCurrent(current);
    if (key.isEmpty() == false && key != m_liveSongKey)
    {
        m_liveSongKey = key;
        if (m_analysisCache.contains(key))
            emit songAnalysisReady(key, m_analysisCache.value(key));
        else
            requestSongAnalysis(key, current);
    }

    reply->deleteLater();
}

LiveMusic::Playback SongRequestClient::playerFromJson(const QJsonObject &root)
{
    LiveMusic::Playback playback;
    playback.active = root.value("isPlaybackActive").toBool();
    playback.playing = root.value("isPlaying").toBool();
    playback.position = root.value("currentTime").toDouble();
    playback.duration = root.value("totalTime").toDouble();

    const QJsonObject current = root.value("current").toObject();
    playback.id = songKeyFromCurrent(current);
    playback.title = current.value("title").toString();
    playback.artist = current.value("artist").toString();
    playback.album = current.value("album").toString();
    playback.genre = current.value("genre").toString();

    if (playback.duration <= 0.0)
        playback.duration = current.value("duration").toDouble();

    return playback;
}

QString SongRequestClient::songKeyFromCurrent(const QJsonObject &current)
{
    if (current.isEmpty())
        return QString();

    const QString songId = current.value("songId").toString();
    if (songId.isEmpty() == false)
        return songId;
    const QString youTubeId = current.value("youTubeId").toString();
    if (youTubeId.isEmpty() == false)
        return youTubeId;
    const QString id = current.value("id").toString();
    if (id.isEmpty() == false)
        return id;

    const QString title = current.value("title").toString();
    const QString artist = current.value("artist").toString();
    if (title.isEmpty() == false)
    {
        const QString songPath = current.value("songPath").toString();
        return songPath.isEmpty() ? artist + "|" + title
                                  : artist + "|" + title + "|" + songPath;
    }

    return current.value("songPath").toString();
}

void SongRequestClient::requestSongAnalysis(const QString &songKey,
                                            const QJsonObject &current)
{
    m_analysisSongKey = songKey;
    m_analysisCurrent = current;
    m_analysisStage = 0;

    if (m_analysisReply != NULL)
    {
        m_analysisReply->abort();
        m_analysisReply = NULL;
    }
    issueAnalysisRequest();
}

void SongRequestClient::issueAnalysisRequest()
{
    if (isConfigured() == false || m_analysisSongKey.isEmpty())
        return;

    QString base = m_baseUrl;
    while (base.endsWith('/'))
        base.chop(1);

    QJsonObject args;
    const QString songId = m_analysisCurrent.value("songId").toString();
    if (songId.isEmpty() == false)
        args.insert("songId", songId);
    const QString songPath = m_analysisCurrent.value("songPath").toString();
    if (songPath.isEmpty() == false)
        args.insert("songPath", songPath);
    const QString title = m_analysisCurrent.value("title").toString();
    if (title.isEmpty() == false)
        args.insert("title", title);
    const QString artist = m_analysisCurrent.value("artist").toString();
    if (artist.isEmpty() == false)
        args.insert("artist", artist);

    const QString command = m_analysisStage == 0 ? "getSongAnalysis" : "analyzeSong";
    QNetworkRequest request(QUrl(base + "/api/v1/commands/" + command));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // Analysing a song can take a while; the first stage is normally instant.
    request.setTransferTimeout(m_analysisStage == 0 ? 8000 : 60000);
    if (m_token.isEmpty() == false)
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());

    m_analysisReply = m_network->post(request, QJsonDocument(args).toJson(QJsonDocument::Compact));
    connect(m_analysisReply, &QNetworkReply::finished,
            this, &SongRequestClient::slotAnalysisFinished);
}

void SongRequestClient::slotAnalysisFinished()
{
    if (sender() != m_analysisReply || m_analysisReply == NULL)
        return;

    QNetworkReply *reply = m_analysisReply;
    m_analysisReply = NULL;
    const QByteArray body = reply->readAll();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool transportOk = (reply->error() == QNetworkReply::NoError && httpStatus < 400);
    reply->deleteLater();

    QJsonObject root;
    if (transportOk)
    {
        QJsonParseError parseError;
        const QJsonDocument json = QJsonDocument::fromJson(body, &parseError);
        if (parseError.error == QJsonParseError::NoError && json.isObject())
            root = json.object();
    }

    // Some command dispatchers wrap the payload in "result" or "data".
    if (root.contains("result") && root.value("result").isObject())
        root = root.value("result").toObject();
    else if (root.contains("data") && root.value("data").isObject())
        root = root.value("data").toObject();

    const double bpm = root.value("bpm").toDouble();
    if (bpm > 20.0)
    {
        LiveMusic::Song song = analysisFromJson(m_analysisSongKey, root, m_analysisCurrent);
        if (song.hasAnalysis())
            m_analysisCache.insert(m_analysisSongKey, song);
        emit songAnalysisReady(m_analysisSongKey, song);
        return;
    }

    if (m_analysisStage == 0)
    {
        // Not cached yet (or the cache endpoint failed): ask SongRequest to
        // analyse the song now.
        m_analysisStage = 1;
        issueAnalysisRequest();
        return;
    }

    // Analysis unavailable. Still publish the metadata so the widget can
    // display the song; the engine falls back to its internal 120 BPM clock.
    LiveMusic::Song song = analysisFromJson(m_analysisSongKey, QJsonObject(), m_analysisCurrent);
    emit songAnalysisReady(m_analysisSongKey, song);
}

LiveMusic::Song SongRequestClient::analysisFromJson(const QString &songKey,
                                                    const QJsonObject &root,
                                                    const QJsonObject &current)
{
    LiveMusic::Song song;
    song.id = songKey;
    song.title = current.value("title").toString(root.value("title").toString());
    song.artist = current.value("artist").toString(root.value("artist").toString());
    song.album = current.value("album").toString();
    song.genre = current.value("genre").toString();
    song.duration = root.value("durationSeconds").toDouble(current.value("duration").toDouble());
    song.bpm = root.value("bpm").toDouble();
    song.beatOffset = root.value("beatOffsetSeconds").toDouble();
    song.lowConfidence = root.value("isLowConfidence").toBool(song.bpm < 20.0)
            || root.value("bpmConfidence").toDouble(1.0) < 0.25;
    song.loudness = root.value("integratedLoudnessLufs").toDouble(
                root.value("loudnessLufs").toDouble(-14.0));
    song.dynamicRange = root.value("dynamicRangeDb").toDouble(10.0);

    const QJsonArray drops = root.value("drops").toArray();
    for (int i = 0; i < drops.size(); i++)
    {
        const QJsonObject obj = drops.at(i).toObject();
        LiveMusic::Drop drop;
        drop.time = obj.value("time").toDouble();
        drop.confidence = obj.value("confidence").toDouble();
        drop.type = obj.value("type").toString();
        drop.label = obj.value("label").toString();
        song.drops.append(drop);
    }

    const QJsonArray parts = root.value("parts").toArray();
    for (int i = 0; i < parts.size(); i++)
    {
        const QJsonObject obj = parts.at(i).toObject();
        LiveMusic::Part part;
        part.start = obj.value("startSeconds").toDouble();
        part.end = obj.value("endSeconds").toDouble();
        part.type = obj.value("type").toString();
        part.label = obj.value("label").toString();
        part.energyDb = obj.value("energyDb").toDouble(-60.0);
        song.parts.append(part);
    }

    const QJsonArray timeline = root.value("timeline").toArray();
    song.timeline.reserve(timeline.size());
    for (int i = 0; i < timeline.size(); i++)
    {
        const QJsonObject obj = timeline.at(i).toObject();
        LiveMusic::TimelinePoint point;
        point.time = obj.value("time").toDouble();
        point.rmsDb = obj.value("rmsDb").toDouble(-60.0);
        point.low = obj.value("lowEnergy").toDouble();
        point.mid = obj.value("midEnergy").toDouble();
        point.high = obj.value("highEnergy").toDouble();
        point.novelty = obj.value("novelty").toDouble();
        song.timeline.append(point);
    }

    return song;
}

QJsonArray SongRequestClient::songsFromResponse(const QJsonObject &root)
{
    const QStringList keys = { "songs", "items", "results", "library", "data" };
    for (const QString &key : keys)
    {
        if (root.value(key).isArray())
            return root.value(key).toArray();
        if (root.value(key).isObject())
        {
            const QJsonArray nested = songsFromResponse(root.value(key).toObject());
            if (nested.isEmpty() == false)
                return nested;
        }
    }
    return QJsonArray();
}
