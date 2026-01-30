#include "terminal.h"
#include "mainwindow.h"

Terminal::Terminal(MainWindow *mainWindow, QWidget *parent)
    : QWidget(parent),
    m_window(mainWindow)
{
    // Initialize properly with 'this' so it is deleted when Terminal is deleted
    myProcess = new QProcess(this);
}

QString Terminal::operatingSystem()
{
    return QSysInfo::productType();
}

void Terminal::runFile()
{
    // 1. Get current file info
    getFileName();
    if (path.isEmpty() || name.isEmpty()) return;

    QString outputName = name;
    if (outputName.endsWith(".cpp"))
        outputName.chop(4);

    QString fullCommand;
    QStringList arguments;

    qsizetype len = name.size();
    path.chop(len);

    // --- macOS ---
    // We use AppleScript (osascript) to tell the Terminal app to run our command.
    // This forces a new native Terminal window to open.

    // 1. Create the command chain (Compile -> Run -> Read/Pause)
    // Note: We add 'read' at the end so the terminal doesn't close immediately if the program ends fast.
    QString macCmd = QString("cd \\\"%1\\\" && g++ %2 -o %3 && \\\"%1\\\"%3").arg(path, name, outputName);

    // 2. Wrap it in AppleScript
    fullCommand = "osascript";
    arguments << "-e" << "tell application \"Terminal\" to activate"
              << "-e" << QString("tell application \"Terminal\" to do script \"%1\" in window 1").arg(macCmd);

    // Note for macOS: You might need to grant your IDE "Automation" permission
    // for Terminal the first time you run this.

    QProcess::startDetached(fullCommand, arguments);
}

void Terminal::getFileName()
{
    m_window -> curr = qobject_cast<QPlainTextEdit*>(m_window -> theWorkspace -> currentWidget());
    path = m_window -> curr -> property("filePath").toString();
    QFileInfo fi(path);
    name = fi.fileName();
}
