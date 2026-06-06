#include "mainwindow.h"
#include <QMessageBox>
#include <QScrollBar>
#include <QScrollArea>
#include <QEventLoop>
#include <QTimer>
#include <QApplication>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {

    executor_ = new Executor();
    snapshotManager_ = new SnapshotManager();
    executor_->setSnapshotManager(snapshotManager_);

    setupUI();
    setupConnections();
}

// ======== UI Setup ========

void MainWindow::setupUI() {
    auto* central = new QWidget(this);
    central->setStyleSheet("background-color: #1e1e1e;");
    setCentralWidget(central);
    setupLayout(central);
}

void MainWindow::setupLayout(QWidget* central) {
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // ====== Top area: Code Editor (left) + Panels (right) ======
    auto* topSplitter = new QSplitter(Qt::Horizontal, central);

    // Left: Code Editor
    codeEditor_ = new CodeEditor(topSplitter);
    topSplitter->addWidget(codeEditor_);

    // Right: Variable table + Console + Memory view — use a vertical splitter
    auto* rightSplitter = new QSplitter(Qt::Vertical, topSplitter);
    rightSplitter->setStyleSheet(
        "QSplitter::handle { background-color: #333; height: 2px; }"
    );
    topSplitter->addWidget(rightSplitter);

    // Variable table section
    auto* varSection = new QWidget(rightSplitter);
    varSection->setStyleSheet("background-color: #1e1e1e;");
    auto* varSectionLayout = new QVBoxLayout(varSection);
    varSectionLayout->setContentsMargins(0, 0, 0, 0);
    varSectionLayout->setSpacing(2);

    auto* varLabel = new QLabel("变量监控 (带变化高亮)", varSection);
    varLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #d4d4d4;");
    varSectionLayout->addWidget(varLabel);

    varTable_ = new QTableWidget(0, 3, varSection);
    varTable_->setHorizontalHeaderLabels({"变量名", "类型", "值"});
    varTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    varTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    varTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    varTable_->setWordWrap(false);
    varTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    varTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    varTable_->verticalHeader()->setVisible(false);
    varTable_->setAlternatingRowColors(true);
    varTable_->setStyleSheet(
        "QTableWidget { background-color: #252526; color: #d4d4d4; gridline-color: #3c3c3c; }"
        "QTableWidget::item { padding: 4px; }"
        "QHeaderView::section { background-color: #333333; color: #d4d4d4; padding: 4px; border: 1px solid #3c3c3c; }"
        "QTableWidget::item:alternate { background-color: #2d2d2d; }"
    );
    varSectionLayout->addWidget(varTable_);
    rightSplitter->addWidget(varSection);
    rightSplitter->setStretchFactor(0, 1);

    // Console section
    auto* consoleSection = new QWidget(rightSplitter);
    consoleSection->setStyleSheet("background-color: #1e1e1e;");
    auto* consoleSectionLayout = new QVBoxLayout(consoleSection);
    consoleSectionLayout->setContentsMargins(0, 0, 0, 0);
    consoleSectionLayout->setSpacing(2);

    auto* consoleLabel = new QLabel("控制台输出", consoleSection);
    consoleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #d4d4d4;");
    consoleSectionLayout->addWidget(consoleLabel);

    consoleOutput_ = new QPlainTextEdit(consoleSection);
    consoleOutput_->setReadOnly(true);
    consoleOutput_->setMaximumBlockCount(2000);
    QFont monoFont("Courier New", 15);
    monoFont.setStyleHint(QFont::Monospace);
    consoleOutput_->setFont(monoFont);
    consoleOutput_->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4;");
    consoleSectionLayout->addWidget(consoleOutput_);

    consoleInput_ = new QLineEdit(consoleSection);
    consoleInput_->setPlaceholderText("输入 cin 数据按回车提交...");
    consoleInput_->setEnabled(false);
    consoleInput_->setFont(monoFont);
    consoleInput_->setStyleSheet(
        "QLineEdit { background-color: #3c3c3c; color: #d4d4d4; padding: 4px; border: 1px solid #555; }"
    );
    consoleSectionLayout->addWidget(consoleInput_);
    rightSplitter->addWidget(consoleSection);
    rightSplitter->setStretchFactor(1, 1);

    // Memory view section
    auto* memSection = new QWidget(rightSplitter);
    memSection->setStyleSheet("background-color: #1e1e1e;");
    auto* memSectionLayout = new QVBoxLayout(memSection);
    memSectionLayout->setContentsMargins(0, 0, 0, 0);
    memSectionLayout->setSpacing(2);

    auto* memLabel = new QLabel("内存布局视图（模拟地址）", memSection);
    memLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #d4d4d4;");
    memSectionLayout->addWidget(memLabel);

    auto* memScrollArea = new QScrollArea(memSection);
    memScrollArea->setWidgetResizable(false);
    memScrollArea->setStyleSheet(
        "QScrollArea { background-color: #1e1e1e; border: none; }"
        "QScrollBar:vertical { background: #2d2d2d; width: 10px; }"
        "QScrollBar:horizontal { background: #2d2d2d; height: 10px; }"
        "QScrollBar::handle:vertical { background: #555; min-height: 20px; border-radius: 5px; }"
        "QScrollBar::handle:horizontal { background: #555; min-width: 20px; border-radius: 5px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }"
    );
    memoryView_ = new MemoryView(memScrollArea);
    memScrollArea->setWidget(memoryView_);
    memSectionLayout->addWidget(memScrollArea);
    rightSplitter->addWidget(memSection);
    rightSplitter->setStretchFactor(2, 1);

    // Set initial sizes: 2:3:1 ratio
    rightSplitter->setSizes({300, 200, 150});
    topSplitter->setStretchFactor(0, 3);
    topSplitter->setStretchFactor(1, 2);

    mainLayout->addWidget(topSplitter, 1);

    // ====== Bottom control bar ======
    auto* bottomBar = new QWidget(central);
    auto* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(8);

    // Run / Step / Reset / AutoPlay buttons
    runBtn_ = new QPushButton("▶ Run", bottomBar);
    stepBtn_ = new QPushButton("→ Step", bottomBar);
    resetBtn_ = new QPushButton("↺ Reset", bottomBar);
    autoPlayBtn_ = new QPushButton("▶ Auto Play", bottomBar);

    runBtn_->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; padding: 6px 16px; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:disabled { background-color: #666; color: #999; }"
    );
    stepBtn_->setStyleSheet(
        "QPushButton { background-color: #2196F3; color: white; padding: 6px 16px; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #1976D2; }"
        "QPushButton:disabled { background-color: #666; color: #999; }"
    );
    resetBtn_->setStyleSheet(
        "QPushButton { background-color: #f44336; color: white; padding: 6px 16px; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #d32f2f; }"
    );
    autoPlayBtn_->setStyleSheet(
        "QPushButton { background-color: #FF9800; color: white; padding: 6px 16px; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #F57C00; }"
        "QPushButton:disabled { background-color: #666; color: #999; }"
    );

    bottomLayout->addWidget(runBtn_);
    bottomLayout->addWidget(stepBtn_);
    bottomLayout->addWidget(resetBtn_);
    bottomLayout->addWidget(autoPlayBtn_);

    // Speed selector
    auto* speedLabel = new QLabel("速度:", bottomBar);
    speedLabel->setStyleSheet("color: #d4d4d4;");
    bottomLayout->addWidget(speedLabel);

    speedCombo_ = new QComboBox(bottomBar);
    speedCombo_->addItems({"0.25x", "0.5x", "1x", "2x", "5x", "10x"});
    speedCombo_->setCurrentIndex(2); // default 1x
    speedCombo_->setStyleSheet(
        "QComboBox { background-color: #3c3c3c; color: #d4d4d4; padding: 4px; border: 1px solid #555; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background-color: #3c3c3c; color: #d4d4d4; selection-background-color: #094771; }"
    );
    bottomLayout->addWidget(speedCombo_);

    // Step info
    stepInfo_ = new QLabel("0 / 0 步   行号: -", bottomBar);
    stepInfo_->setStyleSheet("font-size: 12px; padding: 4px 8px; color: #d4d4d4;");
    bottomLayout->addWidget(stepInfo_);

    // Auto-scroll checkbox
    autoScrollCheck_ = new QCheckBox("自动滚动", bottomBar);
    autoScrollCheck_->setChecked(true);
    autoScrollCheck_->setStyleSheet("color: #d4d4d4;");
    bottomLayout->addWidget(autoScrollCheck_);

    // Time slider
    timeSlider_ = new QSlider(Qt::Horizontal, bottomBar);
    timeSlider_->setMinimum(0);
    timeSlider_->setMaximum(0);
    timeSlider_->setValue(0);
    timeSlider_->setEnabled(false);
    timeSlider_->setTickPosition(QSlider::NoTicks);
    timeSlider_->setStyleSheet(
        "QSlider::groove:horizontal { height: 6px; background: #333; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #888; width: 14px; margin: -4px 0; border-radius: 7px; }"
        "QSlider::handle:horizontal:hover { background: #aaa; }"
        "QSlider::sub-page:horizontal { background: #4CAF50; border-radius: 3px; }"
    );
    bottomLayout->addWidget(timeSlider_, 1);

    mainLayout->addWidget(bottomBar);
}

