#include "markdownhighlighter.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QTextBlock>
#include <QTextDocument>

#include <md4c.h>

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document) {
    rebuildFormats();
}

void MarkdownHighlighter::setDarkMode(bool darkMode) {
    if (m_darkMode == darkMode)
        return;

    m_darkMode = darkMode;
    rebuildFormats();
    rehighlight();
}

void MarkdownHighlighter::setColors(const QString &background, const QString &foreground,
                                    const QString &accent) {
    if (m_customBackground == background && m_customForeground == foreground
            && m_customAccent == accent)
        return;

    m_customBackground = background;
    m_customForeground = foreground;
    m_customAccent = accent;
    rebuildFormats();
    rehighlight();
}

void MarkdownHighlighter::setSearch(const QString &query, int currentMatchStart) {
    if (m_searchQuery == query && m_currentMatchStart == currentMatchStart)
        return;
    m_searchQuery = query;
    m_currentMatchStart = currentMatchStart;
    rehighlight();
}

void MarkdownHighlighter::setActiveBlock(int blockNumber) {
    if (m_activeBlock == blockNumber)
        return;
    const int previous = m_activeBlock;
    m_activeBlock = blockNumber;
    if (!document())
        return;
    for (const int number : {previous, blockNumber}) {
        const QTextBlock block = document()->findBlockByNumber(number);
        if (block.isValid())
            rehighlightBlock(block);
    }
}

void MarkdownHighlighter::rebuildFormats() {
    const QColor marker = m_darkMode ? QColor(QStringLiteral("#4f525a"))
                                     : QColor(QStringLiteral("#aeb1b5"));
    const QColor background = !m_customBackground.isEmpty() ? QColor(m_customBackground)
        : (m_darkMode ? QColor(QStringLiteral("#101010")) : QColor(QStringLiteral("#ffffff")));
    const QColor text = !m_customForeground.isEmpty() ? QColor(m_customForeground)
        : (m_darkMode ? QColor(QStringLiteral("#eeeeee")) : QColor(QStringLiteral("#222324")));
    const QColor link = !m_customAccent.isEmpty() ? QColor(m_customAccent)
        : (m_darkMode ? QColor(QStringLiteral("#5584aa")) : QColor(QStringLiteral("#2077b2")));
    const QColor quote = marker;
    const QColor codeBackground = m_darkMode ? QColor(QStringLiteral("#1c1a1a"))
                                             : QColor(QStringLiteral("#f8f8f8"));

    m_markerFormat = QTextCharFormat();
    m_markerFormat.setForeground(marker);

    // A sub-pixel font size combined with a stretch factor used to make these
    // markers occupy (close to) zero space, but that combination deadlocks Qt's
    // font metrics engine on some platforms. Instead, use a normal font size and
    // cancel out its advance width with negative absolute letter-spacing.
    m_hiddenMarkerFormat = QTextCharFormat();
    m_hiddenMarkerFormat.setForeground(background);
    m_hiddenMarkerFormat.setFontPointSize(1.0);

    QFont hiddenFont = document() ? document()->defaultFont() : QFont();
    hiddenFont.setPointSizeF(1.0);
    const qreal charWidth = QFontMetricsF(hiddenFont).horizontalAdvance(QLatin1Char('['));

    m_hiddenMarkerFormat.setFontLetterSpacingType(QFont::AbsoluteSpacing);
    m_hiddenMarkerFormat.setFontLetterSpacing(-charWidth);

    m_headingFormat = QTextCharFormat();
    m_headingFormat.setForeground(text);
    m_headingFormat.setFontWeight(QFont::Bold);

    m_boldFormat = QTextCharFormat();
    m_boldFormat.setFontWeight(QFont::Bold);
    m_boldFormat.setForeground(text);

    m_italicFormat = QTextCharFormat();
    m_italicFormat.setFontItalic(true);
    m_italicFormat.setForeground(text);

    m_checkboxFormat = QTextCharFormat();
    m_checkboxFormat.setForeground(link);
    m_checkboxFormat.setFontWeight(QFont::Bold);

    m_strikeFormat = QTextCharFormat();
    m_strikeFormat.setFontStrikeOut(true);
    m_strikeFormat.setForeground(marker);

    m_codeFormat = QTextCharFormat();
    m_codeFormat.setForeground(text);
    m_codeFormat.setBackground(codeBackground);

    m_quoteFormat = QTextCharFormat();
    m_quoteFormat.setForeground(quote);
    m_quoteFormat.setFontItalic(true);

    m_linkFormat = QTextCharFormat();
    m_linkFormat.setForeground(link);
    m_linkFormat.setFontUnderline(true);

    m_searchFormat = QTextCharFormat();
    m_searchFormat.setBackground(m_darkMode ? QColor(QStringLiteral("#725b18"))
                                            : QColor(QStringLiteral("#ffe58a")));
    m_currentSearchFormat = QTextCharFormat();
    m_currentSearchFormat.setBackground(m_darkMode ? QColor(QStringLiteral("#b36b20"))
                                                   : QColor(QStringLiteral("#ffad42")));
}

