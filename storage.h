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
    void downloadFile(const QString &cloudFilePath);
    void listFiles();


    void setPendingTasks(const int &pendingTasks);

    void uploadFile(const QByteArray &fileData, const QString &localFilePath);
public slots:
    void networkReplyFile();
    void networkReplyDownloadFile();

private:
    QNetworkAccessManager *m_networkAccessManager;
    QNetworkRequest newRequest;
    QString m_uid;
    QString m_idToken;
    QString m_firebaseBucket = "mehul-s-ide.firebasestorage.app";

    QString cloudFileName = "";

    void parseResponse(const QByteArray &response);

    void writeFirestoreMetadata(const QString &cloudFilePath, const QString &localFilePath);

    int m_pendingTasks;

private slots:
    void networkReplyReadyRead(const QString &localFilePath);

signals:
    void setCloudFiles(const QString &fileName, const QString &cloudFilePath);
    void setDownloadFile(const QByteArray &reponse);
};

#endif // STORAGE_H
