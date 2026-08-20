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
    void on_googleSignInButton_clicked();
    void onLoginSucceeded(const QString &idToken, const QString &uid, const QString &email);
    void onLoginFailed();
    void onTokenRefreshed(const QString &idToken);
    void onSessionExpired();

private:
    MainWindow *mainWindow;
    Ui::LoginWindow *ui;
    Authenticator *auth;

    // Tracks the in-memory session -- reset in onSessionExpired(), set on a
    // successful sign-in. Not persisted: closing and reopening the app still
    // requires restoreSession() to succeed (see below); this flag just backs
    // the "already logged in" bookkeeping between those two.
    bool isLoggedIn = false;
    QString loggedInEmail;

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
