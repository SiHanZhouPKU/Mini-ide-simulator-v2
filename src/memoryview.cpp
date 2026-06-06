#include "memoryview.h"
#include <QPainter>
#include <QToolTip>

MemoryView::MemoryView(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

void MemoryView::setMemoryData(const std::map<std::string, std::pair<int, VariantValue>>& data) {
    memoryData_ = data;
    updateSizeHint();
    update();
}

void MemoryView::updateSizeHint() {
    if (memoryData_.empty()) {
        setMinimumSize(200, 120);
        return;
    }
    const int cellW = 90;
    const int cellH = 70;
    const int margin = 10;
    int n = (int)memoryData_.size();
    int cols = std::min(n, 4);
    int rows = (n + cols - 1) / cols;
    int totalW = cellW * cols + margin * 2 + 20;
    int totalH = rows * cellH + margin * 2 + 60;
    setMinimumSize(totalW, totalH);
}

void MemoryView::clear() {
    memoryData_.clear();
    update();
}

QColor MemoryView::colorForType(VariantValue::Type type) const {
    switch (type) {
        case VariantValue::INT:           return QColor(100, 149, 237); // cornflower blue
        case VariantValue::FLOAT:         return QColor(60, 179, 113);  // medium sea green
        case VariantValue::DOUBLE:        return QColor(34, 139, 34);   // forest green
        case VariantValue::CHAR:          return QColor(255, 165, 0);   // orange
        case VariantValue::BOOL:          return QColor(186, 85, 211);  // medium orchid
        case VariantValue::STRING:        return QColor(255, 99, 71);   // tomato
        case VariantValue::VOID:          return QColor(128, 128, 128); // gray
        case VariantValue::NULL_PTR:      return QColor(169, 169, 169); // dark gray
        case VariantValue::VECTOR_INT:    return QColor(50, 205, 50);   // lime green
        case VariantValue::VECTOR_FLOAT:  return QColor(0, 206, 209);   // dark turquoise
        case VariantValue::VECTOR_DOUBLE: return QColor(0, 139, 139);   // dark cyan
        case VariantValue::VECTOR_CHAR:   return QColor(255, 140, 0);   // dark orange
        case VariantValue::VECTOR_BOOL:   return QColor(153, 50, 204);  // dark orchid
        case VariantValue::VECTOR_STRING: return QColor(220, 20, 60);   // crimson
        case VariantValue::LIST_INT:      return QColor(173, 255, 47);  // green yellow
        case VariantValue::LIST_FLOAT:    return QColor(152, 251, 152); // pale green
        case VariantValue::LIST_DOUBLE:   return QColor(143, 188, 143); // dark sea green
        case VariantValue::LIST_CHAR:     return QColor(255, 218, 185); // peach
        case VariantValue::LIST_BOOL:     return QColor(221, 160, 221); // plum
        case VariantValue::LIST_STRING:   return QColor(255, 182, 193); // light pink
        case VariantValue::SET_INT:       return QColor(255, 215, 0);   // gold
        case VariantValue::MAP_STRING_INT: return QColor(218, 165, 32); // goldenrod
        case VariantValue::OBJECT:        return QColor(70, 130, 180);  // steel blue
        case VariantValue::POINTER:       return QColor(205, 92, 92);   // indian red
    }
    return QColor(200, 200, 200);
}

void MemoryView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    // Background
    painter.fillRect(0, 0, w, h, QColor(30, 30, 30));
    painter.setPen(QColor(60, 60, 60));

    if (memoryData_.empty()) {
        painter.setPen(QColor(140, 140, 140));
        QFont f = painter.font();
        f.setPointSize(11);
        painter.setFont(f);
        painter.drawText(0, 0, w, h, Qt::AlignCenter,
                         "内存视图（运行后显示变量地址布局）");
        return;
    }

    // Calculate total memory cells needed
    // Each cell is 32px wide, variable name + address + value
    const int cellW = 90;
    const int cellH = 70;
    const int margin = 10;
    const int startX = margin;

    int x = startX;
    int y = margin;

    // Draw title
    painter.setPen(QColor(200, 200, 200));
    QFont titleFont = painter.font();
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(0, 0, w, 22, Qt::AlignCenter, "模拟内存布局 (4 字节对齐)");

    y = 26;

    painter.setFont(QFont("Courier New", 10));

    int count = 0;
    for (const auto& [name, addrVal] : memoryData_) {
        int addr = addrVal.first;
        const VariantValue& val = addrVal.second;
        QColor bg = colorForType(val.type);

        // Cell background
        painter.fillRect(x, y, cellW, cellH, bg);
        painter.setPen(Qt::white);
        painter.drawRect(x, y, cellW, cellH);

        // Address
        painter.setPen(QColor(40, 40, 40));
        QString addrStr = QString("0x%1").arg(addr, 0, 16);
        painter.drawText(x + 2, y + 2, cellW - 4, 14, Qt::AlignLeft | Qt::AlignTop, addrStr);

        // Variable name
        painter.setPen(Qt::white);
        QString nameStr = QString::fromStdString(name);
        painter.drawText(x + 2, y + 16, cellW - 4, 14, Qt::AlignLeft | Qt::AlignTop,
                         nameStr + ":");

        // Value
        painter.setPen(QColor(255, 255, 200));
        QString valStr = QString::fromStdString(val.toString());
        painter.drawText(x + 2, y + 30, cellW - 4, 18, Qt::AlignLeft | Qt::AlignTop, valStr);

        // Type
        painter.setPen(QColor(40, 40, 40));
        QString typeStr = QString::fromStdString(val.typeName());
        painter.drawText(x + 2, y + 48, cellW - 4, 14, Qt::AlignLeft | Qt::AlignTop, typeStr);

        x += cellW + 4;
        count++;

        // Wrap to next row
        if (x + cellW + margin > w && count % 4 == 0) {
            x = startX;
            y += cellH + 4;
        }
    }

    // Draw "arrow" pointing to next free address
    if (!memoryData_.empty()) {
        auto last = memoryData_.rbegin();
        int nextAddr = last->second.first + 4;
        painter.setPen(QColor(100, 100, 100));
        int arrowX = x + 4;
        if (arrowX + 60 < w) {
            painter.drawText(arrowX, y, 60, 20, Qt::AlignLeft,
                             QString("… → 0x%1").arg(nextAddr, 0, 16));
        }
    }
}