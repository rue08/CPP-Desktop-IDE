#include "authenticator.h"

Authenticator::Authenticator(QObject *parent)
    : QObject(parent)
    , m_APIKey(QString())
{
    m_networkAccessManager = new QNetworkAccessManager(this);
}

void Authenticator::setAPIKey(const QString &key)
{
    m_APIKey = key;
}

void Authenticator::networkReplyReadyRead()
{
    QByteArray response = m_networkReply -> readAll();

    parseResponse(response);
}

void Authenticator::signUserUp(const QString &email, const QString &password)
{
    QString signUpEndpoint = "https://identitytoolkit.googleapis.com/v1/accounts:signUp?key=";
    signUpEndpoint += m_APIKey;
    QVariantMap v_mpp;
    v_mpp["email"] = email;
    v_mpp["password"] = password;
    v_mpp["returnSecureToken"] = true;

    QJsonDocument jsonPayload = QJsonDocument::fromVariant(v_mpp);

    performPOST(signUpEndpoint, jsonPayload);
}

void Authenticator::signUserIn(const QString &email, const QString &password)
{
    QString signInEndpoint = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=";
    signInEndpoint += m_APIKey;
    QVariantMap v_mpp;
    v_mpp["email"] = email;
    v_mpp["password"] = password;
    v_mpp["returnSecureToken"] = true;

    QJsonDocument jsonPayload = QJsonDocument::fromVariant(v_mpp);

    performPOST(signInEndpoint, jsonPayload);
}

void Authenticator::performPOST(const QString &url, const QJsonDocument &payload)
{
    QNetworkRequest newRequest{QUrl(url)};
    newRequest.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));

    m_networkReply = m_networkAccessManager -> post(newRequest, payload.toJson());

    connect(m_networkReply, &QNetworkReply::readyRead, this, &Authenticator::networkReplyReadyRead);
}

void Authenticator::parseResponse(const QByteArray &response)
{
    QJsonDocument jsonDocument = QJsonDocument::fromJson(response);

    if (jsonDocument.object().contains("error"))
        emit loginFailed();
    else
    {
        QString idToken = jsonDocument.object().value("idToken").toString();
        QString uid = jsonDocument.object().value("localId").toString();

        emit loginSucceeded(idToken, uid);
    }
}
