// CONTROLLER: receives clicks from the view, calls the Session model, then
// tells the view what to show. Has no widgets of its own.

#pragma once

#include <QObject>

#include "session.h"

class MainPage;

class SessionController : public QObject {
  Q_OBJECT

 public:
  explicit SessionController(MainPage* view, QObject* parent = nullptr);
  ~SessionController();

 public slots:
  void createSession();  // "Start Session"
  void clickButton();    // "I'm lost"
  void endSession();     // "End Session"

 private:
  void refreshView();  // reads Session, formats text, calls view display methods

  MainPage* view;              // not owned
  Session* session = nullptr;  // owned; null until the first Start
};