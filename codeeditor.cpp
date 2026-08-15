#include "codeeditor.h"

#include <QPainter>
#include <QTextBlock>
#include <QKeyEvent>

namespace {
bool isOpenBracket(QChar c) { return c == '(' || c == '[' || c == '{'; }
bool isCloseBracket(QChar c) { return c == ')' || c == ']' || c == '}'; }

QChar matchingBracket(QChar c)
{
    switch (c.toLatin1())
    {
        case '(': return QChar(')');
        case ')': return QChar('(');
        case '[': return QChar(']');
        case ']': return QChar('[');
        case '{': return QChar('}');
        case '}': return QChar('{');
        default:  return QChar();
    }
}

// Shared by auto-close (insert the partner), skip-over (step past an
// already-typed closer), and backspace (delete a still-empty pair together).
const QString kOpeners = QStringLiteral("([{\"'");
const QString kClosers = QStringLiteral(")]}\"'");
}


CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    lineNumberArea = new LineNumberArea(this);

    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);
    connect(document(), &QTextDocument::contentsChange, this, &CodeEditor::applyLineSpacingToChangedBlocks);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
    applyLineSpacingToChangedBlocks(0, 0, document()->characterCount());

    // Previously set by hand at every tab-creation call site in
    // mainwindow.cpp -- centralized here now that they all go through this
    // constructor instead.
    QFontMetricsF metrics(font());
    setTabStopDistance(metrics.horizontalAdvance(' ') * 4);
}


int CodeEditor::lineNumberAreaWidth()
{
    int digits = 1;
    for (int lines = qMax(1, blockCount()); lines >= 10; lines /= 10)
        ++digits;

    return 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}


void CodeEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}


void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}


void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}


void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), QColor(24, 26, 32));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            painter.setPen(QColor(110, 116, 130));
            painter.drawText(0, top, lineNumberArea->width() - 8, fontMetrics().height(),
                              Qt::AlignRight, QString::number(blockNumber + 1));
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}


void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> selections;

    if (!isReadOnly())
    {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor(255, 255, 255, 18));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.append(selection);
    }

    highlightMatchingBrackets(selections);

    setExtraSelections(selections);
}


void CodeEditor::applyLineSpacingToChangedBlocks(int position, int charsRemoved, int charsAdded)
{
    Q_UNUSED(charsRemoved);

    if (applyingLineSpacing || charsAdded == 0)
        return;

    applyingLineSpacing = true;

    int lastCharacter = qMax(0, document()->characterCount() - 1);
    QTextBlock block = document()->findBlock(position);
    QTextBlock endBlock = document()->findBlock(qMin(position + charsAdded, lastCharacter));

    for (; block.isValid(); block = block.next())
    {
        QTextCursor cursor(block);
        QTextBlockFormat format = cursor.blockFormat();
        format.setLineHeight(150, QTextBlockFormat::ProportionalHeight);
        cursor.setBlockFormat(format);

        if (block == endBlock)
            break;
    }

    applyingLineSpacing = false;
}


void CodeEditor::highlightMatchingBrackets(QList<QTextEdit::ExtraSelection> &selections)
{
    // Simple depth-counting bracket match -- not aware of strings/comments,
    // so a brace inside a string literal can still "count". That's the same
    // tradeoff most lightweight editors make without a real parser behind
    // them, and matches the syntax highlighter's own regex-only approach.
    QString text = toPlainText();
    int pos = textCursor().position();

    int checkPos = -1;
    if (pos < text.length() && (isOpenBracket(text[pos]) || isCloseBracket(text[pos])))
        checkPos = pos;
    else if (pos > 0 && (isOpenBracket(text[pos - 1]) || isCloseBracket(text[pos - 1])))
        checkPos = pos - 1;

    if (checkPos < 0)
        return;

    QChar bracket = text[checkPos];
    QChar partner = matchingBracket(bracket);
    int direction = isOpenBracket(bracket) ? 1 : -1;

    int depth = 0;
    int matchPos = -1;
    for (int i = checkPos; i >= 0 && i < text.length(); i += direction)
    {
        if (text[i] == bracket)
            ++depth;
        else if (text[i] == partner && --depth == 0)
        {
            matchPos = i;
            break;
        }
    }

    if (matchPos < 0)
        return;

    auto selectionFor = [this](int position) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor(90, 130, 200, 90));
        selection.format.setFontWeight(QFont::Bold);
        QTextCursor c = textCursor();
        c.setPosition(position);
        c.setPosition(position + 1, QTextCursor::KeepAnchor);
        selection.cursor = c;
        return selection;
    };

    selections.append(selectionFor(checkPos));
    selections.append(selectionFor(matchPos));
}


