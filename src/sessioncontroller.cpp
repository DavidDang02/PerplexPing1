#include "sessioncontroller.h"

#include <QDateTime>
#include <QStringList>

#include "mainpage.h"

SessionController::SessionController(MainPage* view, QObject* parent)
    : QObject(parent), view(view) {
  refreshView();
}

SessionController::~SessionController() {
  delete session;
}

// ---- Controller: forward each click to the model, then refresh the view ----

void SessionController::createSession() {
  delete session;  // a new session replaces the old one
  session = new Session();
  refreshView();
}

void SessionController::clickButton() {
  if (session != nullptr) {
    session->buttonClicked();
  }
  refreshView();
}

void SessionController::endSession() {
  if (session != nullptr) {
    session->endSession();
  }
  refreshView();
}

// Formats seconds since the start as "mm:ss in", e.g. 754 -> "12:34 in".
static QString formatOffset(long long seconds) {
  return QString("%1:%2 in")
      .arg(seconds / 60, 2, 10, QChar('0'))
      .arg(seconds % 60, 2, 10, QChar('0'));
}

// Formats a real (wall clock) time as e.g. "2:47:05 PM".
static QString formatClockTime(Session::Clock::time_point time) {
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      time.time_since_epoch());
  return QDateTime::fromMSecsSinceEpoch(ms.count()).toString("h:mm:ss AP");
}

void SessionController::refreshView() {
  const bool running = session != nullptr && !session->isEnded();

  view->setRunning(running);

  if (session == nullptr) {
    view->setStatus("No session");
    view->setStartTime("Started: —");
    view->setEndTime("Ended: —");
    view->showPresses({});
    return;
  }

  view->setStartTime("Started: " + formatClockTime(session->getStartTime()));

  if (running) {
    view->setStatus(
        QString("Session running, %1 presses").arg(session->getPressCount()));
    view->setEndTime("Ended: —");
    view->showPresses({});
    return;
  }

  view->setStatus(
      QString("Session ended, %1 presses").arg(session->getPressCount()));
  view->setEndTime("Ended: " + formatClockTime(session->getEndTime()));

  // Each press as "mm:ss in" plus its wall clock time, e.g. "12:34 in (2:47:05 PM)".
  const std::vector<long long> offsets = session->getPressOffsetsSeconds();
  const std::vector<Session::Clock::time_point>& times = session->getButtonTimestamps();
  QStringList rows;
  for (size_t i = 0; i < offsets.size(); ++i) {
    rows.append(formatOffset(offsets[i]) + " (" + formatClockTime(times[i]) + ")");
  }
  view->showPresses(rows);
}