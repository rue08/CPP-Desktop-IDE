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

private slots:
    void on_loginButton_clicked();
    void onLoginSucceeded(const QString &idToken, const QString &uid);
    void on_signUpButton_clicked();
    void onLoginFailed();

private:
    MainWindow *m_window;
    Ui::LoginWindow *ui;
    Authenticator *auth;
};

#endif // LOGINWINDOW_H
