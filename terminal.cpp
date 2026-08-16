#include "terminal.h"
#include "mainwindow.h"
#include "monacoeditor.h"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QStatusBar>

Terminal::Terminal(MainWindow *mainWindow, QWidget *parent)
    : QWidget(parent),
    mainWindow(mainWindow)
{
    // Initialize properly with 'this' so it is deleted when Terminal is deleted
    myProcess = new QProcess(this);
}

QString Terminal::operatingSystem()
{
    return QSysInfo::productType();
}

bool Terminal::isSafeForShellInterpolation(const QString &value, QString *errorOut)
{
    // Both launch mechanisms below build a command by interpolating this
    // value into a quoted string (AppleScript on macOS, cmd.exe on
    // Windows), rather than passing it as a properly-escaped argument. Any
    // of these characters could let the interpolated value break out of
    // that quoting and inject additional commands -- e.g. a filename
    // synced down from the cloud that was crafted maliciously.
    static const QString dangerousChars = QStringLiteral("\"`$&|^%<>;");
    for (const QChar &c : value)
    {
        if (dangerousChars.contains(c))
        {
            if (errorOut)
                *errorOut = QString("Can't run this file: its path contains an unsupported character ('%1').").arg(c);
            return false;
        }
    }
    return true;
}

void Terminal::runFile()
{
    // 1. Get current file info
    getFileName();
    if (path.isEmpty() || name.isEmpty()) return;

    QString outputName = name;
    if (outputName.endsWith(".cpp"))
        outputName.chop(4);

    qsizetype len = name.size();
    path.chop(len);

    QString shellError;
    if (!isSafeForShellInterpolation(path, &shellError) || !isSafeForShellInterpolation(name, &shellError))
    {
        mainWindow -> statusBar() -> showMessage(shellError, 4000);
        return;
    }

    QString fullCommand;
    QStringList arguments;

#if defined(Q_OS_WIN)
    // --- Windows ---
    // Prefer the MinGW-w64 toolchain bundled next to the app (copied in at
    // build/packaging time -- see windows/README.md) so Run works with no
    // install step on the user's machine. Falls back to PATH so local dev
    // builds still work before the toolchain has been dropped in.
    QString bundledGpp = QCoreApplication::applicationDirPath() + "/mingw64/bin/g++.exe";
    QString compiler = QFileInfo::exists(bundledGpp) ? bundledGpp : QStandardPaths::findExecutable("g++");

    if (compiler.isEmpty())
    {
        mainWindow -> statusBar() -> showMessage(
            "No C++ compiler found. This build doesn't include the bundled MinGW-w64 toolchain, "
            "and none was found on PATH.", 6000);
        return;
    }

    // Launch a new console window running cmd.exe. /k (rather than /c)
    // keeps the window open once the chained command finishes, whether it
    // succeeded or not, so compile errors and program output both stay
    // visible instead of the window vanishing immediately.
    QString nativePath = QDir::toNativeSeparators(path);
    QString winCmd = QString("cd /d \"%1\" && \"%2\" \"%3\" -o \"%4.exe\" && \"%4.exe\"")
                          .arg(nativePath, compiler, name, outputName);

    fullCommand = "cmd.exe";
    arguments << "/k" << winCmd;
#else
    // --- macOS ---
    if (QStandardPaths::findExecutable("g++").isEmpty())
    {
        mainWindow -> statusBar() -> showMessage(
            "g++ was not found on PATH. Install the Xcode Command Line Tools "
            "(run 'xcode-select --install' in Terminal) and try again.", 6000);
        return;
    }

    // We use AppleScript (osascript) to tell the Terminal app to run our command.
    // This forces a new native Terminal window to open.

    // 1. Create the command chain (Compile -> Run -> Read/Pause)
    // We add 'read' at the end so the terminal doesn't close immediately if the program ends fast.
    QString macCmd = QString("cd \\\"%1\\\" && g++ %2 -o %3 && \\\"%1\\\"%3 ; read").arg(path, name, outputName);

    // 2. Wrap it in AppleScript
    fullCommand = "osascript";
    arguments << "-e" << "tell application \"Terminal\" to activate"
              << "-e" << QString("tell application \"Terminal\" to do script \"%1\" in window 1").arg(macCmd);

    // Note for macOS: You might need to grant your IDE "Automation" permission
    // for Terminal the first time you run this.
#endif

    QProcess::startDetached(fullCommand, arguments);
}

void Terminal::getFileName()
{
    mainWindow -> curr = qobject_cast<MonacoEditor*>(mainWindow -> theWorkspace -> currentWidget());
    path = mainWindow -> curr -> property("filePath").toString();
    QFileInfo fi(path);
    name = fi.fileName();
}