// ---- Parsing: md4c finds the structure, we classify every character. ----
//
// md4c reports the document's visible text and the spans/blocks around it.
// Characters it never reports as text are syntax. Spans and headings hide
// theirs off the caret's line; list, quote, table and fence syntax stays dim.

namespace {

using Run = MarkdownHighlighter::Run;
using Role = MarkdownHighlighter::Role;

struct Open {
    int type;
    int first = -1;  // first and end of text seen inside, in QString offsets
    int end = -1;
    int level = 0;   // heading level / fence char / task state
};

struct Parser {
    QString text;
    QByteArray utf8;
    std::vector<int> charAt;  // UTF-8 byte offset -> QString offset
    QList<Open> blocks;
    QList<Open> spans;
    MarkdownHighlighter::Parsed out;

    int lineStart(int pos) const { return text.lastIndexOf(QLatin1Char('\n'), pos - 1) + 1; }
    int lineEnd(int pos) const {
        const int end = text.indexOf(QLatin1Char('\n'), pos);
        return end < 0 ? int(text.length()) : end;
    }
    void add(int start, int end, Role role) {
        if (end > start)
            out.runs.append({start, end - start, role});
    }
    // Dim the leading syntax of a line ("- ", "> ", "1. ", "[ ]", "|") up to `limit`.
    void dimPrefix(int line, int limit, QLatin1StringView chars) {
        int i = line;
        while (i < limit && (chars.contains(text.at(i)) || text.at(i).isSpace()))
            ++i;
        add(line, i, Role::Dim);
    }
    bool isAny(int i, QLatin1StringView chars) const {
        return i >= 0 && i < text.length() && chars.contains(text.at(i));
    }
    // End of a link/image tail starting at the "]": "](url)", "][ref]" or "]".
    int linkTail(int i) const {
        if (!isAny(i, QLatin1StringView("]")))
            return i;
        ++i;
        const QChar open = i < text.length() ? text.at(i) : QChar();
        const QChar close = open == QLatin1Char('(') ? QLatin1Char(')')
                          : open == QLatin1Char('[') ? QLatin1Char(']') : QChar();
        if (close.isNull())
            return i;
        int depth = 0;
        for (int j = i; j < text.length() && text.at(j) != QLatin1Char('\n'); ++j) {
            if (text.at(j) == QLatin1Char('\\')) { ++j; continue; }
            if (text.at(j) == open && open != close) ++depth;
            else if (text.at(j) == close && --depth <= 0) return j + 1;
            if (open == close && j > i && text.at(j) == close) return j + 1;
        }
        return i;
    }

