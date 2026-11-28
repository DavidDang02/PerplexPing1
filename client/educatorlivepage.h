// VIEW: a live session as its educator (AppController::Page::EducatorLive).
// Read-only while it runs: the session code to share, the counts, the
// timeline so far, and who is connected. End Session (asks first) moves it
// to review.

#pragma once

#include "basepage.h"

class QListWidget;
class TimelineWidget;

class EducatorLivePage : public BasePage {
    Q_OBJECT

public:
    explicit EducatorLivePage(AppController* controller, QWidget* parent = nullptr);

    void refresh() override;
    bool showError(const QString& request, Error error) override;

private:
    void refreshParticipants();

    QLabel* titleLabel;
    QLabel* codeLabel;
    QLabel* statusLabel;
    TimelineWidget* timeline;
    QLabel* participantsLabel;
    QListWidget* participantList;
    QLabel* endError;
};
