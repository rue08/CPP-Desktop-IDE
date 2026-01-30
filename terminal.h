#ifndef TERMINAL_H
#define TERMINAL_H

#include <QProcess>
#include <QSysInfo>
#include <QFileInfo>
#include <QWidget>
#include <QPlainTextEdit>

class MainWindow; // Forward declaration

class Terminal : public QWidget
{
    Q_OBJECT

public:
    explicit Terminal(MainWindow *mainWindow, QWidget *parent = nullptr);

    // Make this public so MainWindow can call it
    void runFile();

private slots:
    QString operatingSystem();
    void getFileName();

private:
    MainWindow *m_window;
    QProcess *myProcess; // Initialization moved to .cpp
    QString name = "";
    QString path = "";
};

#endif // TERMINAL_H
