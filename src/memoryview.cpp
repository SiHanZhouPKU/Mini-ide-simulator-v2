#include "memoryview.h"
#include <QPainter>
#include <QToolTip>
#include <algorithm>

MemoryView::MemoryView(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(140);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

void MemoryView::setMemoryData(const std::map<std::string, std::pair<int, VariantValue>>& data) {
    memoryData_ = data;
    updateSizeHint();
    update();
}

static int blockWidth(int byteSize) {
    // 4 px per byte — makes container growth visually obvious
    int w = byteSize * 4;
    if (w < 60) w = 60;
    if (w > 350) w = 350;
    return w;
}

static constexpr int kBlockHeight = 82;
static constexpr int kGapH = 6;
static constexpr int kGapV = 6;
static constexpr int kMargin = 10;

void MemoryView::updateSizeHint() {
    if (memoryData_.empty()) {
        setMinimumSize(200, 140);
        return;
    }

    // Flow layout: lay out blocks horizontally, wrap when exceeding a sensible width
    int maxRowW = 0;
    int rowW = kMargin;
    int rows = 1;

    for (const auto& [name, addrVal] : memoryData_) {
        int bw = blockWidth(addrVal.second.byteSize());
        if (rowW + bw + kGapH > 1100) {  // wrap at ~1100px
            maxRowW = std::max(maxRowW, rowW);
            rowW = kMargin;
            rows++;
        }
        rowW += bw + kGapH;
    }
    maxRowW = std::max(maxRowW, rowW);

    int totalH = rows * (kBlockHeight + kGapV) + kMargin * 2 + 30;
    setMinimumSize(std::min(maxRowW + 20, 1120), totalH);
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

    if (memoryData_.empty()) {
        painter.setPen(QColor(140, 140, 140));
        QFont f = painter.font();
        f.setPointSize(11);
        painter.setFont(f);
        painter.drawText(0, 0, w, h, Qt::AlignCenter,
                         "内存视图（运行后显示变量地址布局）");
        return;
    }

    // Title bar
    painter.setPen(QColor(200, 200, 200));
    QFont titleFont = painter.font();
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(0, 0, w, 22, Qt::AlignCenter,
                     "模拟内存布局 — 块宽度 ∝ 占用字节数");

    // Flow layout
    int x = kMargin;
    int y = 26;

    for (const auto& [name, addrVal] : memoryData_) {
        int addr = addrVal.first;
        const VariantValue& val = addrVal.second;
        int byteSz = val.byteSize();
        int bw = blockWidth(byteSz);
        QColor bg = colorForType(val.type);

        // Wrap to next row if needed
        if (x + bw > w - kMargin && x > kMargin) {
            x = kMargin;
            y += kBlockHeight + kGapV;
        }

        // Block background + border
        painter.fillRect(x, y, bw, kBlockHeight, bg);
        painter.setPen(QPen(Qt::white, 1));
        painter.drawRect(x, y, bw, kBlockHeight);

        // ---- Address range ----
        int endAddr = addr + byteSz;
        painter.setPen(QColor(40, 40, 40));
        QFont addrFont("Courier New", 8);
        addrFont.setBold(true);
        painter.setFont(addrFont);
        QString addrStr = QString("0x%1 – 0x%2")
            .arg(addr, 0, 16).arg(endAddr, 0, 16);
        painter.drawText(x + 3, y + 2, bw - 6, 14,
                         Qt::AlignLeft | Qt::AlignTop, addrStr);

        // ---- Variable name ----
        painter.setPen(Qt::white);
        QFont nameFont("Microsoft YaHei", 9);
        nameFont.setBold(true);
        painter.setFont(nameFont);
        QString nameStr = QString::fromStdString(name);
        painter.drawText(x + 3, y + 17, bw - 6, 18,
                         Qt::AlignLeft | Qt::AlignTop, nameStr);

        // ---- Value (elide if too long) ----
        painter.setPen(QColor(255, 255, 200));
        QFont valFont("Courier New", 8);
        painter.setFont(valFont);
        QString valStr = QString::fromStdString(val.toString());
        // Truncate long values for narrow blocks
        QFontMetrics fm(valFont);
        if (fm.horizontalAdvance(valStr) > bw - 6) {
            valStr = fm.elidedText(valStr, Qt::ElideRight, bw - 6);
        }
        painter.drawText(x + 3, y + 35, bw - 6, 18,
                         Qt::AlignLeft | Qt::AlignTop, valStr);

        // ---- Type + byte size ----
        painter.setPen(QColor(40, 40, 40));
        QFont typeFont("Courier New", 8);
        painter.setFont(typeFont);
        QString typeStr = QString::fromStdString(val.typeName())
                          + QString("  (%1 B)").arg(byteSz);
        painter.drawText(x + 3, y + 55, bw - 6, 18,
                         Qt::AlignLeft | Qt::AlignTop, typeStr);

        x += bw + kGapH;
    }

    // "Next free address" indicator
    if (!memoryData_.empty()) {
        auto last = memoryData_.rbegin();
        int nextAddr = last->second.first + last->second.second.byteSize();
        painter.setPen(QColor(120, 120, 120));
        QFont hintFont("Courier New", 9);
        painter.setFont(hintFont);
        int hintY = y + kBlockHeight + 8;
        if (hintY + 16 < h) {
            painter.drawText(x + 4, hintY, 200, 16, Qt::AlignLeft,
                             QString("→ 下一空闲: 0x%1").arg(nextAddr, 0, 16));
        }
    }
}