    void text_(MD_TEXTTYPE type, const MD_CHAR *data, MD_SIZE size) {
        const qsizetype offset = data - utf8.constData();
        if (offset < 0 || offset + qsizetype(size) > utf8.size())
            return;  // NUL replacement and breaks point outside the input
        const int start = charAt[offset], end = charAt[offset + size];
        if (end <= start)
            return;
        for (Open &o : blocks) { if (o.first < 0) o.first = start; o.end = end; }
        for (Open &o : spans) { if (o.first < 0) o.first = start; o.end = end; }

        if (type == MD_TEXT_HTML) { add(start, end, Role::Html); return; }
        for (const Open &b : blocks) {
            switch (b.type) {
            case MD_BLOCK_H: add(start, end, Role(int(Role::Heading1) + b.level - 1)); break;
            case MD_BLOCK_CODE: add(start, end, Role::Code); break;
            case MD_BLOCK_QUOTE: add(start, end, Role::Quote); break;
            case MD_BLOCK_TH: add(start, end, Role::Bold); break;
            case MD_BLOCK_TD: add(start, end, Role::Plain); break;
            default: break;
            }
        }
        // Only the innermost item decides whether this text is done.
        for (auto it = blocks.crbegin(); it != blocks.crend(); ++it) {
            if (it->type == MD_BLOCK_LI) {
                if (it->level == 'x' || it->level == 'X')
                    add(start, end, Role::Done);
                break;
            }
        }
        for (const Open &s : spans) {
            switch (s.type) {
            case MD_SPAN_EM: add(start, end, Role::Italic); break;
            case MD_SPAN_STRONG: add(start, end, Role::Bold); break;
            case MD_SPAN_DEL: add(start, end, Role::Strike); break;
            case MD_SPAN_CODE: add(start, end, Role::Code); break;
            case MD_SPAN_A: add(start, end, Role::Link); break;
            case MD_SPAN_IMG: add(start, end, Role::Image); break;
            default: break;
            }
        }
    }

    void leaveSpan(const Open &s) {
        if (s.first < 0)
            return;  // nothing visible inside, e.g. "[](url)": leave it raw
        int open = s.first, close = s.end;
        const auto back = [&](QLatin1StringView chars) { while (isAny(open - 1, chars)) --open; };
        const auto fwd = [&](QLatin1StringView chars) { while (isAny(close, chars)) ++close; };
        switch (s.type) {
        case MD_SPAN_EM:
        case MD_SPAN_STRONG: back(QLatin1StringView("*_")); fwd(QLatin1StringView("*_")); break;
        case MD_SPAN_DEL: back(QLatin1StringView("~")); fwd(QLatin1StringView("~")); break;
        case MD_SPAN_CODE:
            if (isAny(open - 1, QLatin1StringView(" ")) && isAny(open - 2, QLatin1StringView("`"))) --open;
            back(QLatin1StringView("`"));
            if (isAny(close, QLatin1StringView(" ")) && isAny(close + 1, QLatin1StringView("`"))) ++close;
            fwd(QLatin1StringView("`"));
            break;
        case MD_SPAN_A:
            if (isAny(open - 1, QLatin1StringView("[<"))) --open;
            if (isAny(close, QLatin1StringView(">")) && text.at(open) == QLatin1Char('<')) ++close;
            else if (text.at(open) == QLatin1Char('[')) close = linkTail(close);
            break;
        case MD_SPAN_IMG:
            if (isAny(open - 1, QLatin1StringView("[")) && isAny(open - 2, QLatin1StringView("!"))) open -= 2;
            close = linkTail(close);
            break;
        default:
            return;
        }
        add(open, s.first, Role::Hide);
        add(s.end, close, Role::Hide);
    }

    void leaveBlock(const Open &b) {
        if (b.first < 0)
            return;
        switch (b.type) {
        case MD_BLOCK_H: {
            const int line = lineStart(b.first);
            int first = line;
            while (first < b.first && text.at(first).isSpace()) ++first;
            if (text.at(first) == QLatin1Char('#')) {
                add(line, b.first, Role::Hide);                    // "## "
                int i = b.end;
                while (isAny(i, QLatin1StringView(" #\t"))) ++i;
                if (text.mid(b.end, i - b.end).contains(QLatin1Char('#')))
                    add(b.end, i, Role::Hide);                      // closing "##"
            } else {
                const int under = lineEnd(b.end) + 1;               // setext "===" / "---"
                if (under < text.length())
                    add(under, lineEnd(under), Role::Hide);
            }
            break;
        }
        case MD_BLOCK_CODE:
            if (b.level) {  // fenced: dim the fence lines around the content
                const int open = lineStart(b.first) - 1;
                if (open > 0) add(lineStart(open - 1 < 0 ? 0 : open), open, Role::Dim);
                const int close = lineEnd(b.end > 0 ? b.end - 1 : 0) + 1;
                if (close < text.length()) add(close, lineEnd(close), Role::Dim);
            }
            break;
        case MD_BLOCK_TABLE:
            // Pipes and the delimiter row; cell text is Plain/Bold on top.
            add(lineStart(b.first), lineEnd(b.end), Role::Dim);
            break;
        case MD_BLOCK_QUOTE:
            for (int line = lineStart(b.first); line <= b.end && line < text.length(); line = lineEnd(line) + 1)
                dimPrefix(line, lineEnd(line), QLatin1StringView(">"));
            break;
        case MD_BLOCK_LI: {
            const int line = lineStart(b.first);
            dimPrefix(line, b.first, QLatin1StringView("-+*.)>0123456789[]xX"));
            break;
        }
        default:
            break;
        }
    }
};

}  // namespace

