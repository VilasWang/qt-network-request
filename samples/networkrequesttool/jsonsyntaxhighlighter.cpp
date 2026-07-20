#include "jsonsyntaxhighlighter.h"
#include <QRegularExpression>

JsonSyntaxHighlighter::JsonSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    m_keyFormat.setForeground(QColor(52, 103, 201));
    m_keyFormat.setFontWeight(QFont::Bold);

    m_stringFormat.setForeground(QColor(16, 124, 16));

    m_numberFormat.setForeground(QColor(204, 120, 50));

    m_keywordFormat.setForeground(QColor(170, 50, 180));
    m_keywordFormat.setFontWeight(QFont::Bold);

    m_punctFormat.setForeground(QColor(100, 100, 100));
}

void JsonSyntaxHighlighter::highlightBlock(const QString &text)
{
    // Apply value strings first (green) — will be overwritten for keys
    QRegularExpression strRe(R"("(?:[^"\\]|\\.)*")");
    auto it = strRe.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_stringFormat);
    }

    // Override keys (blue) — strings followed by colon
    QRegularExpression keyRe(R"("(?:[^"\\]|\\.)*"\s*:)");
    it = keyRe.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        int strLen = match.captured().lastIndexOf('"') + 1;
        setFormat(match.capturedStart(), strLen, m_keyFormat);
    }

    // Numbers
    QRegularExpression numRe(R"(\b-?\d+\.?\d*(?:[eE][+-]?\d+)?\b)");
    it = numRe.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_numberFormat);
    }

    // Keywords
    QRegularExpression kwRe(R"(\b(?:true|false|null)\b)");
    it = kwRe.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_keywordFormat);
    }

    // Punctuation
    QRegularExpression punctRe(R"([{}[\](),])");
    it = punctRe.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_punctFormat);
    }
}
