#pragma once

#include <QColor>
#include <QHash>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVariantList>

class MarkdownHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit MarkdownHighlighter(QTextDocument *document);

    void setDarkMode(bool darkMode);
    void setColors(const QString &background, const QString &foreground, const QString &accent);
    void setSearch(const QString &query, int currentMatchStart);
    // The block holding the caret shows its raw markdown; all others render.
    void setActiveBlock(int blockNumber);
    // Task boxes as {pos: offset of "[", done: bool}, for the editor to draw.
    QVariantList tasks() const { return m_tasks; }

    enum class Role { Hide, Dim, Plain, Bold, Italic, Strike, Code, Quote, Link, Image, Html,
                      Done, Syntax, Heading1, Heading2, Heading3, Heading4, Heading5, Heading6 };
    struct Run {
        int start;
        int length;
        Role role;
        QColor color = {};  // Syntax only: the code token's color
        bool operator==(const Run &) const = default;
    };
    struct Task {
        int pos;
        bool done;
    };
    struct Parsed {
        QList<Run> runs;  // document offsets
        QList<Task> tasks;
    };
    // md4c does the parsing; this classifies every character for styling.
    static Parsed parse(const QString &text);

signals:
    void tasksChanged();

protected:
    void highlightBlock(const QString &text) override;

private:
    void rebuildFormats();
    void ensureParsed();
    QTextCharFormat headingFormat(int level) const;
    void highlightSearch(const QString &text);

    bool m_darkMode = true;
    int m_activeBlock = -1;
    int m_parsedRevision = -1;
    QHash<int, QList<Run>> m_blockRuns;
    QVariantList m_tasks;
    QString m_customBackground;
    QString m_customForeground;
    QString m_customAccent;
    QTextCharFormat m_markerFormat;
    QTextCharFormat m_hiddenMarkerFormat;
    QTextCharFormat m_headingFormat;
    QTextCharFormat m_boldFormat;
    QTextCharFormat m_italicFormat;
    QTextCharFormat m_strikeFormat;
    QTextCharFormat m_checkboxFormat;
    QTextCharFormat m_codeFormat;
    QTextCharFormat m_quoteFormat;
    QTextCharFormat m_linkFormat;
    QString m_searchQuery;
    int m_currentMatchStart = -1;
    QTextCharFormat m_searchFormat;
    QTextCharFormat m_currentSearchFormat;
};