void MainWindow::setupConnections() {
    connect(runBtn_, &QPushButton::clicked, this, &MainWindow::onRunClicked);
    connect(stepBtn_, &QPushButton::clicked, this, &MainWindow::onStepClicked);
    connect(resetBtn_, &QPushButton::clicked, this, &MainWindow::onResetClicked);
    connect(timeSlider_, &QSlider::valueChanged, this, &MainWindow::onSliderMoved);
    connect(consoleInput_, &QLineEdit::returnPressed, this, &MainWindow::onConsoleInput);
    connect(speedCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onSpeedChanged);
    connect(autoPlayBtn_, &QPushButton::clicked, this, [this]() {
        if (autoPlaying_) {
            autoPlaying_ = false;
            killTimer(autoPlayTimerId_);
            autoPlayBtn_->setText("▶ Auto Play");
            return;
        }
        if (!programLoaded_) {
            if (!loadAndParseProgram()) return;
            programLoaded_ = true;
        }
        if (executor_->isFinished()) return;
        autoPlaying_ = true;
        autoPlayBtn_->setText("■ Stop");
        stepBtn_->setEnabled(false);

        int ms = speedCombo_->currentIndex() == 0 ? 800 :
                 speedCombo_->currentIndex() == 1 ? 400 :
                 speedCombo_->currentIndex() == 2 ? 200 :
                 speedCombo_->currentIndex() == 3 ? 100 :
                 speedCombo_->currentIndex() == 4 ? 40  : 20;
        autoPlayTimerId_ = startTimer(ms);
    });

    // Executor ui-thread yield callback
    executor_->setYieldCallback([this]() {
        QApplication::processEvents();
    });

    // Executor cin callback
    executor_->setCinCallback([this]() -> std::string {
        return requestConsoleInput();
    });

    // Executor completion callback
    executor_->setFinishCallback([this]() {
        QMetaObject::invokeMethod(this, [this]() {
            int total = snapshotManager_->count();
            if (total > 0) {
                timeSlider_->setMaximum(total - 1);
                timeSlider_->setValue(total - 1);
            }
            stepBtn_->setEnabled(false);
            consoleInput_->setEnabled(false);
            stepInfo_->setText(QString("%1 / %1 步   已完成").arg(total));
            if (autoPlaying_) {
                autoPlaying_ = false;
                killTimer(autoPlayTimerId_);
                autoPlayBtn_->setText("▶ Auto Play");
            }
        });
    });
}

