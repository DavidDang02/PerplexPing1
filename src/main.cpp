// Entry point for the app. This file must NEVER be listed in the test_build
// target in CMakeLists.txt — tests/test_main.cpp has its own main(), and
// having two main() functions will fail to link ("duplicate main").

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
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
  QApplication::setApplicationName("perplexping");
  QApplication::setApplicationVersion("1.0");

  QCommandLineParser parser;
  parser.setApplicationDescription(
      "PerplexPing client — talks to the central server over WebSocket.");
  parser.addHelpOption();     // -h, --help
  parser.addVersionOption();  // -v, --version

  QCommandLineOption hostOption(
      "host",
      "Server hostname or IP to connect to (e.g. [IP_ADDRESS] or localhost). "
      "Default: localhost.",
      "address");
  QCommandLineOption portOption(
      "port", "Server WebSocket port (1-65535). Default: 8081.", "port");
  QCommandLineOption timeoutOption(
      "connect-timeout",
      "Milliseconds to wait for the WebSocket connection (1-30000). "
      "Default: 5000.",
      "ms");
  parser.addOption(hostOption);
  parser.addOption(portOption);
  parser.addOption(timeoutOption);

  parser.process(app);

  // ---- Port ----
  int port = 8081;
  if (parser.isSet(portOption)) {
    bool ok = false;
    port = parser.value(portOption).toInt(&ok);
    if (!ok || port < 1 || port > 65535) {
      QMessageBox::critical(nullptr, "PerplexPing",
                            "Invalid port: " + parser.value(portOption) +
                            " — must be an integer from 1 to 65535.");
      return 1;
    }
  }

  // ---- Host ----
  const QString host = parser.isSet(hostOption)
                           ? parser.value(hostOption)
                           : "localhost";

  // ---- Connect timeout ----
  int connectTimeoutMs = 5000;
  if (parser.isSet(timeoutOption)) {
    bool ok = false;
    connectTimeoutMs = parser.value(timeoutOption).toInt(&ok);
    if (!ok || connectTimeoutMs < 1 || connectTimeoutMs > 30000) {
      QMessageBox::critical(nullptr, "PerplexPing",
                            "Invalid connect-timeout: " +
                            parser.value(timeoutOption) +
                            " — must be an integer from 1 to 30000.");
      return 1;
    }
  }

  const QString url = "ws://" + host + ":" + QString::number(port);

  // The client talks to the central server over WebSocket. If the server is
  // not reachable, the app shows a generic connection error and exits.
  ServerClient client(url, connectTimeoutMs);
  if (!client.isConnected()) {
    QMessageBox::critical(nullptr, "PerplexPing",
                          "Could not connect to the PerplexPing server at " +
                          url + ".");
    return 1;
  }

  LoginPage loginPage;
  MainPage mainPage;
  SessionPage sessionPage;
  AppModel model(&client);
  AppController controller(&model, &loginPage, &mainPage, &sessionPage);

  return app.exec();
}