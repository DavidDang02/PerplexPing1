#include "appcontroller.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QStringList>

#include "appmodel.h"
#include "loginpage.h"
#include "mainpage.h"
#include "sessionpage.h"

// ---- Formatting helpers (text for the view; the model never sees QString) ----

static QString formatClockTime(Session::Clock::time_point time) {
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      time.time_since_epoch());
  return QDateTime::fromMSecsSinceEpoch(ms.count()).toString("MMM d, h:mm AP");
}

// "50 min", or "<1 min" for a very short session. A session can be missing its
// end time if the app was killed instead of quit (Qt Creator's Stop, a crash).
static QString formatDuration(const Session& session) {
  if (!session.isEnded()) {
    return "no end time";
  }
  const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(
      session.getEndTime() - session.getStartTime());
  if (minutes.count() < 1) {
    return "<1 min";
  }
  return QString("%1 min").arg(minutes.count());
}

// 754 -> "12:34 in"
static QString formatOffset(long long seconds) {
  return QString("%1:%2 in")
      .arg(seconds / 60, 2, 10, QChar('0'))
      .arg(seconds % 60, 2, 10, QChar('0'));
}

// "12:34 in · I'm lost" or "12:34 in · Q: what is a foreign key?"
static QString formatInput(const Session& session, const SessionInput& input) {
  const QString when = formatOffset(session.getOffsetSeconds(input));
  if (input.getType() == InputType::Question) {
    return when + " · Q: " + QString::fromStdString(input.getQuestionText());
  }
  return when + " · I'm lost";
}

// The message the login page shows for each reason AppModel gives.
static QString authMessage(AuthError error) {
  switch (error) {
    case AuthError::EmptyName:     return "Enter a username.";
    case AuthError::EmptyPassword: return "Enter a password.";
    case AuthError::NameTaken:     return "That username is already taken.";
    case AuthError::WrongLogin:    return "Wrong username or password.";
    case AuthError::None:          break;
  }
  return "";
}

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
  const AuthError error = model->logIn(loginPage->username().toStdString(),
                                       loginPage->password().toStdString());
  if (error != AuthError::None) {
    loginPage->showError(authMessage(error));
    return;
  }
  loginPage->clearFields();
  showMainPage();
}

void AppController::registerUser() {
  const AuthError error = model->registerUser(loginPage->username().toStdString(),
                                              loginPage->password().toStdString());
  if (error != AuthError::None) {
    loginPage->showError(authMessage(error));
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
  if (!model->startSession()) {
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
  pastSessions = model->getPastSessions();

  QStringList rows;
  for (const Session& session : pastSessions) {
    rows.append(QString("%1  ·  %2  ·  Lost: %3  ·  Questions: %4")
                    .arg(formatClockTime(session.getStartTime()))
                    .arg(formatDuration(session))
                    .arg(session.getPressCount())
                    .arg(session.getQuestionCount()));
  }
  mainPage->showSessions(rows);
  mainPage->setStatus("Logged in as " + QString::fromStdString(model->getCurrentUser()->getName()));

  // Highlight the newest session and show its details, or blanks if there
  // are none.
  if (pastSessions.empty()) {
    mainPage->setStartTime("Started: —");
    mainPage->setEndTime("Ended: —");
    mainPage->showPresses({});
  } else {
    mainPage->selectSessionRow(0);
    showSessionDetails(pastSessions.front());
  }
  mainPage->setRunning(false);
  goTo(mainPage);
}

void AppController::showSessionDetails(const Session& session) {
  mainPage->setStartTime("Started: " + formatClockTime(session.getStartTime()));
  mainPage->setEndTime(session.isEnded()
                           ? "Ended: " + formatClockTime(session.getEndTime())
                           : QString("Ended: —"));
  QStringList rows;
  for (const SessionInput& input : session.getInputs()) {
    rows.append(formatInput(session, input));
  }
  mainPage->showPresses(rows);
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
  sessionPage->setStatus(QString("Session in progress  ·  Lost: %1  ·  Questions: %2")
                             .arg(session->getPressCount())
                             .arg(session->getQuestionCount()));
}
