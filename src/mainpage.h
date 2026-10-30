// VIEW: the main window after logging in. Lists previous sessions, shows the
// details of the selected one, and announces button clicks as signals.
// Knows nothing about Session.

#pragma once

#include <QMainWindow>
#include <QStringList>

namespace Ui {
class MainPage;
}

class MainPage : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainPage(QWidget* parent = nullptr);
  ~MainPage();

  // Display methods the controller calls. The time and press methods fill
  // the "Session details" panel.
  void setStatus(const QString& text);
  void setStartTime(const QString& text);
  void setEndTime(const QString& text);
  void setRunning(bool running);              // enables/disables Start and Log Out
  void showPresses(const QStringList& rows);  // fills timestampList
  void showSessions(const QStringList& rows); // fills sessionList

 signals:
  void startClicked();
  void logoutClicked();
  void sessionSelected(int index);  // row in sessionList that was clicked

 private:
  Ui::MainPage* ui;
};
