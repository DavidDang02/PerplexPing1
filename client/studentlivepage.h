// VIEW: a live session as a student (AppController::Page::StudentLive).
// The big I'm Lost button, a question box, and the student's own Lost
// presses and questions (the server sends only those), each of which they
// can delete, and questions they can edit, while the session is live.

#pragma once

#include "basepage.h"

class QListWidget;
class QPlainTextEdit;
class QPushButton;

class StudentLivePage : public BasePage {
    Q_OBJECT

public:
    explicit StudentLivePage(AppController* controller, QWidget* parent = nullptr);

    void refresh() override;
    bool showError(const QString& request, Error error) override;

private:
    void editSelected();
    void deleteSelected();
    void updateButtons();
    void onNotice(AppController::Notice notice);

    QLabel* titleLabel;
    QLabel* statusLabel;
    QPushButton* lostButton;
    QLabel* lostError;
    QPlainTextEdit* questionEdit;
    QLabel* questionError;
    QListWidget* itemList;
    QPushButton* editButton;
    QPushButton* deleteButton;
    QLabel* itemError;
};
