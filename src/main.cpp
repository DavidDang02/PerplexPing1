// Entry point for the app. This file must NEVER be listed in the test_build
// target in CMakeLists.txt — doctest generates its own main(), and having
// two main() functions will fail to link ("duplicate main").

#include <QApplication>
#include <QLabel>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QLabel label("Hello, Qt!");
    label.show();

    return app.exec();
}
