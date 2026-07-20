#include "xmlsyntaxhighlighter.h"
#include <QRegularExpression>

XmlSyntaxHighlighter::XmlSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    m_tagFormat.setForeground(QColor(0, 0, 200));
    m_tagFormat.setFontWeight(QFont::Bold);

    m_attrFormat.setForeground(QColor(200, 100, 0));

    m_valueFormat.setForeground(QColor(16, 124, 16));

    m_commentFormat.setForeground(QColor(140, 140, 140));
    m_commentFormat.setFontItalic(true);

    m_specialFormat.setForeground(QColor(100, 60, 160));
    m_specialFormat.setFontWeight(QFont::Bold);
}

void XmlSyntaxHighlighter::highlightBlock(const QString &text)
{
    // Comments
    QRegularExpression commentRe(R"(<!--[\s\S]*?-->)");
    auto it = commentRe.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_commentFormat);
    }

    // Special processing instructions (<?xml ... ?>)
    QRegularExpression specialRe(R"(<\?[\s\S]*?\?>)");
    it = specialRe.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_specialFormat);
    }

    // Attribute values (inside double or single quotes in tags)
    QRegularExpression attrValRe(R"("(?:[^"\\]|\\.)*"|'[^']*')");
    it = attrValRe.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        // Only highlight if inside a tag (between < and >)
        int pos = match.capturedStart();
        int lineStart = pos;
        int tagStart = text.lastIndexOf('<', pos);
        int tagEnd = text.indexOf('>', pos);
        if (tagStart != -1 && tagEnd != -1 && tagStart < pos && pos < tagEnd)
        {
            setFormat(match.capturedStart(), match.capturedLength(), m_valueFormat);
        }
    }

    // Attribute names (word followed by = inside tags)
    QRegularExpression attrRe(R"(\b\w+(?=\s*=\s*["']))");
    it = attrRe.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        int pos = match.capturedStart();
        int tagStart = text.lastIndexOf('<', pos);
        int tagEnd = text.indexOf('>', pos);
        if (tagStart != -1 && tagEnd != -1 && tagStart < pos && pos < tagEnd)
        {
            setFormat(match.capturedStart(), match.capturedLength(), m_attrFormat);
        }
    }

    // Tags: <tagname ...> or </tagname> or <tagname/>
    QRegularExpression tagRe(R"(</?[a-zA-Z_:][a-zA-Z0-9_:.-]*\b[\s\S]*?/?>)");
    // Simpler: just match tag brackets
    QRegularExpression bracketRe(R"(</?[a-zA-Z_:][a-zA-Z0-9_:.-]*)");
    it = bracketRe.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_tagFormat);
    }

    // Closing tag bracket with name
    QRegularExpression closeRe(R"(</[a-zA-Z_:][a-zA-Z0-9_:.-]*>)");
    it = closeRe.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_tagFormat);
    }

    // Self-closing tag end />
    QRegularExpression selfCloseRe(R"(/>)");
    it = selfCloseRe.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_tagFormat);
    }
}
