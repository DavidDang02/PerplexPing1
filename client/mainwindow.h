// VIEW: the app's one window. Shows the navigation bar (NavBar) across the
// top after login, and the page AppController asks for
// (pageChanged) in a stack, a connection banner when the server can't be
// reached, and each notice or error in the status line.
//
// Each page is its own widget (LoginPage, then the BasePage subclasses).
// On pageChanged and stateChanged the current page redraws itself from the
// controller's getters; a failed request is shown next to its box on the
// current page, or in the status line if the page has no box for it.

#pragma once

#include <QHash>
#include <QLabel>
#include <QMainWindow>
#include <QStackedWidget>
#include <QTimer>

#include "appcontroller.h"

class BasePage;
class LoginPage;
class NavBar;
class SessionViewerPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(AppController* controller, QWidget* parent = nullptr);

private:
    void showPage(AppController::Page page);
    BasePage* currentPage() const;  // nullptr on the login page

    AppController* controller;
    NavBar* navBar;   // hidden on the login page
    QStackedWidget* stack;
    QLabel* banner;   // "Can't reach the server" while disconnected
    QTimer* bannerDelay;  // the banner waits this long, so startup and blips don't flash it
    QLabel* status;   // the last notice or error
    LoginPage* loginPage;
    SessionViewerPage* viewerPage;
    QHash<AppController::Page, QWidget*> pages;
};
