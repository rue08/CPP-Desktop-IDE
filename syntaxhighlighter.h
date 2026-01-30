#ifndef SYNTAXHIGHLIGHTER_H
#define SYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>

class SyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    // Constructor: Needs the text document to highlight
    explicit SyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    // This function is automatically called when text changes
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    // Vector to hold all our rules (keywords, strings, etc.)
    QVector<HighlightingRule> highlightingRules;

    // Formats for specific types of text
    QTextCharFormat keywordFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat singleLineCommentFormat;
};

#endif // SYNTAXHIGHLIGHTER_H