// ======== Core Operations ========

bool MainWindow::loadAndParseProgram() {
    std::string source = codeEditor_->toPlainText().toStdString();

    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();

    Parser parser(tokens);
    program_ = parser.parse();

    // Check lexer error first
    if (!lexer.error().empty()) {
        QMessageBox::warning(this, "词法错误",
            QString::fromStdString(lexer.error()));
        return false;
    }

    // Check parser error — must check BEFORE the null-Program check
    // since the parser may set error_ but still return a valid Program
    // (e.g. missing ';' after a declaration)
    if (!parser.error().empty()) {
        QMessageBox::warning(this, "语法错误",
            QString::fromStdString(parser.error()));
        return false;
    }

    if (!program_ || !program_->mainFunc) {
        QMessageBox::warning(this, "解析错误", "解析失败: 需要 int main() { ... }");
        return false;
    }

    executor_->setAst(program_.get());
    executor_->reset();

    consoleOutput_->clear();
    consoleInput_->setEnabled(false);

    varTable_->setRowCount(0);
    memoryView_->clear();

    timeSlider_->setMaximum(0);
    timeSlider_->setValue(0);
    timeSlider_->setEnabled(false);
    stepInfo_->setText("0 / 0 步");
    codeEditor_->highlightLine(0);

    return true;
}