MarkdownHighlighter::Parsed MarkdownHighlighter::parse(const QString &text) {
    Parser p;
    p.text = text;
    p.utf8 = text.toUtf8();
    p.charAt.resize(p.utf8.size() + 1);
    int ci = 0;
    for (qsizetype i = 0; i < p.utf8.size(); ++i) {
        const uchar byte = uchar(p.utf8.at(i));
        p.charAt[i] = ci;
        if ((byte & 0xC0) != 0x80)
            ci += byte >= 0xF0 ? 2 : 1;  // 4-byte UTF-8 is a surrogate pair
        else
            p.charAt[i] = ci - 1;
    }
    p.charAt[p.utf8.size()] = ci;

    MD_PARSER parser{};
    parser.flags = MD_DIALECT_GITHUB;
    parser.enter_block = [](MD_BLOCKTYPE type, void *detail, void *data) {
        auto *p = static_cast<Parser *>(data);
        Open o{type};
        if (type == MD_BLOCK_H)
            o.level = int(static_cast<MD_BLOCK_H_DETAIL *>(detail)->level);
        else if (type == MD_BLOCK_CODE)
            o.level = static_cast<MD_BLOCK_CODE_DETAIL *>(detail)->fence_char;
        else if (type == MD_BLOCK_LI) {
            const auto *li = static_cast<MD_BLOCK_LI_DETAIL *>(detail);
            if (li->is_task) {
                o.level = li->task_mark;
                p->out.tasks.append({p->charAt[li->task_mark_offset] - 1, li->task_mark != ' '});
            }
        }
        p->blocks.append(o);
        return 0;
    };
    parser.leave_block = [](MD_BLOCKTYPE, void *, void *data) {
        auto *p = static_cast<Parser *>(data);
        const Open o = p->blocks.takeLast();
        p->leaveBlock(o);
        return 0;
    };
    parser.enter_span = [](MD_SPANTYPE type, void *, void *data) {
        static_cast<Parser *>(data)->spans.append(Open{type});
        return 0;
    };
    parser.leave_span = [](MD_SPANTYPE, void *, void *data) {
        auto *p = static_cast<Parser *>(data);
        const Open o = p->spans.takeLast();
        p->leaveSpan(o);
        return 0;
    };
    parser.text = [](MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *data) {
        static_cast<Parser *>(data)->text_(type, text, size);
        return 0;
    };
    md_parse(p.utf8.constData(), MD_SIZE(p.utf8.size()), &parser, &p);

    // Paint order: block-wide dims first, then content, then done-task dimming
    // over bold/links, hidden syntax last so nothing re-shows it.
    std::stable_sort(p.out.runs.begin(), p.out.runs.end(), [](const Run &a, const Run &b) {
        const auto rank = [](Role r) {
            return r == Role::Dim ? 0 : r == Role::Done ? 2 : r == Role::Hide ? 3 : 1;
        };
        return rank(a.role) < rank(b.role);
    });
    return p.out;
}

