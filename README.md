# Mini C++ IDE Simulator V2

> 🎓 教学级 C++ 代码执行可视化调试器 — 一个**看得见的内存实验室**

[![Qt 6](https://img.shields.io/badge/Qt-6.11-green)](https://www.qt.io/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/license-MIT-lightgrey)](LICENSE)

你写一段 C++ 代码，它不仅能运行，还能把**每一行代码执行时内存里发生了什么**用图形界面展示出来——变量存在哪个地址、占多少字节、值怎么变化，一目了然。

![screenshot](https://via.placeholder.com/800x450?text=Mini+IDE+Simulator+V2+Screenshot)

---

## 🎯 目标用户

正在学习 C++ 的初学者。很多同学在学指针、vector、内存分配时觉得抽象，就是因为**看不到内存里实际发生了什么**。这个工具用彩色方块把内存布局画出来，让抽象概念变得直观可见。

---

## 🏗️ 架构概览

```
用户输入代码
    ↓
Lexer 分词（~60 种 Token）
    ↓
Parser 构建 AST（递归下降，~30 种节点）
    ↓
Executor 解释执行 ←→ SnapshotManager 记录快照
    ↓                              ↓
GUI 更新显示              ←   读取快照数据
（编辑器高亮 + 变量表 + 内存视图 + 控制台）
```

| 模块 | 文件 | 功能 |
|------|------|------|
| 词法分析 | `src/lexer.h/cpp` | 60 种 Token，支持注释、字符串、数字 |
| 语法分析 | `src/parser.h/cpp` | 递归下降解析，完整运算符优先级 |
| AST 定义 | `src/ast.h` | 30+ 种节点类型（表达式、语句、顶层结构） |
| 解释执行 | `src/executor.h/cpp` | 树遍历解释器，符号表，内存映射 |
| 类型系统 | `src/variant_value.h` | `std::variant` 实现，25+ 运行时类型 |
| 快照系统 | `src/snapshot.h/cpp` | 时间轴回放，状态恢复 |
| 代码编辑器 | `src/codeeditor.h/cpp` | QPlainTextEdit，行号，语法高亮 |
| 内存视图 | `src/memoryview.h/cpp` | QPainter 绘制彩色内存方块 |
| 主窗口 | `src/mainwindow.h/cpp` | GUI 布局，事件处理，控制面板 |

---

## ✨ 功能特性

### 支持的语言特性

- **基本类型**: `int` `float` `double` `char` `bool` `string`
- **运算符**: 完整的算术、比较、逻辑、自增自减、复合赋值
- **控制流**: `if/else` `for` `while` `do-while` range-based for `break` `continue` `return`
- **I/O**: `cout <<` `cin >>` `endl`
- **STL 容器**: `vector<T>` `list<T>` `set<int>` `map<string,int>` (+ stack/queue 模拟)
- **异常处理**: `try/catch/throw`
- **类型转换**: `static_cast<T>()`
- **自定义类型**: `struct/class` 定义、成员访问
- **指针**: `nullptr` `new` `delete`

### GUI 功能

- ⬛ **内存可视化** — 彩色方块展示地址、类型、值、占用字节
- ⏱️ **时间轴回放** — 拖动滑块回溯任意执行步骤
- 🔍 **单步执行** — 逐行执行，当前行黄色高亮
- 📊 **变量监视表** — 实时显示所有变量，值变化时闪烁提示
- 🎮 **控制面板** — Run / Step / Reset / AutoPlay / 滑块调速

---

## 🔧 构建

### 依赖

- **Qt 6** (Widgets 模块)
- **CMake** ≥ 3.16
- **C++17** 编译器

### 编译步骤

```bash
cd Mini-ide-simulator-v2
mkdir build && cd build
cmake ..
make -j4
```

### 运行

```bash
./MiniIdeSimulatorV2
```

---

## 📁 项目结构

```
├── CMakeLists.txt          # CMake 构建配置
├── demo_full.cpp           # 完整功能演示（390 行）
├── 讲稿.md                  # 路演讲稿
├── test_parse.cpp          # 解析器测试
├── test_quick.cpp          # 快速回归测试
├── test_tmpl.cpp           # 模板解析测试
└── src/
    ├── main.cpp            # 程序入口 / 暗色主题
    ├── mainwindow.h/cpp    # 主窗口
    ├── codeeditor.h/cpp    # 代码编辑器
    ├── lexer.h/cpp         # 词法分析器
    ├── parser.h/cpp        # 语法分析器
    ├── ast.h               # AST 节点定义
    ├── executor.h/cpp      # 解释执行引擎
    ├── variant_value.h     # 动态类型系统
    ├── snapshot.h/cpp      # 快照系统
    └── memoryview.h/cpp    # 内存可视化组件
```

---

## 📖 详细文档

完整的技术介绍和设计原理请参阅 [讲稿.md](讲稿.md)。

---

## 🚀 未来计划

- 函数传参和递归调用
- 断点调试
- 表达式求值面板
- 内存编辑
- 内置示例代码库
- WebAssembly 在线版本

---

## 📄 License

MIT License

---

<p align="center">
  <i>把编译执行变成了每一步都能看、能点、能回放的交互体验</i>
</p>
