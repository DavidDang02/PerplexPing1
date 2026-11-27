// VIEW: the app's one window. Shows the page AppController asks for
// (pageChanged) in a stack, a connection banner when the server can't be
// reached, and each notice or error in the status line.
//
// Each page is its own widget that plugs in here. Until a page
// exists, a placeholder names it so the app still runs end to end.

#pragma once

#include <QHash>
#include <QLabel>
#include <QMainWindow>
#include <QStackedWidget>

#include "appcontroller.h"

class LoginPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(AppController* controller, QWidget* parent = nullptr);

private:
    void showPage(AppController::Page page);
    QWidget* makePlaceholder(const QString& name);

    AppController* controller;
    QStackedWidget* stack;
    QLabel* banner;   // "Can't reach the server" while disconnected
    QLabel* status;   // the last notice or error
    LoginPage* loginPage;
    QHash<AppController::Page, QWidget*> pages;
};
