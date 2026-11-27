// Entry point for the app. This file must NEVER be listed in a test target
// in CMakeLists.txt: tests/test_main.cpp has its own main(), and two main()
// functions fail to link ("duplicate main").
//
//   ./app                      connects to ws://localhost:8081
//   ./app --host 10.0.0.5      another computer running perplexing_server
//   ./app --port 9000

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QUrl>

#include "appcontroller.h"
#include "mainwindow.h"
#include "protocol.h"
#include "serverclient.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("PerplexPing");
    QApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("PerplexPing: tell your instructor when you're lost.");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption hostOption("host", "Server hostname or IP. Default: localhost.", "address", "localhost");
    QCommandLineOption portOption("port", QString("Server port. Default: %1.").arg(Protocol::kDefaultPort),
                                  "port", QString::number(Protocol::kDefaultPort));
    parser.addOption(hostOption);
    parser.addOption(portOption);
    parser.process(app);

    bool ok = false;
    const int port = parser.value(portOption).toInt(&ok);
    if (!ok || port < 1 || port > 65535) {
        QMessageBox::critical(nullptr, "PerplexPing",
                              "The port must be a number from 1 to 65535, not \"" +
                                  parser.value(portOption) + "\".");
        return 1;
    }
    QUrl url;
    url.setScheme("ws");
    url.setHost(parser.value(hostOption));
    url.setPort(port);

    // The app keeps running if the server isn't up yet: it shows
    // "Can't reach the server" and connects as soon as it can.
    ServerClient client(url);
    AppController controller(&client, nullptr);
    MainWindow window(&controller);
    window.show();
    client.start();

    return app.exec();
}
