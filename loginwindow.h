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

    // Clears the in-memory session (this session tracking, plus
    // Authenticator's stored refresh token) on an explicit logout. Unlike
    // onSessionExpired(), this doesn't emit sessionExpired() or reopen the
    // login dialog -- logging out is a deliberate action, not an error state.
    void logOut();

    // Attempts to silently restore a session saved from a previous run --
    // call once, right after construction. No-op if Authenticator has
    // nothing persisted (first launch, or after an explicit logout). If
    // something *was* saved but turns out to be invalid, this surfaces
    // exactly like a live session dying mid-use (onSessionExpired()) --
    // status message plus the login dialog forced open.
    void restoreSession();

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

    // True while a refreshIdToken() call in flight was triggered by
    // restoreSession() specifically, rather than the ordinary reactive
    // refresh Storage triggers mid-session (refreshSessionNow()). The two
    // need different handling once Authenticator::tokenRefreshed() comes
    // back: a mid-session refresh only needs to hand the new idToken to
    // Storage (the session's already established); restoreSession() needs
    // the full "establish a new session" treatment, same as a normal login
    // succeeding. Checked and cleared in onTokenRefreshed().
    bool restoringSession = false;

signals:
    void enableActionUpload(bool flag, const QString &idToken, const QString &uid);
    void idTokenRefreshed(const QString &idToken);
    void sessionExpired();
};

#endif // LOGINWINDOW_H
