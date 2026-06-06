#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QWidget>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

class QPaintEvent;

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(class CodeEditor* editor);
protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;
private:
    CodeEditor* editor_;
};

/**
 * CppHighlighter — C++ 语法高亮器。
 * 高亮关键字、数字、字符串、注释、预处理器指令。
 */
class CppHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit CppHighlighter(QTextDocument* parent = nullptr);
protected:
    void highlightBlock(const QString& text) override;
private:
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    std::vector<HighlightRule> rules_;
    QRegularExpression commentStartExpr_;
    QRegularExpression commentEndExpr_;
    QTextCharFormat multiLineCommentFormat_;
};

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CodeEditor(QWidget* parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent* event);
    int lineNumberAreaWidth() const;
    void highlightLine(int line);
    int highlightedLine() const { return highlightedLine_; }

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect& rect, int dy);

private:
    QWidget* lineNumberArea_;
    int highlightedLine_ = 0;
};

#endif // CODEEDITOR_H
