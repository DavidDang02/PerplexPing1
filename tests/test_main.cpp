// Generates main() for the test binary. This is the only file that defines
// DOCTEST_CONFIG_IMPLEMENT; defining it in a second file causes a
// "duplicate main" link error.
//
// doctest's ready-made main (DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN) is not used
// because Qt SQL needs a QCoreApplication to exist before a database opens.
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <QCoreApplication>
#include <QSqlDatabase>

int main(int argc, char** argv) {
    QCoreApplication::addLibraryPath("/usr/lib64/qt6/plugins");
    QCoreApplication app(argc, argv);

    // debug: print library paths to see if Qt finds the plugin
    std::cerr << "LIBPATHS: ";
    for (const QString& p : QCoreApplication::libraryPaths()) {
        std::cerr << p.toStdString() << " ";
    }
    std::cerr << "\n";

    std::cerr << "DRIVERS: ";
    for (const QString& d : QSqlDatabase::drivers()) {
        std::cerr << d.toStdString() << " ";
    }
    std::cerr << "\n";

    doctest::Context ctx;
    ctx.applyCommandLine(argc, argv);
    // Return doctest's result, so a failing test makes CI fail.
    return ctx.run();
}
