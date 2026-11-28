// VIEW: My Courses (AppController::Page::Courses), shown right after logging
// in. Lists the user's courses (as educator or student), opens one, joins a
// course by its course code, joins a live session by its session code,
// and creates a course. Account Management and Log Out are in the NavBar's
// user menu.

#pragma once

#include "basepage.h"

class QCheckBox;
class QLineEdit;
class CardList;
class QPushButton;

class CoursesPage : public BasePage {
    Q_OBJECT

public:
    explicit CoursesPage(AppController* controller, QWidget* parent = nullptr);

    void refresh() override;
    bool showError(const QString& request, Error error) override;

private:
    void openSelected();
    void onNotice(AppController::Notice notice);

    CardList* courseList;
    QLabel* emptyLabel;

    QLineEdit* courseCodeEdit;
    QLabel* joinCourseError;
    QLineEdit* sessionCodeEdit;
    QLabel* joinSessionError;

    QLineEdit* newCourseEdit;
    QCheckBox* anonymousBox;
    QCheckBox* sharedBox;
    QLabel* createError;

};
