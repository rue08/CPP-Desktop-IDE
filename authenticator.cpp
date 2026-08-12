#include "authenticator.h"
#include "config.h"
#include <QUrl>
#include <QUrlQuery>

Authenticator::Authenticator(QObject *parent)
    : QObject(parent)
    , apiKey(QString())
{
    networkAccessManager = new QNetworkAccessManager(this);

    apiKey = FIREBASE_API_KEY;
}

void Authenticator::networkReplyReadyRead()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

    QByteArray response = reply -> readAll();
    parseResponse(response);

    reply -> deleteLater();
}

void Authenticator::signUserUp(const QString &email, const QString &password)
{
    QString signUpEndpoint = "https://identitytoolkit.googleapis.com/v1/accounts:signUp?key=";
    signUpEndpoint += apiKey;
    QVariantMap payload;
    payload["email"] = email;
    payload["password"] = password;
    payload["returnSecureToken"] = true;

    QJsonDocument jsonPayload = QJsonDocument::fromVariant(payload);

    performPOST(signUpEndpoint, jsonPayload);
}

void Authenticator::signUserIn(const QString &email, const QString &password)
{
    QString signInEndpoint = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=";
    signInEndpoint += apiKey;
    QVariantMap payload;
    payload["email"] = email;
    payload["password"] = password;
    payload["returnSecureToken"] = true;

    QJsonDocument jsonPayload = QJsonDocument::fromVariant(payload);

    performPOST(signInEndpoint, jsonPayload);
}

void Authenticator::performPOST(const QString &url, const QJsonDocument &payload)
{
    QNetworkRequest newRequest{QUrl(url)};
    newRequest.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));

    QNetworkReply* reply = networkAccessManager -> post(newRequest, payload.toJson());

    connect(reply, &QNetworkReply::finished, this, &Authenticator::networkReplyReadyRead);
}

void Authenticator::parseResponse(const QByteArray &response)
{
    QJsonDocument jsonDocument = QJsonDocument::fromJson(response);
    QJsonObject object = jsonDocument.object();

    // A transport failure (offline, timeout, DNS) can leave `response` empty
    // or non-JSON, which used to fall through and emit loginSucceeded with
    // blank credentials. Require a real idToken before treating this as success.
    if (!jsonDocument.isObject() || object.contains("error") || !object.contains("idToken"))
    {
        emit loginFailed();
        return;
    }

    QString idToken = object.value("idToken").toString();
    QString uid = object.value("localId").toString();
    QString refreshToken = object.value("refreshToken").toString();

    this -> refreshToken = refreshToken;

    emit loginSucceeded(idToken, uid);
}


void Authenticator::refreshIdToken()
{
    if (refreshInProgress)
        return;
    refreshInProgress = true;

    QString refreshEndpoint = "https://securetoken.googleapis.com/v1/token?key=" + apiKey;

    // Unlike every other Firebase Auth call here, this endpoint takes a
    // form-encoded body, not JSON.
    QUrlQuery body;
    body.addQueryItem("grant_type", "refresh_token");
    body.addQueryItem("refresh_token", refreshToken);

    QNetworkRequest request{QUrl(refreshEndpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/x-www-form-urlencoded"));

    QNetworkReply *reply = networkAccessManager -> post(request, body.query(QUrl::FullyEncoded).toUtf8());

    connect(reply, &QNetworkReply::finished, this, &Authenticator::onRefreshFinished);
}


void Authenticator::onRefreshFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray response = reply -> readAll();
    reply -> deleteLater();

    refreshInProgress = false;

    QJsonDocument jsonDocument = QJsonDocument::fromJson(response);
    QJsonObject object = jsonDocument.object();

    // This endpoint's response uses snake_case field names, unlike the
    // sign-in/sign-up endpoints' camelCase -- same values, different keys.
    if (!jsonDocument.isObject() || object.contains("error") || !object.contains("id_token"))
    {
        // The refresh token itself was rejected: password changed elsewhere,
        // account disabled/deleted, revoked, or long unused. No way back
        // from here except logging in again.
        emit sessionExpired();
        return;
    }

    QString idToken = object.value("id_token").toString();
    QString refreshToken = object.value("refresh_token").toString();

    this -> refreshToken = refreshToken;

    emit tokenRefreshed(idToken);
}
