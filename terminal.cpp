#include "terminal.h"
#include "mainwindow.h"
#include "monacoeditor.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
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

void Terminal::runFile(const QString &filePath)
{
    // 1. Get current file info -- filePath comes fully resolved from
    // MainWindow::resolveRunnablePath(), which is what actually knows
    // whether the current tab is a local file (real filePath property) or a
    // cloud one (no real local path -- resolved to a local export copy
    // instead). See its comment for why this can't just be read back off
    // the tab's own property the way it used to be.
    path = filePath;
    QFileInfo fi(path);
    name = fi.fileName();
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
    //
    // `path` (shared with the macOS branch above) always ends in a trailing
    // separator -- see the chop() near the top of this function. On macOS
    // that's a harmless trailing "/", but toNativeSeparators() turns it into
    // "\", and a path ending "...\" immediately followed by the closing '"'
    // below is a classic Windows command-line trap: CreateProcess's argument
    // escaping (which QProcess::startDetached goes through to launch
    // cmd.exe) reads a trailing "\"" as an *escaped literal quote*, not
    // "close the quoted string" -- so the quote never actually closes, and
    // everything after it (the rest of the chained command) gets swallowed
    // into what `cd` sees as one garbage path argument. Trimmed here before
    // quoting so the closing quote is never preceded by a backslash.
    QString trimmedPath = path;
    while (trimmedPath.endsWith('/') || trimmedPath.endsWith('\\'))
        trimmedPath.chop(1);

    QString nativePath = QDir::toNativeSeparators(trimmedPath);
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
    // We add a pause at the end so the terminal doesn't close immediately if the program ends
    // fast, whether the compile/run actually succeeded or not. A bare 'read' blocks silently with
    // no indication of what's happening or that the user needs to do anything -- echoing a message
    // first makes it explicit that pressing Enter is what releases the terminal back to a normal
    // prompt (and is required before the next Run will work, since Run reuses this same window).
    //
    // The echo+read pair lives in a small trailer script rather than being typed inline, because
    // do script echoes back whatever text it's given as the literal "command line" before showing
    // its output -- typing "... ; echo '...' ; read" in directly would show that plumbing as part
    // of the command itself, right above its own output repeating it. Keeping the compile/run
    // chain itself inline (rather than wrapping the whole thing in a script) means the visible
    // command line still reads as the real thing you'd type by hand, not an opaque script
    // invocation -- only the trailing pause step is routed through the script.
    //
    // Not done via read's own -p flag: that flag isn't portable across shells -- bash treats -p as
    // "show this prompt", but zsh (the macOS default since Catalina) treats it as "read from a
    // coprocess" instead, which fails with "no coprocess".
    QString trailerContents = QStringLiteral(
        "echo \"Press Enter to exit this command execution...\"\n"
        "read\n"
    );

    QString trailerPath = QDir::tempPath() + QString("/ide_run_trailer_%1.sh").arg(QCoreApplication::applicationPid());
    QFile trailerFile(trailerPath);
    if (!trailerFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        mainWindow -> statusBar() -> showMessage("Couldn't write the run script to " + trailerPath, 4000);
        return;
    }
    trailerFile.write(trailerContents.toUtf8());
    trailerFile.close();
    trailerFile.setPermissions(trailerFile.permissions() | QFile::ExeOwner);

    QString macCmd = QString("cd \\\"%1\\\" && g++ %2 -o %3 && \\\"%1\\\"%3 ; sh \\\"%4\\\"").arg(path, name, outputName, trailerPath);

    // 2. Wrap it in AppleScript
    fullCommand = "osascript";
    arguments << "-e" << "tell application \"Terminal\" to activate"
              << "-e" << QString("tell application \"Terminal\" to do script \"%1\" in window 1").arg(macCmd);

    // Note for macOS: You might need to grant your IDE "Automation" permission
    // for Terminal the first time you run this.
#endif

    QProcess::startDetached(fullCommand, arguments);
}

