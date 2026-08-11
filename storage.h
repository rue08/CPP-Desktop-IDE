#ifndef STORAGE_H
#define STORAGE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QPointer>

class MainWindow;

class Storage : public QObject
{
    Q_OBJECT
public:
    explicit Storage(QObject *parent = nullptr);

    void setUid(const QString &uid);
    void setIdToken(const QString &idToken);

    // `targetTab` is opaque to Storage -- it's just echoed back on
    // setDownloadFile()/downloadFailed() so the caller can tell which of its
    // own widgets this particular download was for, even if several are in
    // flight at once or the UI has moved on by the time the reply arrives.
    // If targetTab is destroyed before the download finishes, it comes back
    // as nullptr instead of a dangling pointer.
    void downloadFile(const QString &cloudFilePath, QObject *targetTab);
    void listFiles();

    // Queues a single file for upload. Safe to call repeatedly in a loop for
    // a multi-file batch: Storage tracks how many uploads are outstanding
    // internally and refreshes the cloud file list once the whole batch settles.
    void uploadFile(const QByteArray &fileData, const QString &localFilePath);

private:
    QNetworkAccessManager *m_networkAccessManager;
    QNetworkRequest newRequest;
    QString m_uid;
    QString m_idToken;
    QString m_firebaseBucket = "mehul-s-ide.firebasestorage.app";

    QString cloudFileName = "";

    int m_pendingUploads = 0;

    void onUploadFinished(const QString &localFilePath);
    void onListFilesFinished();
    void onDownloadFinished(QPointer<QObject> targetTab);

    void parseResponse(const QByteArray &response);
    void writeFirestoreMetadata(const QString &cloudFilePath, const QString &localFilePath);

    // Called exactly once per uploadFile() call, on success or failure, once
    // that upload's work (including its metadata write) is fully done.
    void finishPendingUpload();

    // Returns a human-readable error if the reply failed (transport error, or
    // a Firebase-style {"error": {...}} JSON body), otherwise a null QString.
    static QString extractError(QNetworkReply *reply, const QByteArray &response);

signals:
    // Emitted right before a fresh file list is fetched, so the UI can clear
    // its view without every caller having to remember to do it themselves.
    void cloudFilesCleared();
    void setCloudFiles(const QString &fileName, const QString &cloudFilePath);
    void setDownloadFile(const QByteArray &response, QObject *targetTab);

    void uploadSucceeded(const QString &localFilePath, const QString &cloudFilePath);
    void uploadFailed(const QString &localFilePath, const QString &errorString);
    void listFilesFailed(const QString &errorString);
    void downloadFailed(const QString &errorString, QObject *targetTab);
};

#endif // STORAGE_H
