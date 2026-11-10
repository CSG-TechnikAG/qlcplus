/*
  Q Light Controller Plus
  songrequestclient.h

  HTTP client for the SongRequest local API.

  Licensed under the Apache License, Version 2.0
*/

#ifndef SONGREQUESTCLIENT_H
#define SONGREQUESTCLIENT_H

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include "livemusicengine.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

/** @addtogroup ui UI
 * @{
 */

class SongRequestClient final : public QObject
{
    Q_OBJECT

public:
    enum Status
    {
        Disabled = 0,   //!< Not configured or explicitly disabled
        Connecting,     //!< A status request is in flight
        Connected,      //!< Last status request succeeded
        Disconnected    //!< Last status request failed
    };
    Q_ENUM(Status)

    static SongRequestClient *instance();

    void loadSettings();
    void configure(bool enabled, const QString &url, const QString &token);
    bool enabled() const;
    bool isConfigured() const;
    QString baseUrl() const;
    QString token() const;

    Status status() const;
    QString statusText() const;
    QString lastError() const;

    /** Validate the current connection immediately. */
    void connectNow();

    /** Fetch the SongRequest library. Results arrive through libraryReady(). */
    void fetchLibrary(const QString &query = QString());

    /** Start/stop fast live playback polling for the Live Music widget. */
    void startLiveTracking();
    void stopLiveTracking();
    bool liveTracking() const;
    void setLivePollInterval(int milliseconds);

    /** Latest player snapshot (also updated while live tracking is active). */
    LiveMusic::Playback playerState() const;

    /** Extract the song array from the different response shapes used by the
     *  SongRequest API. */
    static QJsonArray songsFromResponse(const QJsonObject &root);

signals:
    void statusChanged();
    void libraryReady(const QJsonArray &songs);
    void requestFailed(const QString &message);

    /** Emitted for every fast player poll while live tracking is active. */
    void playerChanged(const LiveMusic::Playback &playback);

    /** Emitted when a song's BPM/drop/part analysis becomes available.
     *  The payload may have no analysis (bpm == 0) if the analysis failed;
     *  the show engine then falls back to a safe internal tempo. */
    void songAnalysisReady(const QString &songKey, const LiveMusic::Song &song);

private slots:
    void slotStatusFinished();
    void slotLibraryFinished();
    void slotPoll();
    void slotLivePoll();
    void slotPlayerFinished();
    void slotAnalysisFinished();

private:
    void requestStatus(bool showConnecting);
    void requestPlayer();
    void requestSongAnalysis(const QString &songKey, const QJsonObject &current);
    void issueAnalysisRequest();
    static QString songKeyFromCurrent(const QJsonObject &current);
    static LiveMusic::Playback playerFromJson(const QJsonObject &root);
    static LiveMusic::Song analysisFromJson(const QString &songKey,
                                            const QJsonObject &root,
                                            const QJsonObject &current);
    explicit SongRequestClient(QObject *parent = nullptr);
    static SongRequestClient *s_instance;

    QNetworkAccessManager *m_network;
    QTimer *m_pollTimer;
    QTimer *m_playerTimer;
    QNetworkReply *m_statusReply;
    QNetworkReply *m_libraryReply;
    QNetworkReply *m_playerReply;
    QNetworkReply *m_analysisReply;

    bool m_enabled;
    QString m_baseUrl;
    QString m_token;
    Status m_status;
    QString m_lastError;

    bool m_liveTracking;
    int m_livePollInterval;
    QString m_liveSongKey;
    QString m_analysisSongKey;
    int m_analysisStage;
    QJsonObject m_analysisCurrent;
    LiveMusic::Playback m_playerState;
    QHash<QString, LiveMusic::Song> m_analysisCache;
};

/** @} */

#endif // SONGREQUESTCLIENT_H
