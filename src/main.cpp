#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Dark theme for the whole app
    app.setStyleSheet(
        "QMainWindow { background-color: #1e1e1e; }"
        "QToolTip { background-color: #333; color: #d4d4d4; border: 1px solid #555; }"
        "QMessageBox { background-color: white; color: black; }"
        "QMessageBox QLabel { background-color: white; color: black; }"
        "QMessageBox QPushButton { min-width: 80px; min-height: 24px; }"
    );

    MainWindow window;
    window.setWindowTitle("Mini C++ IDE Simulator v2 — 教学级代码执行可视化调试器");
    window.resize(1300, 850);
    window.show();
    return app.exec();
}