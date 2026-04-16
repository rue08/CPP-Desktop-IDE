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

private:
    QNetworkAccessManager *m_networkAccessManager;
    QString m_APIKey;


    void performPOST(const QString &url, const QJsonDocument &payload);
    void parseResponse(const QByteArray &response);

signals:
    void loginSucceeded(const QString &idToken, const QString &uid);
    void loginFailed();
};

#endif // AUTHENTICATOR_H
