// Generates main() for the test binary. This is the only file that defines
// DOCTEST_CONFIG_IMPLEMENT; defining it in a second file causes a
// "duplicate main" link error.
//
// doctest's ready-made main (DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN) is not used
// because Qt SQL needs a QCoreApplication to exist before a database opens.
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <QCoreApplication>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    doctest::Context ctx;
    ctx.applyCommandLine(argc, argv);
    // Return doctest's result, so a failing test makes CI fail.
    return ctx.run();
}
