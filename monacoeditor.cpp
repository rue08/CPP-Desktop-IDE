#include "monacoeditor.h"

#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebChannel>
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QUrl>
#include <QUrlQuery>
#include <QSettings>

namespace {
const char *FONT_SIZE_SETTINGS_KEY = "editorFontSize";
const int DEFAULT_FONT_SIZE = 14;
}

MonacoEditor::MonacoEditor(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    view = new QWebEngineView(this);
    layout->addWidget(view);

    bridge = new MonacoBridge(this);
    channel = new QWebChannel(this);
    channel->registerObject(QStringLiteral("bridge"), bridge);
    view->page()->setWebChannel(channel);

    connect(bridge, &MonacoBridge::userEdited, this, &MonacoEditor::onUserEdited);
    connect(bridge, &MonacoBridge::ready, this, &MonacoEditor::onEditorReady);
    connect(bridge, &MonacoBridge::fontSizeEdited, this, &MonacoEditor::onFontSizeEdited);

    // monaco-host/index.html + the vendored vs/ build next to it are copied
    // next to the built binary at build time -- see CMakeLists.txt and
    // third_party/fetch-monaco.sh. Same directory-resolution pattern as the
    // bundled Windows MinGW toolchain in Terminal::runFile().
    QString hostPath = QCoreApplication::applicationDirPath() + "/monaco/index.html";
    QUrl url = QUrl::fromLocalFile(hostPath);

    // Passed as a query param rather than pushed in after the page loads --
    // Monaco's very first paint (monaco.editor.create() in index.html) needs
    // this before the QWebChannel round-trip could otherwise deliver it, so
    // there's no flash of the default size before it snaps to the saved one.
    int savedFontSize = QSettings().value(QLatin1String(FONT_SIZE_SETTINGS_KEY), DEFAULT_FONT_SIZE).toInt();
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("fontSize"), QString::number(savedFontSize));
    url.setQuery(query);

    view->setUrl(url);
}


void MonacoEditor::setPlainText(const QString &text)
{
    cachedText = text;
    savedText = text; // this is now the "on disk" baseline modified-tracking compares against

    // Loading content is not an edit -- matches how callers used to follow
    // QPlainTextEdit::setPlainText() with document()->setModified(false).
    if (modified)
    {
        modified = false;
        emit modifiedChanged(false);
    }

    if (editorReady)
        emit bridge->loadContent(text);
    else
    {
        pendingText = text;
        hasPendingText = true;
    }
}


void MonacoEditor::setModified(bool m)
{
    // Only ever called with false in practice (after a successful save) --
    // that's what moves the "on disk" baseline forward to whatever's in the
    // buffer right now, same as setPlainText() does for a load.
    if (!m)
        savedText = cachedText;

    if (modified == m)
        return;

    modified = m;
    emit modifiedChanged(m);
}


void MonacoEditor::undo()
{
    emit bridge->triggerAction(QStringLiteral("undo"));
}


void MonacoEditor::redo()
{
    emit bridge->triggerAction(QStringLiteral("redo"));
}


void MonacoEditor::toggleCommentSelection()
{
    emit bridge->triggerAction(QStringLiteral("editor.action.commentLine"));
}


void MonacoEditor::cut()
{
    view->page()->triggerAction(QWebEnginePage::Cut);
}


void MonacoEditor::copy()
{
    view->page()->triggerAction(QWebEnginePage::Copy);
}


void MonacoEditor::paste()
{
    view->page()->triggerAction(QWebEnginePage::Paste);
}


void MonacoEditor::onUserEdited(const QString &text)
{
    cachedText = text;

    // Recomputed against the saved baseline rather than just latching to
    // true -- this is what lets undoing back to the exact saved text clear
    // the modified indicator again (e.g. undo() routing through here via
    // Monaco's own edit stack, same as a normal keystroke would).
    bool isDirty = (cachedText != savedText);
    if (modified != isDirty)
    {
        modified = isDirty;
        emit modifiedChanged(isDirty);
    }
}


void MonacoEditor::onEditorReady()
{
    editorReady = true;

    if (hasPendingText)
    {
        emit bridge->loadContent(pendingText);
        hasPendingText = false;
    }
}


void MonacoEditor::onFontSizeEdited(int size)
{
    // Deliberately app-wide rather than per-tab -- every other open tab
    // keeps whatever size it already rendered at until it's reloaded, but
    // any *new* tab (including ones opened in a future session) picks up
    // this value via the QSettings read in the constructor above.
    QSettings().setValue(QLatin1String(FONT_SIZE_SETTINGS_KEY), size);
}
