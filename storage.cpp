#include "storage.h"
#include <QFileInfo>

Storage::Storage(QObject *parent)
    : QObject{parent}
{
    m_networkAccessManager = new QNetworkAccessManager(this);
}

void Storage::setUid(const QString &uid)
{
    m_uid = uid;
}

void Storage::setIdToken(const QString &idToken)
{
    m_idToken = idToken;
}


void Storage::setPendingTasks(const int& pendingTasks)
{
    m_pendingTasks = pendingTasks;
}


void Storage::uploadFile(const QByteArray& fileData, const QString& localFilePath)
{
    QString remotePath = QString("users/%1/%2").arg(m_uid, QFileInfo(localFilePath).fileName());
    QString encodedPath = QUrl::toPercentEncoding(remotePath);
    QString uploadFileEndpoint = "https://firebasestorage.googleapis.com/v0/b/" + m_firebaseBucket + "/o?uploadType=media&name=" + encodedPath;

    newRequest = QNetworkRequest{QUrl(uploadFileEndpoint)};
    newRequest.setRawHeader("Authorization", ("Bearer " + m_idToken).toUtf8());
    newRequest.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/octet-stream"));

    QNetworkReply* reply = m_networkAccessManager -> post(newRequest, fileData);

    connect(reply, &QNetworkReply::finished, this, [this, localFilePath]() {
        // This code runs ONLY when the reply is finished
        this -> networkReplyReadyRead(localFilePath);
    });
}


void Storage::networkReplyReadyRead(const QString& localFilePath)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

    QByteArray response = reply -> readAll();
    QJsonDocument jsonDocument = QJsonDocument::fromJson(response);
    QJsonObject jsonObject = jsonDocument.object();

    // Firebase returns the full path in the "name" field on success
    QString cloudFilePath = jsonObject.value("name").toString();

    writeFirestoreMetadata(cloudFilePath, localFilePath);

    reply->deleteLater();
}


void Storage::writeFirestoreMetadata(const QString &cloudFilePath, const QString& localFilePath)
{
    QString fileMetadataEndpoint = "https://firestore.googleapis.com/v1/projects/mehul-s-ide/databases/(default)/documents/Users/" + m_uid + "/Files/" + QFileInfo(localFilePath).fileName();

    QJsonObject fields;
    fields["name"] = QJsonObject {
        {"stringValue", QFileInfo(localFilePath).fileName()}
    };
    fields["cloudPath"] = QJsonObject {
        {"stringValue", cloudFilePath}
    };

    QJsonObject root;
    root["fields"] =  fields;

    QJsonDocument jsonPayload(root);

    newRequest = QNetworkRequest{QUrl(fileMetadataEndpoint)};
    newRequest.setRawHeader("Authorization", ("Bearer " + m_idToken).toUtf8());
    newRequest.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));

    QNetworkReply* reply = m_networkAccessManager -> sendCustomRequest(newRequest, "PATCH", jsonPayload.toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_pendingTasks--;

        // ONLY list files when the last one is done
        if (m_pendingTasks <= 0)
            this -> listFiles();

        reply->deleteLater();
    });
}


void Storage::listFiles()
{
    QString listFilesEndpoint = "https://firestore.googleapis.com/v1/projects/mehul-s-ide/databases/(default)/documents/Users/" + m_uid + "/Files";
    QNetworkRequest request{QUrl(listFilesEndpoint)};
    request.setRawHeader("Authorization", ("Bearer " + m_idToken).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));

    QNetworkReply *reply = m_networkAccessManager -> get(request);
    connect(reply, &QNetworkReply::finished, this, &Storage::networkReplyFile);
}


void Storage::networkReplyFile()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

    QByteArray response = reply -> readAll();
    parseResponse(response);

    reply->deleteLater();
}


void Storage::parseResponse(const QByteArray &response)
{
    QJsonDocument jsonDocument = QJsonDocument::fromJson(response);

    QJsonArray documents = jsonDocument.object().value("documents").toArray();

    for (int i = 0; i < documents.size(); i++)
    {
        QJsonObject fileMetaData = documents[i].toObject();
        QJsonObject fields = fileMetaData.value("fields").toObject();

        QString fileName = fields.value("name").toObject().value("stringValue").toString();
        QString cloudFilePath = fields.value("cloudPath").toObject().value("stringValue").toString();

        emit setCloudFiles(fileName, cloudFilePath);
    }
}


void Storage::downloadFile(const QString &cloudFilePath)
{
    QString downloadFileEndpoint = "https://firebasestorage.googleapis.com/v0/b/" + m_firebaseBucket + "/o/" + QString(QUrl::toPercentEncoding(cloudFilePath)) + "?alt=media";
    newRequest = QNetworkRequest{QUrl(downloadFileEndpoint)};
    newRequest.setRawHeader("Authorization", ("Bearer " + m_idToken).toUtf8());

    QNetworkReply* reply = m_networkAccessManager -> get(newRequest);

    connect(reply, &QNetworkReply::readyRead, this, &Storage::networkReplyDownloadFile);
}


void Storage::networkReplyDownloadFile()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray response = reply -> readAll();

    emit setDownloadFile(response);
}
