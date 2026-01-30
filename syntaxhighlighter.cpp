#include "syntaxhighlighter.h"

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // --- 1. KEYWORDS (Light Blue) ---
    // Hex: #569CD6 -> RGB: 86, 156, 214
    keywordFormat.setForeground(QColor(86, 156, 214));

    const QString keywordPatterns[] = {
        QStringLiteral("\\bchar\\b"), QStringLiteral("\\bclass\\b"), QStringLiteral("\\bconst\\b"),
        QStringLiteral("\\bdouble\\b"), QStringLiteral("\\benum\\b"), QStringLiteral("\\bexplicit\\b"),
        QStringLiteral("\\bfriend\\b"), QStringLiteral("\\binline\\b"), QStringLiteral("\\bint\\b"),
        QStringLiteral("\\blong\\b"), QStringLiteral("\\bnamespace\\b"), QStringLiteral("\\boperator\\b"),
        QStringLiteral("\\bprivate\\b"), QStringLiteral("\\bprotected\\b"), QStringLiteral("\\bpublic\\b"),
        QStringLiteral("\\bshort\\b"), QStringLiteral("\\bsignals\\b"), QStringLiteral("\\bsigned\\b"),
        QStringLiteral("\\bslots\\b"), QStringLiteral("\\bstatic\\b"), QStringLiteral("\\bstruct\\b"),
        QStringLiteral("\\btemplate\\b"), QStringLiteral("\\btypedef\\b"), QStringLiteral("\\btypename\\b"),
        QStringLiteral("\\bunion\\b"), QStringLiteral("\\bunsigned\\b"), QStringLiteral("\\bvirtual\\b"),
        QStringLiteral("\\bvoid\\b"), QStringLiteral("\\bvolatile\\b"), QStringLiteral("\\bbool\\b"),
        QStringLiteral("\\breturn\\b"), QStringLiteral("\\bif\\b"), QStringLiteral("\\belse\\b"),
        QStringLiteral("\\bfor\\b"), QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bdo\\b"),
        QStringLiteral("\\bswitch\\b"), QStringLiteral("\\bcase\\b"), QStringLiteral("\\btrue\\b"),
        QStringLiteral("\\bfalse\\b"), QStringLiteral("\\bnullptr\\b"), QStringLiteral("\\busing\\b"),
        QStringLiteral("\\bthis\\b"), QStringLiteral("\\bnew\\b"), QStringLiteral("\\bdelete\\b")
    };

    for (const QString &pattern : std::as_const(keywordPatterns)) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // --- 2. STANDARD TYPES (Teal/Greenish-Blue) ---
    // Hex: #4EC9B0 -> RGB: 78, 201, 176
    QTextCharFormat typeFormat;
    typeFormat.setForeground(QColor(78, 201, 176));

    const QString typePatterns[] = {
        QStringLiteral("\\bstring\\b"), QStringLiteral("\\bvector\\b"), QStringLiteral("\\bstack\\b"),
        QStringLiteral("\\bqueue\\b"), QStringLiteral("\\bmap\\b"), QStringLiteral("\\bset\\b"),
        QStringLiteral("\\bpair\\b"), QStringLiteral("\\bdeque\\b"), QStringLiteral("\\blist\\b"),
        QStringLiteral("\\bstd\\b")
    };

    for (const QString &pattern : std::as_const(typePatterns)) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = typeFormat;
        highlightingRules.append(rule);
    }

    // --- 3. FUNCTIONS (Light Yellow) ---
    // Hex: #DCDCAA -> RGB: 220, 220, 170
    QTextCharFormat functionFormat;
    functionFormat.setForeground(QColor(220, 220, 170));

    HighlightingRule functionRule;
    functionRule.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z0-9_]+(?=\\()"));
    functionRule.format = functionFormat;
    highlightingRules.append(functionRule);

    // --- 4. STRINGS (Orange/Clay) ---
    // Hex: #CE9178 -> RGB: 206, 145, 120
    quotationFormat.setForeground(QColor(206, 145, 120));

    HighlightingRule quotationRule;
    quotationRule.pattern = QRegularExpression(QStringLiteral("\".*\""));
    quotationRule.format = quotationFormat;
    highlightingRules.append(quotationRule);

    // --- 5. COMMENTS (Green) ---
    // Hex: #6A9955 -> RGB: 106, 153, 85
    singleLineCommentFormat.setForeground(QColor(106, 153, 85));

    HighlightingRule singleLineCommentRule;
    singleLineCommentRule.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
    singleLineCommentRule.format = singleLineCommentFormat;
    highlightingRules.append(singleLineCommentRule);

    // --- 6. NUMBERS (Light Green) ---
    // Hex: #B5CEA8 -> RGB: 181, 206, 168
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor(181, 206, 168));

    HighlightingRule numberRule;
    numberRule.pattern = QRegularExpression(QStringLiteral("\\b[0-9]+\\.?[0-9]*\\b"));
    numberRule.format = numberFormat;
    highlightingRules.append(numberRule);

    // ... (Your existing Number rules) ...

    // --- 7. PREPROCESSOR DIRECTIVES (Violet/Purple) ---
    // Matches: #include, #define, #ifdef, #pragma
    // Hex: #C586C0 -> RGB: 197, 134, 192
    QTextCharFormat preprocessorFormat;
    preprocessorFormat.setForeground(QColor(197, 134, 192));

    const QString preprocessorPatterns[] = {
        QStringLiteral("^\\s*#\\s*include\\b"),
        QStringLiteral("^\\s*#\\s*define\\b"),
        QStringLiteral("^\\s*#\\s*ifdef\\b"),
        QStringLiteral("^\\s*#\\s*ifndef\\b"),
        QStringLiteral("^\\s*#\\s*endif\\b"),
        QStringLiteral("^\\s*#\\s*if\\b"),
        QStringLiteral("^\\s*#\\s*else\\b"),
        QStringLiteral("^\\s*#\\s*pragma\\b")
    };

    for (const QString &pattern : std::as_const(preprocessorPatterns)) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = preprocessorFormat;
        highlightingRules.append(rule);
    }

    // --- 8. HEADER FILES (Orange/Clay) ---
    // Matches: <vector>, <iostream> inside includes
    // We reuse the existing quotationFormat (Orange) so it looks like a string
    HighlightingRule headerRule;
    headerRule.pattern = QRegularExpression(QStringLiteral("<[a-zA-Z0-9_\\.]+>"));
    headerRule.format = quotationFormat;
    highlightingRules.append(headerRule);
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : std::as_const(highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}
