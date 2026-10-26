// VIEW: the main window. Shows what the controller tells it and announces
// button clicks as signals. Knows nothing about Session.

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

  // Display methods the controller calls.
  void setStatus(const QString& text);
  void setStartTime(const QString& text);
  void setEndTime(const QString& text);
  void setRunning(bool running);              // enables/disables the 3 buttons
  void showPresses(const QStringList& rows);  // fills timestampList

 signals:
  void startClicked();
  void lostClicked();
  void endClicked();

 private:
  Ui::MainPage* ui;
};