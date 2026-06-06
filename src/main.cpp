#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Dark theme for the whole app
    app.setStyleSheet(
        "QMainWindow { background-color: #1e1e1e; }"
        "QToolTip { background-color: #333; color: #d4d4d4; border: 1px solid #555; }"
        "QMessageBox { background-color: #f0f0f0; color: #222; }"
        "QMessageBox QLabel { background-color: transparent; color: #222; font-size: 13px; }"
        "QMessageBox QPushButton {"
        "  background-color: #0078d4; color: white;"
        "  border: none; border-radius: 4px;"
        "  min-width: 80px; min-height: 28px;"
        "  padding: 4px 16px;"
        "}"
        "QMessageBox QPushButton:hover { background-color: #106ebe; }"
        "QMessageBox QPushButton:pressed { background-color: #005a9e; }"
    );

    MainWindow window;
    window.setWindowTitle("Mini C++ IDE Simulator v2 — 教学级代码执行可视化调试器");
    window.resize(1300, 850);
    window.show();
    return app.exec();
}