void MainWindow::onRunClicked() {
    if (!loadAndParseProgram()) return;

    executor_->runAll();

    if (!executor_->error().empty()) {
        consoleOutput_->appendPlainText(QString::fromStdString("[错误] " + executor_->error()));
        QMessageBox::warning(this, "执行错误", QString::fromStdString(executor_->error()));
    }

    int total = snapshotManager_->count();
    if (total > 0) {
        timeSlider_->setMaximum(total - 1);
        timeSlider_->setValue(total - 1);
        timeSlider_->setEnabled(true);
        refreshSnapshotView(total - 1);
    }

    updateButtonStates();
    consoleInput_->setEnabled(false);
    stepInfo_->setText(QString("%1 / %1 步   已完成").arg(total));
}

void MainWindow::onStepClicked() {
    if (!programLoaded_) {
        if (!loadAndParseProgram()) return;
        programLoaded_ = true;
    }

    if (executor_->isFinished()) {
        stepBtn_->setEnabled(false);
        return;
    }

    executor_->step();

    if (!executor_->error().empty()) {
        consoleOutput_->appendPlainText(QString::fromStdString("[错误] " + executor_->error()));
        QMessageBox::warning(this, "执行错误", QString::fromStdString(executor_->error()));
    }

    int total = snapshotManager_->count();
    if (total > 0) {
        timeSlider_->setMaximum(total - 1);
        timeSlider_->setValue(total - 1);
        timeSlider_->setEnabled(true);
        refreshSnapshotView(total - 1);
    }

    int step = executor_->executedCount();
    stepInfo_->setText(QString("%1 / ? 步").arg(step));
    updateButtonStates();

    if (executor_->isFinished()) {
        consoleInput_->setEnabled(false);
        int finalTotal = snapshotManager_->count();
        stepInfo_->setText(QString("%1 / %1 步   已完成").arg(finalTotal));
    }
}

void MainWindow::onResetClicked() {
    if (autoPlaying_) {
        autoPlaying_ = false;
        killTimer(autoPlayTimerId_);
        autoPlayBtn_->setText("▶ Auto Play");
    }
    programLoaded_ = false;

    executor_->reset();
    snapshotManager_->clear();

    varTable_->setRowCount(0);
    consoleOutput_->clear();
    consoleInput_->clear();
    consoleInput_->setEnabled(false);
    memoryView_->clear();

    timeSlider_->setMaximum(0);
    timeSlider_->setValue(0);
    timeSlider_->setEnabled(false);

    codeEditor_->highlightLine(0);
    stepInfo_->setText("0 / 0 步");

    runBtn_->setEnabled(true);
    stepBtn_->setEnabled(true);
    autoPlayBtn_->setEnabled(true);
    stepBtn_->setEnabled(true);
}

void MainWindow::onSliderMoved(int value) {
    if (value >= 0 && value < snapshotManager_->count()) {
        refreshSnapshotView(value);
        snapshotManager_->setCurrentIndex(value);
    }
}

void MainWindow::onConsoleInput() {
    pendingConsoleInput_ = consoleInput_->text().toStdString();
    consoleOutput_->appendPlainText("> " + consoleInput_->text());
    consoleInput_->clear();
    consoleInput_->setEnabled(false);
}

void MainWindow::onSpeedChanged(int index) {
    if (autoPlaying_) {
        killTimer(autoPlayTimerId_);
        int ms = index == 0 ? 800 :
                 index == 1 ? 400 :
                 index == 2 ? 200 :
                 index == 3 ? 100 :
                 index == 4 ? 40  : 20;
        autoPlayTimerId_ = startTimer(ms);
    }
}

