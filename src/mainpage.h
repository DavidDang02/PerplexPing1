// CONTROLLER: the main page. Its slots receive button clicks from the view
// (mainpage.ui), forward them to the Session model, then ask the view to
// refresh. All session logic lives in Session; this class only connects the
// two. (Qt's ui-> pattern needs the controller to be the window itself.)

#pragma once

#include <QMainWindow>

#include "session.h"

namespace Ui {
class MainPage;
}

class MainPage : public QMainWindow {
    Q_OBJECT

public:
    explicit MainPage(QWidget *parent = nullptr);
    ~MainPage();

private slots:
    void createSession();  // "Start Session" button
    void clickButton();    // "I'm lost" button
    void endSession();     // "End Session" button

private:
    // VIEW update: copies what the model says onto the widgets.
    void refreshView();

    Ui::MainPage *ui;             // VIEW: widgets designed in mainpage.ui
    Session *session = nullptr;   // MODEL: null until the first Start
};
