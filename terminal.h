#ifndef TERMINAL_H
#define TERMINAL_H

#include <QProcess>
#include <QSysInfo>
#include <QFileInfo>
#include <QWidget>

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
    MainWindow *mainWindow;
    QProcess *myProcess; // Initialization moved to .cpp
    QString name = "";
    QString path = "";

    // Rejects path/filename components that contain characters which could
    // break out of the quoted context of a shell command (AppleScript on
    // macOS, cmd.exe on Windows) if interpolated directly. Returns false --
    // and leaves an explanatory message in errorOut -- if `value` isn't
    // safe to interpolate.
    static bool isSafeForShellInterpolation(const QString &value, QString *errorOut);
};

#endif // TERMINAL_H
