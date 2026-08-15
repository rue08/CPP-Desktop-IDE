#include "storage.h"
#include <QFileInfo>
#include <QUrl>

Storage::Storage(QObject *parent)
    : QObject{parent}
{
    networkAccessManager = new QNetworkAccessManager(this);
}

void Storage::setIdToken(const QString &idToken)
{
    this -> idToken = idToken;
}

void Storage::setBackendUrl(const QString &backendUrl)
{
    // Trim any trailing slash so callers below can unconditionally do
    // backendUrl + "/path" without ever risking a doubled slash.
    QString trimmed = backendUrl;
    while (trimmed.endsWith('/'))
        trimmed.chop(1);

    this -> backendUrl = trimmed;
}


void Storage::loginToBackend()
{
    QJsonObject payload;
    payload["id_token"] = idToken;

    newRequest = QNetworkRequest{QUrl(backendUrl + "/auth/firebase/login")};
    newRequest.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));

    QNetworkReply* reply = networkAccessManager -> post(newRequest, QJsonDocument(payload).toJson());

    connect(reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
        QByteArray response = reply -> readAll();

        // Deliberately not routed through deferIfAuthError: this call *is*
        // how the session gets established in the first place, so a 401
        // here means the idToken itself is bad -- retrying via the normal
        // refresh dance would just come back to this same call.
        QString error = extractError(reply, response);

        if (error.isEmpty())
            emit backendLoginSucceeded();
        else
            emit backendLoginFailed(error);

        reply->deleteLater();
    });
}


QString Storage::extractError(QNetworkReply *reply, const QByteArray &response)
{
    // The backend's error responses are shaped like {"error": "message"} --
    // a flat string, unlike Firebase's nested {"error": {"message": ...}}.
    QJsonDocument jsonDocument = QJsonDocument::fromJson(response);
    if (jsonDocument.isObject() && jsonDocument.object().contains("error"))
    {
        QString message = jsonDocument.object().value("error").toString();
        return message.isEmpty() ? QStringLiteral("The server returned an error.") : message;
    }

    if (reply->error() != QNetworkReply::NoError)
        return reply->errorString();

    return QString();
}


bool Storage::isAuthTokenError(QNetworkReply *reply, const QByteArray &response)
{
    Q_UNUSED(response);

    // The backend returns 401 specifically for a missing/invalid/expired
    // token (see middleware/auth.js) -- as opposed to 403, which means the
    // token is fine but there's no users row for it yet (loginToBackend()
    // was never called), a refresh wouldn't fix that.
    return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401;
}


bool Storage::deferIfAuthError(QNetworkReply *reply, const QByteArray &response,
                                std::function<void(bool)> retry)
{
    if (!isAuthTokenError(reply, response))
        return false;

    pendingRetries.append(std::move(retry));

    // Several requests can fail on the same stale token at once (a
    // multi-file upload batch, for instance) -- only ask for one refresh and
    // let everything else queue behind it.
    if (!refreshInProgress)
    {
        refreshInProgress = true;
        emit tokenRefreshRequired();
    }

    return true;
}


void Storage::resumeAfterTokenRefresh(const QString &idToken)
{
    this -> idToken = idToken;
    refreshInProgress = false;

    const auto retries = pendingRetries;
    pendingRetries.clear();

    for (const auto &retry : retries)
        retry(true);
}


void Storage::abandonPendingRetries()
{
    refreshInProgress = false;

    const auto retries = pendingRetries;
    pendingRetries.clear();

    for (const auto &retry : retries)
        retry(false);
}


void Storage::uploadFile(const QByteArray& fileData, const QString& localFilePath)
{
    pendingUploads++;
    startUpload(fileData, localFilePath);
}


void Storage::startUpload(const QByteArray& fileData, const QString& localFilePath)
{
    QString filename = QFileInfo(localFilePath).fileName();

    QJsonObject payload;
    payload["filename"] = filename;
    payload["content"] = QString::fromUtf8(fileData);

    newRequest = QNetworkRequest{QUrl(backendUrl + "/files")};
    newRequest.setRawHeader("Authorization", ("Bearer " + idToken).toUtf8());
    newRequest.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));

    QNetworkReply* reply = networkAccessManager -> post(newRequest, QJsonDocument(payload).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, fileData, localFilePath]() {
        this -> onUploadFinished(fileData, localFilePath);
    });
}


