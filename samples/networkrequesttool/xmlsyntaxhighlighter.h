#ifndef XMLSYNTAXHIGHLIGHTER_H
#define XMLSYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class XmlSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit XmlSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    QTextCharFormat m_tagFormat;
    QTextCharFormat m_attrFormat;
    QTextCharFormat m_valueFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_specialFormat;
};

#endif // XMLSYNTAXHIGHLIGHTER_H