void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && handleAutoIndent())
        return;

    if (handleAutoClosePair(event))
        return;

    QPlainTextEdit::keyPressEvent(event);
}


bool CodeEditor::handleBackspaceOverPair()
{
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
        return false;

    int pos = cursor.position();
    if (pos <= 0)
        return false;

    QChar prevChar = document()->characterAt(pos - 1);
    QChar nextChar = document()->characterAt(pos);

    int idx = kOpeners.indexOf(prevChar);
    if (idx == -1 || nextChar != kClosers.at(idx))
        return false;

    // Only collapse a *still-empty* pair -- if there's real content between
    // them, plain backspace (delete just the one character) is correct.
    cursor.beginEditBlock();
    cursor.deleteChar();
    cursor.deletePreviousChar();
    cursor.endEditBlock();
    setTextCursor(cursor);
    return true;
}


bool CodeEditor::handleAutoClosePair(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Backspace)
        return handleBackspaceOverPair();

    const QString text = event->text();
    if (text.isEmpty())
        return false;

    QChar typed = text.at(0);
    QTextCursor cursor = textCursor();

    int openerIndex = kOpeners.indexOf(typed);

    // Typing an opener around a selection wraps the selection instead of
    // replacing it -- select a word, type '(', get (word) with the
    // selection preserved inside the new pair.
    if (openerIndex != -1 && cursor.hasSelection())
    {
        QChar close = kClosers.at(openerIndex);
        int start = cursor.selectionStart();
        int end = cursor.selectionEnd();

        cursor.beginEditBlock();
        cursor.setPosition(end);
        cursor.insertText(QString(close));
        cursor.setPosition(start);
        cursor.insertText(QString(typed));
        cursor.endEditBlock();

        cursor.setPosition(start + 1);
        cursor.setPosition(end + 1, QTextCursor::KeepAnchor);
        setTextCursor(cursor);
        return true;
    }

    if (openerIndex != -1)
    {
        // Quotes only auto-pair when not immediately touching a letter/digit
        // -- otherwise typing an apostrophe mid-word (it's) would insert a
        // stray closing quote right after it.
        if ((typed == '"' || typed == '\'') && document()->characterAt(cursor.position()).isLetterOrNumber())
            return false;

        QChar close = kClosers.at(openerIndex);
        cursor.beginEditBlock();
        cursor.insertText(QString(typed) + QString(close));
        cursor.setPosition(cursor.position() - 1);
        cursor.endEditBlock();
        setTextCursor(cursor);
        return true;
    }

    // Typing a closer while the very next character is already that same
    // closer just steps over it instead of inserting a duplicate.
    int closerIndex = kClosers.indexOf(typed);
    if (closerIndex != -1 && !cursor.hasSelection() && document()->characterAt(cursor.position()) == typed)
    {
        cursor.movePosition(QTextCursor::Right);
        setTextCursor(cursor);
        return true;
    }

    return false;
}


bool CodeEditor::handleAutoIndent()
{
    QTextCursor cursor = textCursor();
    QString lineText = cursor.block().text();
    QString beforeCursor = lineText.left(cursor.positionInBlock());
    QString afterCursor = lineText.mid(cursor.positionInBlock());

    QString leadingWhitespace;
    for (QChar c : beforeCursor)
    {
        if (c == ' ' || c == '\t')
            leadingWhitespace += c;
        else
            break;
    }

    bool afterOpenBrace = beforeCursor.trimmed().endsWith('{');
    bool beforeCloseBrace = afterCursor.trimmed().startsWith('}');

    cursor.beginEditBlock();

    if (afterOpenBrace && beforeCloseBrace)
    {
        // Cursor sits between { and } -- split into a fresh indented line
        // for the cursor, with the closing brace dropped back to the
        // original indent on the line after it.
        QString innerIndent = leadingWhitespace + "    ";
        cursor.insertText("\n" + innerIndent + "\n" + leadingWhitespace);
        cursor.setPosition(cursor.position() - leadingWhitespace.length() - 1);
    }
    else
    {
        QString newIndent = afterOpenBrace ? leadingWhitespace + "    " : leadingWhitespace;
        cursor.insertText("\n" + newIndent);
    }

    cursor.endEditBlock();
    setTextCursor(cursor);
    return true;
}
