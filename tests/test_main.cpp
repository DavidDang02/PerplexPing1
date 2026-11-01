// This file exists solely to generate main() for the test binary.
// No other test file should ever contain this #define — doctest allows
// only one, and adding it to a second file causes a "duplicate main"
// link error.

/** #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN tells doctest to implement a main function for you which is great most of the time.
 *  Not so great if you want QSql to work. QSql requires a QCoreApplication object to be created first before a database can be created.
 *  QCoreApplication is a singelton pattern so there can only be one instance of it at a time.
* **/
#include "doctest.h"
#include <QCoreApplication>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    doctest::Context ctx;
    ctx.applyCommandLine(argc, argv);
    int res = ctx.run();
    return ctx.shouldExit() ? res : 0;
}
