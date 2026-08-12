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
#include <QVector>
#include <functional>

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

    // Called by the owner once a token refresh Storage asked for (via
    // tokenRefreshRequired()) has resolved: on success, sets the new idToken
    // and transparently redoes every request that was waiting on it; on
    // failure, settles them all as failures instead of leaving them hanging.
    void resumeAfterTokenRefresh(const QString &idToken);
    void abandonPendingRetries();

private:
    QNetworkAccessManager *networkAccessManager;
    QNetworkRequest newRequest;
    QString uid;
    QString idToken;
    QString firebaseBucket = "mehul-s-ide.firebasestorage.app";

    QString cloudFileName = "";

    int pendingUploads = 0;

    // Requests deferred behind an in-flight token refresh. Each one knows how
    // to redo its own exact request (retry(true)) or how to settle itself as
    // a failure if the refresh didn't pan out (retry(false)).
    bool refreshInProgress = false;
    QVector<std::function<void(bool refreshed)>> pendingRetries;

    void startUpload(const QByteArray &fileData, const QString &localFilePath);
    void onUploadFinished(const QByteArray &fileData, const QString &localFilePath);
    void onListFilesFinished();
    void onDownloadFinished(const QString &cloudFilePath, QPointer<QObject> targetTab);

    void parseResponse(const QByteArray &response);
    void writeFirestoreMetadata(const QString &cloudFilePath, const QString &localFilePath);

    // Called exactly once per uploadFile() call, on success or failure, once
    // that upload's work (including its metadata write) is fully done.
    void finishPendingUpload();

    // Returns a human-readable error if the reply failed (transport error, or
    // a Firebase-style {"error": {...}} JSON body), otherwise a null QString.
    static QString extractError(QNetworkReply *reply, const QByteArray &response);

    // True specifically when the failure means "the idToken was rejected" --
    // as opposed to a network problem, permissions issue, or anything else
    // that a token refresh wouldn't fix.
    static bool isAuthTokenError(QNetworkReply *reply, const QByteArray &response);

    // If this failure was an expired/invalid token, queues `retry` to run
    // once the session is healed or given up on, coalescing any other
    // concurrently-failing requests into the same single refresh, and
    // returns true (caller should stop, this failure has been handled).
    // Returns false if this wasn't a token problem, so the caller should
    // report it as an ordinary failure instead.
    bool deferIfAuthError(QNetworkReply *reply, const QByteArray &response,
                           std::function<void(bool refreshed)> retry);

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

    // Storage has no access to Authenticator -- this just asks whoever owns
    // both to perform a refresh and report back via resumeAfterTokenRefresh()
    // or abandonPendingRetries().
    void tokenRefreshRequired();
};

#endif // STORAGE_H
