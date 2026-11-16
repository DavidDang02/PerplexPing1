#include "appcontroller.h"

#include <QCoreApplication>

#include "appmodel.h"
#include "error.h"
#include "loginpage.h"
#include "mainpage.h"
#include "sessionpage.h"

// ---- Setup ----

AppController::AppController(AppModel* model, LoginPage* loginPage, MainPage* mainPage,
                             SessionPage* sessionPage, QObject* parent)
    : QObject(parent),
      model(model),
      loginPage(loginPage),
      mainPage(mainPage),
      sessionPage(sessionPage) {
  connect(loginPage, &LoginPage::loginClicked, this, &AppController::logIn);
  connect(loginPage, &LoginPage::registerClicked, this, &AppController::registerUser);

  connect(mainPage, &MainPage::logoutClicked, this, &AppController::logOut);
  connect(mainPage, &MainPage::startClicked, this, &AppController::startSession);
  connect(mainPage, &MainPage::sessionSelected, this, &AppController::selectSession);

  connect(sessionPage, &SessionPage::lostClicked, this, &AppController::clickLostButton);
  connect(sessionPage, &SessionPage::questionSubmitted, this, &AppController::submitQuestion);
  connect(sessionPage, &SessionPage::endClicked, this, &AppController::endSession);

  // If the app closes while a session is running, end and save it, so it
  // doesn't stay "in progress" forever.
  connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { this->model->endSession(); });

  goTo(loginPage);
}

void AppController::goTo(QWidget* page) {
  if (currentPage != nullptr && currentPage != page) {
    page->move(currentPage->pos());
    currentPage->hide();
  }
  page->show();
  currentPage = page;
}

// ---- Login page ----

void AppController::logIn() {
  const Result<User> result = model->logIn(loginPage->username().toStdString(),
                                           loginPage->password().toStdString());
  if (!result.isOk()) {
    loginPage->showError(result.error);
    return;
  }
  loginPage->clearFields();
  showMainPage();
}

void AppController::registerUser() {
  const Result<User> result = model->registerUser(loginPage->username().toStdString(),
                                                  loginPage->password().toStdString());
  if (!result.isOk()) {
    loginPage->showError(result.error);
    return;
  }
  loginPage->clearFields();
  showMainPage();
}

// ---- Main page ----

void AppController::logOut() {
  model->logOut();
  pastSessions.clear();
  goTo(loginPage);
}

void AppController::startSession() {
  if (!model->startSession().isOk()) {
    return;
  }
  sessionPage->clearQuestion();
  refreshSessionPage();
  goTo(sessionPage);
}

void AppController::selectSession(int row) {
  if (row < 0 || row >= static_cast<int>(pastSessions.size())) {
    return;
  }
  showSessionDetails(pastSessions[row]);
}

void AppController::showMainPage() {
  const Result<std::vector<Session>> sessionsResult = model->getPastSessions();
  pastSessions = sessionsResult.isOk() ? sessionsResult.value : std::vector<Session>{};

  mainPage->showSessions(pastSessions);
  mainPage->showUser(*model->getCurrentUser());

  // Highlight the newest session and show its details, or blanks if there
  // are none.
  if (pastSessions.empty()) {
    mainPage->clearSessionDetails();
  } else {
    mainPage->selectSessionRow(0);
    showSessionDetails(pastSessions.front());
  }
  goTo(mainPage);
}

void AppController::showSessionDetails(const Session& session) {
  mainPage->showSessionDetails(session);
}

// ---- Session page ----
// The model applies the rules and saves; the controller just refreshes the page.

void AppController::clickLostButton() {
  model->clickLostButton();
  refreshSessionPage();
}

void AppController::submitQuestion(const QString& text) {
  model->askQuestion(text.toStdString());
  refreshSessionPage();
}

void AppController::endSession() {
  model->endSession();
  showMainPage();  // the session just ended is now first in the list
}

void AppController::refreshSessionPage() {
  const Session* session = model->getCurrentSession();
  if (session == nullptr) {
    return;
  }
  sessionPage->showSession(*session);
}
