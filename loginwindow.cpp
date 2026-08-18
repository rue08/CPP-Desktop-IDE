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
    emit sessionExpired();
}

LoginWindow::~LoginWindow()
{
    delete ui;
}

void LoginWindow::on_loginButton_clicked()
{
    QString email = ui -> lineEdit_email -> text();
    QString password = ui -> lineEdit_password -> text();

    auth -> signUserIn(email, password);
}

void LoginWindow::onLoginSucceeded(const QString &idToken, const QString &uid)
{
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

