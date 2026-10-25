/*
  Q Light Controller Plus
  songrequestclient.h

  HTTP client for the SongRequest local API.

  Licensed under the Apache License, Version 2.0
*/

#ifndef SONGREQUESTCLIENT_H
#define SONGREQUESTCLIENT_H

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

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

    /** Extract the song array from the different response shapes used by the
     *  SongRequest API. */
    static QJsonArray songsFromResponse(const QJsonObject &root);

signals:
    void statusChanged();
    void libraryReady(const QJsonArray &songs);
    void requestFailed(const QString &message);

private slots:
    void slotStatusFinished();
    void slotLibraryFinished();
    void slotPoll();

private:
    void requestStatus(bool showConnecting);
    explicit SongRequestClient(QObject *parent = nullptr);
    static SongRequestClient *s_instance;

    QNetworkAccessManager *m_network;
    QTimer *m_pollTimer;
    QNetworkReply *m_statusReply;
    QNetworkReply *m_libraryReply;

    bool m_enabled;
    QString m_baseUrl;
    QString m_token;
    Status m_status;
    QString m_lastError;
};

/** @} */

#endif // SONGREQUESTCLIENT_H
