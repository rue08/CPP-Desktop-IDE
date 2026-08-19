#include "authenticator.h"
#include "config.h"
#include <QUrl>
#include <QUrlQuery>
#include <QSettings>

namespace {
const char *REFRESH_TOKEN_SETTINGS_KEY = "session/refreshToken";
const char *EMAIL_SETTINGS_KEY = "session/email";
}

Authenticator::Authenticator(QObject *parent)
    : QObject(parent)
    , apiKey(QString())
{
    networkAccessManager = new QNetworkAccessManager(this);

    apiKey = FIREBASE_API_KEY;

    // Whatever session (if any) survived from a previous run -- empty
    // QSettings values just leave these blank, same as a fresh install.
    // See hasPersistedSession()/persistedEmail(), used by
    // LoginWindow::restoreSession().
    QSettings settings;
    refreshToken = settings.value(REFRESH_TOKEN_SETTINGS_KEY).toString();
    sessionEmail = settings.value(EMAIL_SETTINGS_KEY).toString();
}


bool Authenticator::hasPersistedSession() const
{
    return !refreshToken.isEmpty();
}


QString Authenticator::persistedEmail() const
{
    return sessionEmail;
}


void Authenticator::persistSession()
{
    QSettings settings;
    settings.setValue(REFRESH_TOKEN_SETTINGS_KEY, refreshToken);
    settings.setValue(EMAIL_SETTINGS_KEY, sessionEmail);
}


void Authenticator::clearPersistedSession()
{
    QSettings settings;
    settings.remove(REFRESH_TOKEN_SETTINGS_KEY);
    settings.remove(EMAIL_SETTINGS_KEY);
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
    pendingSignUp = true;
    sessionEmail = email;

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
    pendingSignUp = false;
    sessionEmail = email;

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

    // Read and clear up front -- this response belongs to whichever request
    // is currently in flight, and that's over as of this call either way.
    bool wasSignUp = pendingSignUp;
    pendingSignUp = false;

    // A transport failure (offline, timeout, DNS) can leave `response` empty
    // or non-JSON, which used to fall through and emit loginSucceeded with
    // blank credentials. Require a real idToken before treating this as success.
    if (!jsonDocument.isObject() || object.contains("error") || !object.contains("idToken"))
    {
        // Sign-in and sign-up failures are otherwise identical here (both
        // just land on the generic "Incorrect email/password." message) --
        // EMAIL_EXISTS is the one case worth calling out specifically, since
        // that message is actively wrong for it. Firebase shapes this as
        // {"error": {"message": "EMAIL_EXISTS", ...}}.
        QString errorCode = object.value("error").toObject().value("message").toString();

        if (wasSignUp && errorCode == "EMAIL_EXISTS")
            emit signUpFailed("An account with this email already exists.");
        else
            emit loginFailed();
        return;
    }

    QString idToken = object.value("idToken").toString();
    QString uid = object.value("localId").toString();
    QString refreshToken = object.value("refreshToken").toString();

    this -> refreshToken = refreshToken;
    persistSession();

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


void Authenticator::logOut()
{
    refreshToken.clear();
    clearPersistedSession();
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
        // from here except logging in again -- and no point keeping a
        // known-dead token around to retry next launch either.
        clearPersistedSession();
        emit sessionExpired();
        return;
    }

    QString idToken = object.value("id_token").toString();
    QString refreshToken = object.value("refresh_token").toString();

    this -> refreshToken = refreshToken;
    persistSession(); // Firebase rotates the refresh token on each use -- keep the saved copy current

    emit tokenRefreshed(idToken);
}
