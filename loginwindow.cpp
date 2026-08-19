#include "loginwindow.h"
#include "ui_loginwindow.h"
#include "mainwindow.h"
#include <QPropertyAnimation>
#include <QStatusBar>
#include <QAction>
#include <QList>

LoginWindow::LoginWindow(MainWindow *mainWindow, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginWindow)
    , mainWindow(mainWindow)
{
    ui->setupUi(this);

    auth = new Authenticator(this);

    connect(auth, &Authenticator::loginFailed, this, &LoginWindow::onLoginFailed);
    connect(auth, &Authenticator::signUpFailed, this, &LoginWindow::onSignUpFailed);
    connect(auth, &Authenticator::loginSucceeded, this, &LoginWindow::onLoginSucceeded);
    connect(auth, &Authenticator::tokenRefreshed, this, &LoginWindow::onTokenRefreshed);
    connect(auth, &Authenticator::sessionExpired, this, &LoginWindow::onSessionExpired);
}


void LoginWindow::refreshSessionNow()
{
    auth -> refreshIdToken();
}


void LoginWindow::onTokenRefreshed(const QString &idToken)
{
    emit idTokenRefreshed(idToken);
}


void LoginWindow::onSessionExpired()
{
    // The session actually ended -- a subsequent login, even with the same
    // email, is a real new sign-in, not a no-op repeat.
    isLoggedIn = false;
    loggedInEmail.clear();

    emit sessionExpired();
}


void LoginWindow::logOut()
{
    isLoggedIn = false;
    loggedInEmail.clear();

    auth -> logOut();
}

LoginWindow::~LoginWindow()
{
    delete ui;
}

void LoginWindow::on_loginButton_clicked()
{
    QString email = ui -> lineEdit_email -> text();
    QString password = ui -> lineEdit_password -> text();

    // Same account as the one already signed in this session -- nothing to
    // do, and no need to round-trip Firebase to find that out. A *different*
    // account is deliberately left to fall through below (switches session).
    if (isLoggedIn && email.trimmed().compare(loggedInEmail, Qt::CaseInsensitive) == 0)
    {
        mainWindow -> statusBar() -> showMessage("Already logged in.", 3000);
        close();
        return;
    }

    pendingEmail = email;
    auth -> signUserIn(email, password);
}

void LoginWindow::onLoginSucceeded(const QString &idToken, const QString &uid)
{
    isLoggedIn = true;
    loggedInEmail = pendingEmail;

    emit enableActionUpload(true, idToken, uid);
    close();
}

void LoginWindow::on_signUpButton_clicked()
{
    QString email = ui -> lineEdit_email -> text();
    QString password = ui -> lineEdit_password -> text();

    auth -> signUserUp(email, password);
}

void LoginWindow::onLoginFailed()
{
    mainWindow -> statusBar() -> showMessage("Incorrect email/password.", 2000);
}

void LoginWindow::onSignUpFailed(const QString &message)
{
    mainWindow -> statusBar() -> showMessage(message, 4000);
}

