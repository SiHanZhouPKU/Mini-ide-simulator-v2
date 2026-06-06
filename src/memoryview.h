#ifndef MEMORYVIEW_H
#define MEMORYVIEW_H

#include <QWidget>
#include <QPainter>
#include <map>
#include <string>
#include <vector>
#include <utility>
#include "variant_value.h"

/**
 * MemoryView — 内存可视化组件。
 *
 * 用 "教学模拟地址"（0x1000 起始，4 字节对齐）模拟内存布局，
 * 每个变量在内存中占据一块区域，用彩色方块绘制。
 *
 * 支持示意图：
 *   ┌────────┬────────┬────────┬────────┐
 *   │ 0x1000 │ 0x1004 │ 0x1008 │ 0x100C │
 *   │  a=5   │  b=10  │  c='x' │  arr   │
 *   │ ──int──│ ──int──│ ──char─│ ──vec──│
 *   └────────┴────────┴────────┴────────┘
 */
class MemoryView : public QWidget {
    Q_OBJECT
public:
    explicit MemoryView(QWidget* parent = nullptr);

    void setMemoryData(const std::map<std::string, std::pair<int, VariantValue>>& data);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::map<std::string, std::pair<int, VariantValue>> memoryData_;

    QColor colorForType(VariantValue::Type type) const;
    void updateSizeHint();
};

#endif // MEMORYVIEW_H
