#include "codeeditor.h"
#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>

// ======== CppHighlighter ========

CppHighlighter::CppHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent) {

    // Keywords
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor(86, 156, 214));
    keywordFormat.setFontWeight(QFont::Bold);
    QStringList keywords = {
        // Types
        "\\bint\\b", "\\bfloat\\b", "\\bdouble\\b", "\\bchar\\b", "\\bbool\\b",
        "\\bstring\\b", "\\bvoid\\b", "\\bauto\\b",
        "\\blong\\b", "\\bshort\\b", "\\bunsigned\\b",
        // STL containers
        "\\bvector\\b", "\\blist\\b", "\\bmap\\b", "\\bset\\b",
        "\\bstack\\b", "\\bqueue\\b", "\\bdeque\\b",
        // Control flow
        "\\bif\\b", "\\belse\\b", "\\bwhile\\b", "\\bdo\\b",
        "\\bfor\\b", "\\bbreak\\b", "\\bcontinue\\b", "\\breturn\\b",
        "\\bswitch\\b", "\\bcase\\b", "\\bdefault\\b",
        // I/O
        "\\bcin\\b", "\\bcout\\b", "\\bendl\\b",
        "\\busing\\b", "\\bnamespace\\b", "\\bstd\\b", "\\binclude\\b",
        "\\bfstream\\b", "\\bifstream\\b", "\\bofstream\\b",
        "\\bopen\\b", "\\bclose\\b", "\\bgetline\\b",
        // OOP
        "\\bclass\\b", "\\bstruct\\b",
        "\\bpublic\\b", "\\bprivate\\b", "\\bprotected\\b",
        "\\bvirtual\\b", "\\boverride\\b", "\\bfinal\\b",
        "\\bconst\\b", "\\bstatic\\b", "\\bfriend\\b",
        "\\bthis\\b", "\\bnew\\b", "\\bdelete\\b", "\\bnullptr\\b",
        "\\bexplicit\\b", "\\boperator\\b",
        // Templates
        "\\btemplate\\b", "\\btypename\\b",
        // Exception
        "\\btry\\b", "\\bcatch\\b", "\\bthrow\\b",
        // Literals
        "\\btrue\\b", "\\bfalse\\b",
    };
    for (const QString& pattern : keywords) {
        HighlightRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        rules_.push_back(rule);
    }

    // Numbers
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor(181, 206, 168));
    HighlightRule numRule;
    numRule.pattern = QRegularExpression("\\b[0-9]+\\.?[0-9]*\\b");
    numRule.format = numberFormat;
    rules_.push_back(numRule);

    // String literals
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor(206, 145, 120));
    HighlightRule strRule;
    strRule.pattern = QRegularExpression("\"[^\"]*\"");
    strRule.format = stringFormat;
    rules_.push_back(strRule);

    // Char literals
    QTextCharFormat charFormat;
    charFormat.setForeground(QColor(206, 145, 120));
    HighlightRule chRule;
    chRule.pattern = QRegularExpression("'[^']*'");
    chRule.format = charFormat;
    rules_.push_back(chRule);

    // Single-line comments
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor(106, 153, 85));
    HighlightRule commentRule;
    commentRule.pattern = QRegularExpression("//[^\n]*");
    commentRule.format = commentFormat;
    rules_.push_back(commentRule);

    // Preprocessor directives
    QTextCharFormat preprocFormat;
    preprocFormat.setForeground(QColor(156, 220, 254));
    HighlightRule preprocRule;
    preprocRule.pattern = QRegularExpression("#[a-zA-Z_][a-zA-Z0-9_]*");
    preprocRule.format = preprocFormat;
    rules_.push_back(preprocRule);

    // Multi-line comments
    multiLineCommentFormat_.setForeground(QColor(106, 153, 85));
    commentStartExpr_ = QRegularExpression("/\\*");
    commentEndExpr_ = QRegularExpression("\\*/");
}