void MarkdownHighlighter::ensureParsed() {
    if (m_parsedRevision == document()->revision())
        return;
    m_parsedRevision = document()->revision();
    const Parsed parsed = parse(document()->toPlainText());

    // Bucket runs per block, clipped to it.
    QHash<int, QList<Run>> byBlock;
    for (const Run &run : parsed.runs) {
        for (QTextBlock block = document()->findBlock(run.start);
             block.isValid() && block.position() < run.start + run.length; block = block.next()) {
            const int start = qMax(run.start, block.position());
            const int end = qMin(run.start + run.length, block.position() + block.length() - 1);
            if (end > start)
                byBlock[block.blockNumber()].append({start - block.position(), end - start, run.role});
        }
    }
    // An edit can restyle other blocks (opening a fence, adding "==="); the
    // highlighter only revisits the edited one, so queue the rest.
    QList<int> changed;
    for (auto it = byBlock.cbegin(); it != byBlock.cend(); ++it)
        if (m_blockRuns.value(it.key()) != it.value()) changed.append(it.key());
    for (auto it = m_blockRuns.cbegin(); it != m_blockRuns.cend(); ++it)
        if (!byBlock.contains(it.key())) changed.append(it.key());
    m_blockRuns = byBlock;
    if (!changed.isEmpty()) {
        QMetaObject::invokeMethod(this, [this, changed] {
            for (const int number : changed) {
                const QTextBlock block = document()->findBlockByNumber(number);
                if (block.isValid()) rehighlightBlock(block);
            }
        }, Qt::QueuedConnection);
    }

    QVariantList tasks;
    for (const Task &task : parsed.tasks)
        tasks.append(QVariantMap{{QStringLiteral("pos"), task.pos}, {QStringLiteral("done"), task.done}});
    if (tasks != m_tasks) {
        m_tasks = tasks;
        emit tasksChanged();
    }
}

void MarkdownHighlighter::highlightBlock(const QString &text) {
    ensureParsed();
    const bool active = currentBlock().blockNumber() == m_activeBlock;
    for (const Run &run : m_blockRuns.value(currentBlock().blockNumber())) {
        QTextCharFormat style;
        switch (run.role) {
        case Role::Hide: setFormat(run.start, run.length, active ? m_markerFormat : m_hiddenMarkerFormat); continue;
        case Role::Dim: case Role::Html: style = m_markerFormat; break;
        case Role::Plain: style.setForeground(m_headingFormat.foreground()); break;
        case Role::Bold: style = m_boldFormat; break;
        case Role::Italic: style = m_italicFormat; break;
        case Role::Strike: style = m_strikeFormat; break;
        case Role::Code: style = m_codeFormat; break;
        case Role::Quote: style = m_quoteFormat; break;
        case Role::Link: style = m_linkFormat; break;
        case Role::Image:
            style = m_linkFormat;
            style.setFontUnderline(false);
            style.setFontItalic(true);
            break;
        case Role::Done:
            style.setFontStrikeOut(true);
            style.setForeground(m_strikeFormat.foreground());
            break;
        default:
            style = headingFormat(int(run.role) - int(Role::Heading1) + 1);
            break;
        }
        // Merge so nested styles add up (code in a link, bold in a done task).
        for (int i = run.start; i < run.start + run.length; ++i) {
            QTextCharFormat merged = format(i);
            merged.merge(style);
            setFormat(i, 1, merged);
        }
    }
    highlightSearch(text);
}

QTextCharFormat MarkdownHighlighter::headingFormat(int level) const {
    static const qreal scale[] = {1.6, 1.35, 1.15, 1.0, 1.0, 1.0};
    QTextCharFormat format = m_headingFormat;
    const int basePixels = document()->defaultFont().pixelSize();
    if (basePixels > 0)
        format.setProperty(QTextFormat::FontPixelSize, qRound(basePixels * scale[level - 1]));
    return format;
}

void MarkdownHighlighter::highlightSearch(const QString &text) {
    if (m_searchQuery.isEmpty())
        return;

    int from = 0;
    while ((from = text.indexOf(m_searchQuery, from, Qt::CaseInsensitive)) >= 0) {
        const int documentStart = currentBlock().position() + from;
        QTextCharFormat format = this->format(from);
        format.setBackground(documentStart == m_currentMatchStart
                                 ? m_currentSearchFormat.background()
                                 : m_searchFormat.background());
        setFormat(from, m_searchQuery.length(), format);
        from += qMax(1, m_searchQuery.length());
    }
}
