// Entry point for the app. This file must NEVER be listed in the test_build
// target in CMakeLists.txt — tests/test_main.cpp has its own main(), and
// having two main() functions will fail to link ("duplicate main").

#include <QApplication>
#include <QMessageBox>

#include "appcontroller.h"
#include "appmodel.h"
#include "database.h"
#include "loginpage.h"
#include "mainpage.h"
#include "serverclient.h"
#include "sessionpage.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  // The client talks to the central server over WebSocket. If the server is
  // not reachable, the app shows a generic connection error and exits.
  ServerClient client("ws://localhost:8081");
  if (!client.isConnected()) {
    QMessageBox::critical(nullptr, "PerplexPing",
                          "Could not connect to the PerplexPing server.");
    return 1;
  }

  LoginPage loginPage;
  MainPage mainPage;
  SessionPage sessionPage;
  AppModel model(&client);
  AppController controller(&model, &loginPage, &mainPage, &sessionPage);

  return app.exec();
}