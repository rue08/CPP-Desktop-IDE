#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QDialog>
#include "authenticator.h"

namespace Ui {
class LoginWindow;
}

class MainWindow;

class LoginWindow : public QDialog
{
    Q_OBJECT

public:
    explicit LoginWindow(MainWindow *mainWindow = nullptr, QWidget *parent = nullptr);
    ~LoginWindow();

    // Asks Authenticator to exchange the stored refresh token for a new
    // idToken. Called when Storage reports a request just failed because the
    // token had already gone stale.
    void refreshSessionNow();

private slots:
    void on_loginButton_clicked();
    void onLoginSucceeded(const QString &idToken, const QString &uid);
    void on_signUpButton_clicked();
    void onLoginFailed();
    void onTokenRefreshed(const QString &idToken);
    void onSessionExpired();

private:
    MainWindow *mainWindow;
    Ui::LoginWindow *ui;
    Authenticator *auth;

signals:
    void enableActionUpload(bool flag, const QString &idToken, const QString &uid);
    void idTokenRefreshed(const QString &idToken);
    void sessionExpired();
};

#endif // LOGINWINDOW_H
