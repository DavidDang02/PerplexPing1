// VIEW: the window shown while a session is running. Has the "I'm Lost"
// button, a question box, and End Session, and announces each as a signal.
// Gets the Session from the controller and words it itself (ViewText).

#pragma once

#include <QMainWindow>
#include <QString>

#include "error.h"
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
  void clearQuestion();                 // empties the question box and hides any error
  // Shows the red message under the question box for `error`, e.g.
  // "Type a question before sending." (M2 grading note: empty submit gave no cue.)
  void showQuestionError(Error error);
  // The empty box's hint says the question was sent.
  void showQuestionSent();

 signals:
  void lostClicked();
  void endClicked();
  void questionSubmitted(const QString& text);

 private:
  void submitQuestion();  // emits questionSubmitted with the text as typed

  Ui::SessionPage* ui;
};