void MainWindow::timerEvent(QTimerEvent* event) {
    if (!autoPlaying_) return;
    if (executor_->isFinished()) {
        autoPlaying_ = false;
        killTimer(autoPlayTimerId_);
        autoPlayBtn_->setText("▶ Auto Play");
        return;
    }
    onStepClicked();
}

std::string MainWindow::requestConsoleInput() {
    if (autoPlaying_) {
        // During auto-play, pause auto-play for input
        autoPlaying_ = false;
        killTimer(autoPlayTimerId_);
        autoPlayBtn_->setText("▶ Auto Play");
    }

    consoleInput_->setEnabled(true);
    consoleInput_->setFocus();
    consoleInput_->clear();
    pendingConsoleInput_.clear();

    consoleOutput_->appendPlainText("-- 程序需要输入 --");

    QEventLoop loop;
    QMetaObject::Connection conn = connect(consoleInput_, &QLineEdit::returnPressed,
                                           &loop, &QEventLoop::quit);
    loop.exec();
    disconnect(conn);

    consoleInput_->setEnabled(false);
    return pendingConsoleInput_;
}

void MainWindow::refreshSnapshotView(int index) {
    const Snapshot& snap = snapshotManager_->getSnapshot(index);

    // Get previous snapshot for change detection
    const std::map<std::string, VariantValue>* oldVars = nullptr;
    if (index > 0) {
        oldVars = &snapshotManager_->getSnapshot(index - 1).variables;
    }

    try {
        refreshVarTable(snap.variables, oldVars);
        refreshConsoleOutput(snap.consoleOutput);
        refreshMemoryView(snap.memoryMap);
        codeEditor_->highlightLine(snap.lineNumber);

        int total = snapshotManager_->count();
        stepInfo_->setText(QString("第 %1 / %2 步   行号: %3")
                           .arg(index + 1).arg(total).arg(snap.lineNumber));
    } catch (const std::bad_variant_access& e) {
        consoleOutput_->appendPlainText(QString::fromStdString(
            std::string("[快照显示错误] ") + e.what()));
    } catch (const std::exception& e) {
        consoleOutput_->appendPlainText(QString::fromStdString(
            std::string("[快照显示错误] ") + e.what()));
    }
}

void MainWindow::refreshVarTable(const std::map<std::string, VariantValue>& vars,
                                  const std::map<std::string, VariantValue>* oldVars) {
    varTable_->setRowCount(static_cast<int>(vars.size()));

    int row = 0;
    for (const auto& [name, val] : vars) {
        auto* nameItem = new QTableWidgetItem(QString::fromStdString(name));
        auto* typeItem = new QTableWidgetItem(QString::fromStdString(val.typeName()));
        auto* valItem  = new QTableWidgetItem(QString::fromStdString(val.toString()));

        // Change detection: highlight changed values
        if (oldVars) {
            auto it = oldVars->find(name);
            if (it != oldVars->end() && it->second.toString() != val.toString()) {
                // Value changed — highlight
                QColor highlight(0, 120, 215, 80);
                nameItem->setBackground(highlight);
                typeItem->setBackground(highlight);
                valItem->setBackground(highlight);
            } else if (it == oldVars->end()) {
                // New variable
                QColor highlight(0, 200, 0, 40);
                nameItem->setBackground(highlight);
                typeItem->setBackground(highlight);
                valItem->setBackground(highlight);
            }
        }

        varTable_->setItem(row, 0, nameItem);
        varTable_->setItem(row, 1, typeItem);
        varTable_->setItem(row, 2, valItem);
        row++;
    }
}

void MainWindow::refreshConsoleOutput(const std::string& text) {
    consoleOutput_->clear();
    consoleOutput_->appendPlainText(QString::fromStdString(text));
    if (autoScrollCheck_->isChecked()) {
        consoleOutput_->verticalScrollBar()->setValue(
            consoleOutput_->verticalScrollBar()->maximum());
    }
}

void MainWindow::refreshMemoryView(const std::map<std::string, std::pair<int, VariantValue>>& memMap) {
    memoryView_->setMemoryData(memMap);
}

void MainWindow::updateButtonStates() {
    bool running = programLoaded_ && !executor_->isFinished();
    stepBtn_->setEnabled(running);
    autoPlayBtn_->setEnabled(running || !programLoaded_);
}
