// VIEW: the main window after logging in. Lists previous sessions, shows the
// details of the selected one, and announces button clicks as signals.
// Gets model objects from the controller and words them itself (ViewText).

#pragma once

#include <QMainWindow>
#include <vector>

#include "session.h"
#include "user.h"

namespace Ui {
class MainPage;
}

class MainPage : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainPage(QWidget* parent = nullptr);
  ~MainPage();

  // Display methods the controller calls.
  void showUser(const User& user);                     // "Logged in as ana"
  void showSessions(const std::vector<Session>& sessions);  // fills sessionList, newest first
  void selectSessionRow(int row);                      // highlights that row in sessionList
  // Fills the "Session details" panel: start, end, and every input with its
  // time into the session.
  void showSessionDetails(const Session& session);
  void clearSessionDetails();                          // blanks, when there are no sessions

 signals:
  void startClicked();
  void logoutClicked();
  void sessionSelected(int index);  // row in sessionList that was clicked

 private:
  Ui::MainPage* ui;
};
