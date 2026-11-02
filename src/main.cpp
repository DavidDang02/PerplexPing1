// Entry point for the app. This file must NEVER be listed in the test_build
// target in CMakeLists.txt — doctest generates its own main(), and having
// two main() functions will fail to link ("duplicate main").

#include <QApplication>
#include <QMessageBox>

#include "appcontroller.h"
#include "appmodel.h"
#include "database.h"
#include "loginpage.h"
#include "mainpage.h"
#include "sessionpage.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  // The database file sits next to the app, so every run sees the same data.
  Database db(QCoreApplication::applicationDirPath() + "/perplexping.db");
  if (!db.open()) {
    QMessageBox::critical(nullptr, "PerplexPing", "Could not open perplexping.db.");
    return 1;
  }

  LoginPage loginPage;
  MainPage mainPage;
  SessionPage sessionPage;
  AppModel model(&db);
  AppController controller(&model, &loginPage, &mainPage, &sessionPage);

  return app.exec();
}
