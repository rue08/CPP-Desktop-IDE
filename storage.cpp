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

void Storage::setLocalFilePath(const QString &localFilePath)
{
    m_localFilePath = localFilePath;
}

void Storage::networkReplyReadyReadResponse()
{
    QJsonDocument jsonDocument = QJsonDocument::fromJson(m_networkReply -> readAll());
    writingFileMetadata(jsonDocument.object().value("name").toString());
}

void Storage::networkReplyReadyReadFiles()
{
    parseResponse(m_networkReply -> readAll());
}

void Storage::networkReplyDownloadFiles()
{
    QByteArray response = m_networkReply -> readAll();
    emit setDownloadFile(response);
}

void Storage::listFiles()
{
    QString listFilesEndpoint = "https://firestore.googleapis.com/v1/projects/mehul-s-ide/databases/(default)/documents/Users/" + m_uid + "/Files";
    newRequest = QNetworkRequest{QUrl(listFilesEndpoint)};
    newRequest.setRawHeader("Authorization", ("Bearer " + m_idToken).toUtf8());
    newRequest.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));
    m_networkReply = m_networkAccessManager -> get(newRequest);

    connect(m_networkReply, &QNetworkReply::readyRead, this, &Storage::networkReplyReadyReadFiles);
}

void Storage::downloadFile(const QString &cloudFilePath)
{
    QString downloadFileEndpoint = "https://firebasestorage.googleapis.com/v0/b/" + m_firebaseBucket + "/o/" + QString(QUrl::toPercentEncoding(cloudFilePath)) + "?alt=media";
    newRequest = QNetworkRequest{QUrl(downloadFileEndpoint)};
    newRequest.setRawHeader("Authorization", ("Bearer " + m_idToken).toUtf8());
    m_networkReply = m_networkAccessManager -> get(newRequest);

    connect(m_networkReply, &QNetworkReply::readyRead, this, &Storage::networkReplyDownloadFiles);
}

void Storage::parseResponse(const QByteArray &response)
{
    QJsonDocument jsonDocument = QJsonDocument::fromJson(response);

    QJsonArray documents = jsonDocument.object().value("documents").toArray();
    QByteArray doc = jsonDocument.object().value("documents").toJson();
    jsonDocument = QJsonDocument::fromJson(doc);
    QByteArray fields = jsonDocument.object().value("fields").toJson();
    jsonDocument = QJsonDocument::fromJson(fields);
    QString fileName = jsonDocument.object().value("name").toString();

    for (int i = 0; i < documents.size(); i++)
    {
        QJsonObject fileMetaData = documents[i].toObject();
        QJsonObject fields = fileMetaData["fields"].toObject();
        QString fileName = fields["name"].toObject().value("stringValue").toString();
        QString cloudFilePath = fields["cloudPath"].toObject().value("stringValue").toString();
        emit setCloudFiles(fileName, cloudFilePath);
    }
}

void Storage::uploadFile()
{
    QFile file(m_localFilePath);
    file.open(QIODevice::ReadOnly);
    QByteArray fileData = file.readAll();
    file.close();

    QString remotePath = QString("users/%1/%2").arg(m_uid, QFileInfo(m_localFilePath).fileName());

    QString uploadFileEndpoint = "https://firebasestorage.googleapis.com/v0/b/" + m_firebaseBucket + "/o?uploadType=media&name=" + remotePath;

    newRequest = QNetworkRequest{QUrl(uploadFileEndpoint)};
    newRequest.setRawHeader("Authorization", ("Bearer " + m_idToken).toUtf8());
    newRequest.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/octet-stream"));

    m_networkReply = m_networkAccessManager -> post(newRequest, fileData);

    connect(m_networkReply, &QNetworkReply::readyRead, this, &Storage::networkReplyReadyReadResponse);
}


void Storage::writingFileMetadata(const QString &cloudFilePath)
{
    QString fileMetadataEndpoint = "https://firestore.googleapis.com/v1/projects/mehul-s-ide/databases/(default)/documents/Users/" + m_uid + "/Files";

    QJsonObject fields;
    fields["name"] = QJsonObject {
        {"stringValue", QFileInfo(m_localFilePath).fileName()}
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

    m_networkReply = m_networkAccessManager -> post(newRequest, jsonPayload.toJson());
}
