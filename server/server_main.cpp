#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QHostAddress>
#include <QHostInfo>

#include "database.h"
#include "perplexserver.h"

// Resolve a host string (an IP literal like "127.0.0.1"/"::1", a hostname like
// "localhost", or "0.0.0.0" for every interface) to a QHostAddress. An empty
// string means loopback. Returns a null QHostAddress if the string can't be
// resolved.
static QHostAddress resolveHost(const QString& text) {
  if (text.isEmpty()) {
    return QHostAddress::LocalHost;  // loopback, the default
  }
  // Try a literal IP first (v4 or v6).
  QHostAddress literal(text);
  if (!literal.isNull()) {
    return literal;
  }
  // Otherwise treat it as a hostname and resolve it.
  const QHostInfo info = QHostInfo::fromName(text);
  if (!info.addresses().isEmpty()) {
    return info.addresses().first();
  }
  return QHostAddress();  // null: unresolved
}

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("perplexing_server");
  QCoreApplication::setApplicationVersion("1.0");

  QCommandLineParser parser;
  parser.setApplicationDescription(
      "PerplexPing WebSocket server.\n"
      "Binds a WebSocket endpoint and serves the central SQLite database.");
  parser.addHelpOption();     // -h, --help
  parser.addVersionOption();  // -v, --version

  QCommandLineOption hostOption(
      "host",
      "Host address to bind on (IP literal or hostname; use 0.0.0.0 for all "
      "interfaces). Default: localhost (loopback only).",
      "address");
  QCommandLineOption portOption(
      "port", "TCP port to listen on (1-65535). Default: 8081.", "port");
  QCommandLineOption dbOption(
      "db",
      "Path to the SQLite database file. Default: perplexping.db next to the "
      "executable.",
      "path");
  parser.addOption(hostOption);
  parser.addOption(portOption);
  parser.addOption(dbOption);

  parser.process(app);

  // ---- Port ----
  int port = 8081;
  if (parser.isSet(portOption)) {
    bool ok = false;
    port = parser.value(portOption).toInt(&ok);
    if (!ok || port < 1 || port > 65535) {
      qCritical() << "Invalid port:" << parser.value(portOption)
                  << "- must be an integer from 1 to 65535.";
      return 1;
    }
  }

  // ---- Host ----
  const QString hostText = parser.value(hostOption);
  const QHostAddress host = resolveHost(hostText);
  if (host.isNull()) {
    qCritical() << "Invalid or unresolvable host:" << hostText;
    return 1;
  }

  // ---- Database ----
  QString dbPath = QCoreApplication::applicationDirPath() + "/perplexping.db";
  if (parser.isSet(dbOption)) {
    dbPath = parser.value(dbOption);
  }

  Database db(dbPath);
  Result<bool> opened = db.open();
  if (!opened.isOk()) {
    qCritical() << "Database failed to open:" << dbPath;
    return 1;
  }

  PerplexServer server(port, &db, nullptr, host);
  if (!server.listen()) {
    qCritical() << "Failed to start server on" << host.toString() << ":"
                << port << ":" << server.lastError();
    return 1;
  }

  qDebug() << "PerplexPing server listening on" << host.toString() << ":"
           << server.port() << "with database" << dbPath;

  return app.exec();
}