void Storage::onUploadFinished(const QByteArray &fileData, const QString& localFilePath)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray response = reply -> readAll();

    if (deferIfAuthError(reply, response, [this, fileData, localFilePath](bool refreshed) {
            if (refreshed)
                this -> startUpload(fileData, localFilePath);
            else
            {
                emit uploadFailed(localFilePath, "Session expired. Please log in again.");
                finishPendingUpload();
            }
        }))
    {
        reply->deleteLater();
        return;
    }

    QString error = extractError(reply, response);
    if (!error.isEmpty())
    {
        emit uploadFailed(localFilePath, error);
        reply->deleteLater();
        finishPendingUpload();
        return;
    }

    // The backend hands back the row it just created/updated, keyed by its
    // numeric id -- that id is what setCloudFiles()/downloadFile() use from
    // here on to refer to this file.
    QJsonDocument jsonDocument = QJsonDocument::fromJson(response);
    QString fileId = QString::number(jsonDocument.object().value("id").toInt());

    emit uploadSucceeded(localFilePath, fileId);
    finishPendingUpload();

    reply->deleteLater();
}


void Storage::finishPendingUpload()
{
    // Refresh the list once every upload in the current batch has finished
    // (successfully or not) -- never before, and never left hanging.
    if (--pendingUploads <= 0)
        listFiles();
}


void Storage::listFiles()
{
    emit cloudFilesCleared();

    QNetworkRequest request{QUrl(backendUrl + "/files")};
    request.setRawHeader("Authorization", ("Bearer " + idToken).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));

    QNetworkReply *reply = networkAccessManager -> get(request);
    connect(reply, &QNetworkReply::finished, this, &Storage::onListFilesFinished);
}


void Storage::onListFilesFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray response = reply -> readAll();

    if (deferIfAuthError(reply, response, [this](bool refreshed) {
            if (refreshed)
                this -> listFiles();
            else
                emit listFilesFailed("Session expired. Please log in again.");
        }))
    {
        reply->deleteLater();
        return;
    }

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
    QJsonArray files = jsonDocument.array();

    for (int i = 0; i < files.size(); i++)
    {
        QJsonObject file = files[i].toObject();

        QString fileName = file.value("filename").toString();
        QString fileId = QString::number(file.value("id").toInt());

        emit setCloudFiles(fileName, fileId);
    }
}


void Storage::downloadFile(const QString &fileId, QObject *targetTab)
{
    newRequest = QNetworkRequest{QUrl(backendUrl + "/files/" + fileId)};
    newRequest.setRawHeader("Authorization", ("Bearer " + idToken).toUtf8());

    QNetworkReply* reply = networkAccessManager -> get(newRequest);

    // Track targetTab via QPointer, not a raw pointer: if the caller's widget
    // is destroyed before this download finishes (tab closed, app torn down
    // mid-download), the pointer safely resolves to null instead of dangling.
    QPointer<QObject> trackedTarget(targetTab);
    connect(reply, &QNetworkReply::finished, this, [this, fileId, trackedTarget]() {
        this -> onDownloadFinished(fileId, trackedTarget);
    });
}


void Storage::onDownloadFinished(const QString &fileId, QPointer<QObject> targetTab)
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray response = reply -> readAll();

    if (deferIfAuthError(reply, response, [this, fileId, targetTab](bool refreshed) {
            // The tab may have been closed while we were waiting on the
            // refresh -- targetTab safely reads as null in that case, so
            // there's nothing left to retry or report into.
            if (targetTab.isNull())
                return;

            if (refreshed)
                this -> downloadFile(fileId, targetTab.data());
            else
                emit downloadFailed("Session expired. Please log in again.", targetTab.data());
        }))
    {
        reply->deleteLater();
        return;
    }

    QString error = extractError(reply, response);
    if (!error.isEmpty())
    {
        emit downloadFailed(error, targetTab.data());
        reply->deleteLater();
        return;
    }

    // The backend returns the file row as JSON ({id, filename, content,
    // updated_at}) -- unlike the old Firebase Storage GET, the response body
    // itself isn't the raw file content.
    QJsonDocument jsonDocument = QJsonDocument::fromJson(response);
    QByteArray content = jsonDocument.object().value("content").toString().toUtf8();

    emit setDownloadFile(content, targetTab.data());

    reply->deleteLater();
}
