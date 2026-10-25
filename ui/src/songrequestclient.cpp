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
    , m_statusReply(NULL)
    , m_libraryReply(NULL)
    , m_enabled(false)
    , m_baseUrl(DEFAULT_URL)
    , m_status(Disabled)
{
    m_pollTimer->setInterval(5000);
    connect(m_pollTimer, &QTimer::timeout, this, &SongRequestClient::slotPoll);
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
        emit statusChanged();
        return;
    }

    m_status = Connecting;
    m_lastError.clear();
    m_pollTimer->start();
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
