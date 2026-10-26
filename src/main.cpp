// Entry point for the app. This file must NEVER be listed in the test_build
// target in CMakeLists.txt — doctest generates its own main(), and having
// two main() functions will fail to link ("duplicate main").

#include <QApplication>

#include "mainpage.h"
#include "sessioncontroller.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  MainPage view;
  SessionController controller(&view);

  QObject::connect(&view, &MainPage::startClicked, &controller, &SessionController::createSession);
  QObject::connect(&view, &MainPage::lostClicked, &controller, &SessionController::clickLostButton);
  QObject::connect(&view, &MainPage::endClicked, &controller, &SessionController::endSession);

  view.show();
  
  return app.exec();
}