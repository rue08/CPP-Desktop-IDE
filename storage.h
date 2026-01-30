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

class MainWindow;

class Storage : public QObject
{
    Q_OBJECT
public:
    explicit Storage(QObject *parent = nullptr);


    void setUid(const QString &uid);
    void setIdToken(const QString &idToken);
    void setLocalFilePath(const QString &localFilePath);
    void uploadFile();
    void downloadFile(const QString &cloudFilePath);
    void listFiles();

public slots:
    void networkReplyReadyReadResponse();
    void networkReplyReadyReadFiles();
    void networkReplyDownloadFiles();

private:
    QNetworkAccessManager *m_networkAccessManager;
    QNetworkReply *m_networkReply;
    QNetworkRequest newRequest;
    QString m_localFilePath;
    QString m_uid;
    QString m_idToken;
    QString m_firebaseBucket = "mehul-s-ide.firebasestorage.app";


    void parseResponse(const QByteArray &response);
    void writingFileMetadata(const QString &cloudFilePath);

signals:
    void setCloudFiles(const QString &fileName, const QString &cloudFilePath);
    void setDownloadFile(const QByteArray &reponse);
};

#endif // STORAGE_H
