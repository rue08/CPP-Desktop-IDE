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
    , m_window(mainWindow)
{
    auth = new Authenticator(this);
    auth -> setAPIKey("***REMOVED***");

    connect(auth, &Authenticator::loginFailed, this, &LoginWindow::onLoginFailed);
    connect(auth, &Authenticator::loginSucceeded, this, &LoginWindow::onLoginSucceeded);

    ui->setupUi(this);
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
    m_window -> statusBar() -> showMessage("Successfully logged in.", 2000);
    m_window -> idToken = idToken;
    m_window -> uid = uid;
    m_window -> enableActionUpload(true);
    close();
}

void LoginWindow::on_signUpButton_clicked()
{
    QString email = ui -> lineEdit_email -> text();
    QString password = ui -> lineEdit_password -> text();

    auth -> signUserUp(email, password);

    m_window -> statusBar() -> showMessage("Successfully logged in.", 2000);

    close();
}

void LoginWindow::onLoginFailed()
{
    m_window -> statusBar() -> showMessage("Incorrect email/password.", 2000);
}