void CppHighlighter::highlightBlock(const QString& text) {
    // Apply single-line rules
    for (const auto& rule : rules_) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Multi-line comments
    setCurrentBlockState(0);
    int startIndex = 0;
    if (previousBlockState() != 1) {
        QRegularExpressionMatch startMatch = commentStartExpr_.match(text, startIndex);
        startIndex = startMatch.hasMatch() ? startMatch.capturedStart() : -1;
    }
    while (startIndex >= 0) {
        QRegularExpressionMatch endMatch = commentEndExpr_.match(text, startIndex + 2);
        int endIndex = endMatch.hasMatch() ? endMatch.capturedStart() : -1;
        int commentLength;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        }
        setFormat(startIndex, commentLength, multiLineCommentFormat_);
        QRegularExpressionMatch nextStart = commentStartExpr_.match(text, startIndex + 2);
        startIndex = nextStart.hasMatch() ? nextStart.capturedStart() : -1;
    }
}

// ======== LineNumberArea ========

LineNumberArea::LineNumberArea(CodeEditor* editor)
    : QWidget(editor), editor_(editor) {}

QSize LineNumberArea::sizeHint() const {
    return QSize(editor_->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent* event) {
    editor_->lineNumberAreaPaintEvent(event);
}

// ======== CodeEditor ========

CodeEditor::CodeEditor(QWidget* parent)
    : QPlainTextEdit(parent) {
    lineNumberArea_ = new LineNumberArea(this);

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &CodeEditor::updateLineNumberArea);

    updateLineNumberAreaWidth(0);

    // Monospace font
    QFont font("Courier New", 16);
    font.setStyleHint(QFont::Monospace);
    setFont(font);
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * 4);

    // Dark theme
    setStyleSheet(
        "QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; }"
    );

    // Syntax highlighter
    new CppHighlighter(document());

    // Default placeholder code
    setPlainText(
        "#include <iostream>\n"
        "using namespace std;\n"
        "\n"
        "int main() {\n"
        "    int a = 5;\n"
        "    int b = 10;\n"
        "    a = a + b;\n"
        "    cout << \"a = \" << a << endl;\n"
        "    // try string\n"
        "    string s = \"hello\";\n"
        "    s = s + \" world\";\n"
        "    cout << s << endl;\n"
        "    return 0;\n"
        "}\n"
    );
}

int CodeEditor::lineNumberAreaWidth() const {
    int digits = 1;
    int maxLines = qMax(1, blockCount());
    while (maxLines >= 10) { maxLines /= 10; digits++; }
    int space = 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect& rect, int dy) {
    if (dy)
        lineNumberArea_->scroll(0, dy);
    else
        lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(), rect.height());
    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    lineNumberArea_->setGeometry(QRect(cr.left(), cr.top(),
                                       lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::highlightLine(int line) {
    highlightedLine_ = line;
    setExtraSelections(QList<QTextEdit::ExtraSelection>());

    if (line > 0) {
        QTextEdit::ExtraSelection sel;
        QColor lineColor(255, 255, 200, 80); // translucent yellow
        sel.format.setBackground(lineColor);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = QTextCursor(document()->findBlockByNumber(line - 1));
        sel.cursor.clearSelection();
        setExtraSelections({sel});

        QTextBlock block = document()->findBlockByNumber(line - 1);
        if (block.isValid()) {
            QTextCursor cursor(block);
            setTextCursor(cursor);
            centerCursor();
        }
    }
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event) {
    QPainter painter(lineNumberArea_);
    painter.fillRect(event->rect(), QColor(37, 37, 38));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            int lineNum = blockNumber + 1;
            if (lineNum == highlightedLine_) {
                painter.fillRect(0, top, lineNumberArea_->width(),
                                 fontMetrics().height(), QColor(80, 80, 40));
            }
            painter.setPen(lineNum == highlightedLine_ ?
                           QColor(255, 200, 50) : QColor(120, 120, 120));
            painter.drawText(0, top, lineNumberArea_->width() - 5,
                             fontMetrics().height(),
                             Qt::AlignRight, QString::number(lineNum));
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        blockNumber++;
    }
}
