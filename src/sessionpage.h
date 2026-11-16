// VIEW: the window shown while a session is running. Has the "I'm Lost"
// button, a question box, and End Session, and announces each as a signal.
// Gets the Session from the controller and words it itself (ViewText).

#pragma once

#include <QMainWindow>
#include <QString>

#include "session.h"

namespace Ui {
class SessionPage;
}

class SessionPage : public QMainWindow {
  Q_OBJECT

 public:
  explicit SessionPage(QWidget* parent = nullptr);
  ~SessionPage();

  void showSession(const Session& session);  // the live Lost / question counts
  void clearQuestion();                 // empties the question box

 signals:
  void lostClicked();
  void endClicked();
  void questionSubmitted(const QString& text);

 private:
  void submitQuestion();  // emits questionSubmitted, then clears the box

  Ui::SessionPage* ui;
};
