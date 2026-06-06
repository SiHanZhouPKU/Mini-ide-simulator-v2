#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QHeaderView>
#include <QComboBox>
#include <QCheckBox>

#include "codeeditor.h"
#include "lexer.h"
#include "parser.h"
#include "executor.h"
#include "snapshot.h"
#include "memoryview.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onRunClicked();
    void onStepClicked();
    void onResetClicked();
    void onSliderMoved(int value);
    void onConsoleInput();
    void onSpeedChanged(int index);

private:
    // GUI
    CodeEditor* codeEditor_;
    QTableWidget* varTable_;
    QPlainTextEdit* consoleOutput_;
    QLineEdit* consoleInput_;
    MemoryView* memoryView_;
    QPushButton* runBtn_;
    QPushButton* stepBtn_;
    QPushButton* resetBtn_;
    QPushButton* autoPlayBtn_;
    QSlider* timeSlider_;
    QLabel* stepInfo_;
    QComboBox* speedCombo_;
    QCheckBox* autoScrollCheck_;

    // Engine
    Lexer* lexer_ = nullptr;
    Parser* parser_ = nullptr;
    Executor* executor_;
    SnapshotManager* snapshotManager_;
    std::unique_ptr<Program> program_;
    std::vector<int> changedVars_; // indices of changed rows for highlighting

    // State
    bool programLoaded_ = false;
    std::string pendingConsoleInput_;
    bool autoPlaying_ = false;
    int autoPlayTimerId_ = 0;

    void setupUI();
    void setupLayout(QWidget* central);
    void setupConnections();

    bool loadAndParseProgram();
    void refreshSnapshotView(int index);
    void refreshVarTable(const std::map<std::string, VariantValue>& vars,
                         const std::map<std::string, VariantValue>* oldVars = nullptr);
    void refreshConsoleOutput(const std::string& text);
    void refreshMemoryView(const std::map<std::string, std::pair<int, VariantValue>>& memMap);
    void updateButtonStates();

    std::string requestConsoleInput();

protected:
    void timerEvent(QTimerEvent* event) override;
};

#endif // MAINWINDOW_H
