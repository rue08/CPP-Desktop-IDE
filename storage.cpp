#include "storage.h"
#include <QFileInfo>
#include <QUrl>

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


QString Storage::extractError(QNetworkReply *reply, const QByteArray &response)
{
    // Firebase's REST APIs return a JSON body shaped like
    // {"error": {"code": ..., "message": ..., "status": ...}} on failure.
    // Prefer that message when present since it's far more specific than
    // Qt's generic transport-level error.
    QJsonDocument jsonDocument = QJsonDocument::fromJson(response);
    if (jsonDocument.isObject() && jsonDocument.object().contains("error"))
    {
        QString message = jsonDocument.object().value("error").toObject().value("message").toString();
        return message.isEmpty() ? QStringLiteral("The server returned an error.") : message;
    }

    if (reply->error() != QNetworkReply::NoError)
        return reply->errorString();

    return QString();
}


void Storage::uploadFile(const QByteArray& fileData, const QString& localFilePath)
{
    m_pendingUploads++;

    QString remotePath = QString("users/%1/%2").arg(m_uid, QFileInfo(localFilePath).fileName());
    QString encodedPath = QUrl::toPercentEncoding(remotePath);
    QString uploadFileEndpoint = "https://firebasestorage.googleapis.com/v0/b/" + m_firebaseBucket + "/o?uploadType=media&name=" + encodedPath;

    newRequest = QNetworkRequest{QUrl(uploadFileEndpoint)};
    newRequest.setRawHeader("Authorization", ("Bearer " + m_idToken).toUtf8());
    newRequest.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/octet-stream"));

    QNetworkReply* reply = m_networkAccessManager -> post(newRequest, fileData);

    connect(reply, &QNetworkReply::finished, this, [this, localFilePath]() {
        this -> onUploadFinished(localFilePath);
    });
}


void Storage::onUploadFinished(const QString& localFilePath)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray response = reply -> readAll();

    QString error = extractError(reply, response);
    if (!error.isEmpty())
    {
        emit uploadFailed(localFilePath, error);
        reply->deleteLater();
        finishPendingUpload();
        return;
    }

    QJsonDocument jsonDocument = QJsonDocument::fromJson(response);

    // Firebase returns the full path in the "name" field on success
    QString cloudFilePath = jsonDocument.object().value("name").toString();

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

    connect(reply, &QNetworkReply::finished, this, [this, reply, localFilePath, cloudFilePath]() {
        QByteArray response = reply -> readAll();
        QString error = extractError(reply, response);

        if (error.isEmpty())
            emit uploadSucceeded(localFilePath, cloudFilePath);
        else
            emit uploadFailed(localFilePath, error);

        // Always settle the batch counter, whether this upload succeeded or not.
        finishPendingUpload();

        reply->deleteLater();
    });
}


void Storage::finishPendingUpload()
{
    // Refresh the list once every upload in the current batch has finished
    // (successfully or not) -- never before, and never left hanging.
    if (--m_pendingUploads <= 0)
        listFiles();
}


void Storage::listFiles()
{
    emit cloudFilesCleared();

    QString listFilesEndpoint = "https://firestore.googleapis.com/v1/projects/mehul-s-ide/databases/(default)/documents/Users/" + m_uid + "/Files";
    QNetworkRequest request{QUrl(listFilesEndpoint)};
    request.setRawHeader("Authorization", ("Bearer " + m_idToken).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));

    QNetworkReply *reply = m_networkAccessManager -> get(request);
    connect(reply, &QNetworkReply::finished, this, &Storage::onListFilesFinished);
}


void Storage::onListFilesFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray response = reply -> readAll();

    QString error = extractError(reply, response);
    if (!error.isEmpty())
        emit listFilesFailed(error);
    else
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


void Storage::downloadFile(const QString &cloudFilePath, QObject *targetTab)
{
    QString downloadFileEndpoint = "https://firebasestorage.googleapis.com/v0/b/" + m_firebaseBucket + "/o/" + QString(QUrl::toPercentEncoding(cloudFilePath)) + "?alt=media";
    newRequest = QNetworkRequest{QUrl(downloadFileEndpoint)};
    newRequest.setRawHeader("Authorization", ("Bearer " + m_idToken).toUtf8());

    QNetworkReply* reply = m_networkAccessManager -> get(newRequest);

    // Track targetTab via QPointer, not a raw pointer: if the caller's widget
    // is destroyed before this download finishes (tab closed, app torn down
    // mid-download), the pointer safely resolves to null instead of dangling.
    QPointer<QObject> trackedTarget(targetTab);
    connect(reply, &QNetworkReply::finished, this, [this, trackedTarget]() {
        this -> onDownloadFinished(trackedTarget);
    });
}


void Storage::onDownloadFinished(QPointer<QObject> targetTab)
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray response = reply -> readAll();

    QString error = extractError(reply, response);
    if (!error.isEmpty())
        emit downloadFailed(error, targetTab.data());
    else
        emit setDownloadFile(response, targetTab.data());

    reply->deleteLater();
}
