#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QWebSocketServer>

#include "database.h"
#include "socketserver.h"

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);

  // The central database lives on the server machine. Override with
  // `perplexing_server <db-path> [port]` if you want a different file/port.
  QString dbPath = QCoreApplication::applicationDirPath() + "/perplexping.db";
  int port = 8081;
  if (argc > 1) {
    dbPath = QString::fromLocal8Bit(argv[1]);
  }
  if (argc > 2) {
    port = QString::fromLocal8Bit(argv[2]).toInt();
  }

  Database db(dbPath);
  if (!db.open()) {
    qCritical() << "Database failed to open:" << dbPath;
    return 1;
  }

  SocketsServer server(port, &db, nullptr);
  if (!server.listen()) {
    qCritical() << "Failed to start server on port" << port << ":" << server.lastError();
    return 1;
  }

  qDebug() << "PerplexPing server listening on port" << server.port()
           << "with database" << dbPath;

  return app.exec();
}
