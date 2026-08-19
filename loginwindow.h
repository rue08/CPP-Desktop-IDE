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
    void onSignUpFailed(const QString &message);
    void onTokenRefreshed(const QString &idToken);
    void onSessionExpired();

private:
    MainWindow *mainWindow;
    Ui::LoginWindow *ui;
    Authenticator *auth;

    // Tracks the in-memory session so a repeat login with the same account
    // can be short-circuited instead of silently redoing the whole
    // sign-in flow -- see on_loginButton_clicked(). Not persisted: closing
    // and reopening the app still requires a fresh login (that's a separate,
    // not-yet-built piece of work). Reset in onSessionExpired(); will also
    // need resetting wherever a future logout action is added.
    bool isLoggedIn = false;
    QString loggedInEmail;

    // The email a sign-in attempt is currently in flight for -- captured at
    // submission time so onLoginSucceeded() (which only gets idToken/uid
    // back from Authenticator) knows what to record as loggedInEmail.
    QString pendingEmail;

signals:
    void enableActionUpload(bool flag, const QString &idToken, const QString &uid);
    void idTokenRefreshed(const QString &idToken);
    void sessionExpired();
};

#endif // LOGINWINDOW_H
