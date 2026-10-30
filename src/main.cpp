// Entry point for the app. This file must NEVER be listed in the test_build
// target in CMakeLists.txt — doctest generates its own main(), and having
// two main() functions will fail to link ("duplicate main").

#include <QApplication>

#include "loginpage.h"
#include "mainpage.h"
#include "sessioncontroller.h"
#include "sessionpage.h"

// Shows `to` in the same spot as `from`, then hides `from`.
static void switchPage(QWidget* from, QWidget* to) {
  to->move(from->pos());
  to->show();
  from->hide();
}

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  LoginPage loginPage;
  MainPage mainPage;
  SessionPage sessionPage;
  SessionController controller(&mainPage);

  QObject::connect(&mainPage, &MainPage::startClicked, &controller, &SessionController::createSession);
  QObject::connect(&sessionPage, &SessionPage::lostClicked, &controller, &SessionController::clickLostButton);
  QObject::connect(&sessionPage, &SessionPage::endClicked, &controller, &SessionController::endSession);

  // Page flow: login -> main -> session, and back.
  QObject::connect(&loginPage, &LoginPage::loginClicked, [&] { switchPage(&loginPage, &mainPage); });
  QObject::connect(&mainPage, &MainPage::logoutClicked, [&] {
    loginPage.clearFields();
    switchPage(&mainPage, &loginPage);
  });
  QObject::connect(&mainPage, &MainPage::startClicked, [&] {
    sessionPage.clearQuestion();
    switchPage(&mainPage, &sessionPage);
  });
  QObject::connect(&sessionPage, &SessionPage::endClicked, [&] { switchPage(&sessionPage, &mainPage); });

  loginPage.show();

  return app.exec();
}
