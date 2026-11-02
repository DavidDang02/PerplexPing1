// CONTROLLER: receives signals from the three pages, asks the model (AppModel)
// to do the work, then tells the pages what to show and which page is on
// screen. Has no widgets of its own and never touches the database.

#pragma once

#include <vector>

#include <QObject>
#include <QString>
#include <QWidget>

#include "session.h"

class AppModel;
class LoginPage;
class MainPage;
class SessionPage;

class AppController : public QObject {
  Q_OBJECT

 public:
  // None of the pointers are owned. Connects every page signal to a slot and
  // shows the login page.
  AppController(AppModel* model, LoginPage* loginPage, MainPage* mainPage,
                SessionPage* sessionPage, QObject* parent = nullptr);

 public slots:
  // Login page
  void logIn();
  void registerUser();

  // Main page
  void logOut();
  void startSession();
  void selectSession(int row);  // row in the previous-sessions list

  // Session page
  void clickLostButton();
  void submitQuestion(const QString& text);
  void endSession();

 private:
  void goTo(QWidget* page);  // shows `page` where the current page was, hides the rest
  void showMainPage();       // reloads the previous sessions from the model
  void showSessionDetails(const Session& session);
  void refreshSessionPage();

  AppModel* model;
  LoginPage* loginPage;
  MainPage* mainPage;
  SessionPage* sessionPage;
  QWidget* currentPage = nullptr;

  std::vector<Session> pastSessions;  // same order as the main page's list
};
