// VIEW: an ended session (AppController::Page::SessionViewer). For the
// educator it is the review page (in review) and stays editable after
// publishing; for students it opens once published.
//
// Left: the confusion timeline (click a bar to play that moment) and the
// video player. The educator also gets the Recording card: Find recording
// from the course's linked Panopto folder (a one-time setup card links it),
// or paste a link + "session starts at"; and Publish.
// Right: the questions with their answers (search box + "Unanswered only"),
// clicking a question plays its moment, and an answer box. Who may answer,
// edit or delete is decided by the server; the page offers the buttons
// that make sense for getRole() and the user's own items.

#pragma once

#include "basepage.h"

#include <QUrl>

class QCheckBox;
class QGroupBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class TimelineWidget;
class VideoPlayer;

class SessionViewerPage : public BasePage {
    Q_OBJECT

public:
    explicit SessionViewerPage(AppController* controller, QWidget* parent = nullptr);

    void refresh() override;
    bool showError(const QString& request, Error error) override;
    void play(const QUrl& url);  // AppController::playRecording

private:
    void refreshRecordingCard(const Session& session, bool educator);
    void refreshQuestions(const Session& session, bool educator);
    void updateButtons();
    int selectedQuestionId() const;  // the question, or the question of the selected answer
    int selectedAnswerId() const;    // 0 unless an answer row is selected
    void onItemClicked(QTreeWidgetItem* item);
    void editAnswer();
    void deleteAnswer();
    void deleteQuestion();
    void onNotice(AppController::Notice notice);

    int shownSessionId = 0;

    QLabel* titleLabel;
    QLabel* statusLabel;
    QPushButton* publishButton;
    QLabel* publishError;

    TimelineWidget* timeline;
    QLabel* peakLabel;
    VideoPlayer* player;

    // Recording card (educator).
    QGroupBox* recordingBox;
    QLabel* recordingLabel;
    QPushButton* removeRecordingButton;
    QGroupBox* setupBox;  // shown while the course has no Panopto folder
    QLineEdit* lectureLinkEdit;
    QLabel* folderLabel;
    QPushButton* findButton;
    QLineEdit* urlEdit;
    QLineEdit* startsAtEdit;
    QLabel* recordingError;

    // Questions.
    QLineEdit* searchEdit;
    QCheckBox* unansweredBox;
    QTreeWidget* questionTree;
    QLabel* emptyLabel;
    QPushButton* deleteQuestionButton;
    QPushButton* editAnswerButton;
    QPushButton* deleteAnswerButton;
    QPlainTextEdit* answerEdit;
    QPushButton* answerButton;
    QLabel* questionError;
};
