#ifndef AUTHENTICATOR_H
#define AUTHENTICATOR_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>

class Authenticator : public QObject
{
    Q_OBJECT

public:
    explicit Authenticator(QObject *parent = nullptr);


    void setAPIKey(const QString &key);
    void networkReplyReadyRead();
    void signUserUp(const QString &email, const QString &password);
    void signUserIn(const QString &email, const QString &password);

    // Exchanges the stored refresh token for a new idToken. Only ever called
    // reactively -- Storage reports a request failed because the token had
    // already gone stale, and this is how the session recovers.
    void refreshIdToken();

private:
    QNetworkAccessManager *networkAccessManager;
    QString apiKey;

    QString refreshToken;

    // Guards against refreshIdToken() being invoked again while one is
    // already in flight (e.g. several requests failing on the same stale
    // token in quick succession).
    bool refreshInProgress = false;

    void performPOST(const QString &url, const QJsonDocument &payload);
    void parseResponse(const QByteArray &response);
    void onRefreshFinished();

signals:
    void loginSucceeded(const QString &idToken, const QString &uid);
    void loginFailed();

    // A new idToken is ready -- an on-demand refresh, triggered by Storage
    // hitting a stale token, succeeded.
    void tokenRefreshed(const QString &idToken);

    // The refresh token itself was rejected (password changed elsewhere,
    // account disabled/deleted, explicitly revoked, or long unused). There's
    // no recovery from this short of logging in again.
    void sessionExpired();
};

#endif // AUTHENTICATOR_H
