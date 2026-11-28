// VIEW: one course (AppController::Page::Course). Everyone sees its sessions
// (with a search box) and can open one. Students can't open a live session
// here; they join it with its session code on My Courses (CoursesPage).
// The course's educator also sees the
// course code, starts a session, renames or deletes sessions, and edits the
// course's settings (name, anonymous, questions shared) or deletes it.
// Which parts show is decided by getRole(); the server checks it again.

#pragma once

#include <string>
#include <tuple>

#include "basepage.h"

class QCheckBox;
class QGroupBox;
class QLineEdit;
class CardList;
class QPushButton;
class QTimer;

class CoursePage : public BasePage {
    Q_OBJECT

public:
    explicit CoursePage(AppController* controller, QWidget* parent = nullptr);

    void refresh() override;
    bool showError(const QString& request, Error error) override;

private:
    int selectedSessionId() const;  // 0 if none
    bool canOpenSelected() const;  // students can't open a live session here
    void openSelected();
    void copyCourseCode();
    void showCourseCode();  // the button's normal text
    void renameSelected();
    void deleteSelected();
    void fillSettings(const Course& course);
    void onNotice(AppController::Notice notice);

    int shownCourseId = 0;  // the settings boxes were last filled for this course
    // The course's name, anonymous, questions shared when the boxes were last
    // filled. refresh() fills them again only when the server's values change.
    std::tuple<std::string, bool, bool> shownSettings;

    QLabel* titleLabel;
    QWidget* codeRow;          // educator only
    QPushButton* codeButton;   // "Course code: K7M2QX"; a click copies the code
    QTimer* copiedTimer;       // puts the button's text back after "Copied!"

    QGroupBox* startBox;
    QLineEdit* sessionNameEdit;
    QLabel* startError;

    QLineEdit* searchEdit;
    CardList* sessionList;
    QLabel* emptyLabel;
    QPushButton* renameButton;
    QPushButton* deleteButton;
    QLabel* sessionError;

    QGroupBox* settingsBox;
    QLineEdit* courseNameEdit;
    QCheckBox* anonymousBox;
    QCheckBox* sharedBox;
    QLabel* settingsError;
};
