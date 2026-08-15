#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>

class LineNumberArea;

// Drop-in replacement for QPlainTextEdit that adds the baseline editor
// behavior every tab in the workspace is expected to have: a line-number
// gutter, current-line highlighting, matching-bracket highlighting,
// auto-closing brackets/quotes, and auto-indent on Enter. Every place that
// used to do `new QPlainTextEdit()` for a workspace tab now does
// `new CodeEditor()` instead -- nothing else about how callers use it changes,
// since it's still just a QPlainTextEdit underneath.
class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();

    // Stock QPlainTextEdit packs lines tightly (single-height). Bumping this
    // is a per-block QTextBlockFormat property, not a widget-level setting,
    // so it has to be (re)applied to whichever blocks just changed -- newly
    // typed lines inherit it from the block they split off from, but a
    // fresh load (setPlainText(), called directly on the QPlainTextEdit*
    // base pointer from mainwindow.cpp, so it can't be intercepted/wrapped)
    // replaces every block from scratch and needs it reapplied. Hooking
    // contentsChange() catches both cases uniformly.
    void applyLineSpacingToChangedBlocks(int position, int charsRemoved, int charsAdded);

private:
    QWidget *lineNumberArea;

    // Re-entrancy guard for applyLineSpacingToChangedBlocks(): setting a
    // block's format is itself a document change, so without this the slot
    // would immediately re-trigger itself.
    bool applyingLineSpacing = false;

    // Returns true if the keystroke was fully handled here (caller should
    // not fall through to the default QPlainTextEdit::keyPressEvent).
    bool handleAutoClosePair(QKeyEvent *event);
    bool handleBackspaceOverPair();
    bool handleAutoIndent();

    void highlightMatchingBrackets(QList<QTextEdit::ExtraSelection> &selections);
};


// The gutter widget itself -- just forwards its paint event back to the
// CodeEditor, which already knows how to lay out line numbers against its
// own scroll position/font metrics.
class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor)
        : QWidget(editor), codeEditor(editor) {}

    QSize sizeHint() const override
    {
        return QSize(codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        codeEditor->lineNumberAreaPaintEvent(event);
    }

private:
    CodeEditor *codeEditor;
};

#endif // CODEEDITOR_H
