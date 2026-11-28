#include "sessionviewerpage.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "protocol.h"
#include "timelinewidget.h"
#include "videoplayer.h"
#include "viewtext.h"

namespace Request = Protocol::Request;

namespace {
// Tree rows keep their ids in these roles. A question row has kAnswerRole 0.
constexpr int kQuestionRole = Qt::UserRole;
constexpr int kAnswerRole = Qt::UserRole + 1;
}  // namespace

SessionViewerPage::SessionViewerPage(AppController* controller, QWidget* parent)
    : BasePage(controller, parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);

    // ---- Top: status, Publish (back is in the NavBar), then the title ----
    auto* top = new QHBoxLayout;
    top->addStretch();
    statusLabel = new QLabel(this);
    top->addWidget(statusLabel);
    publishButton = new QPushButton("Publish to Students", this);
    publishButton->setStyleSheet("font-weight:700;");
    top->addWidget(publishButton);
    layout->addLayout(top);
    titleLabel = makeHeading("");
    layout->addWidget(titleLabel);
    publishError = makeErrorLabel();
    layout->addWidget(publishError);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(splitter, 1);

    // ================= Left: timeline, video, recording =================
    auto* leftScroll = new QScrollArea(splitter);
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    auto* left = new QWidget(leftScroll);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 8, 0);

    leftLayout->addWidget(makeHeading("Confusion timeline", UiFont::kSubheading));
    timeline = new TimelineWidget(left);
    leftLayout->addWidget(timeline);
    peakLabel = new QLabel(left);
    peakLabel->setWordWrap(true);
    leftLayout->addWidget(peakLabel);

    player = new VideoPlayer(left);
    leftLayout->addWidget(player, 1);

    // ---- Recording card (educator) ----
    recordingBox = new QGroupBox("Recording", left);
    auto* recordingLayout = new QVBoxLayout(recordingBox);
    auto* currentRow = new QHBoxLayout;
    recordingLabel = new QLabel(recordingBox);
    recordingLabel->setWordWrap(true);
    removeRecordingButton = new QPushButton("Remove…", recordingBox);
    currentRow->addWidget(recordingLabel, 1);
    currentRow->addWidget(removeRecordingButton);
    recordingLayout->addLayout(currentRow);

    // Setup card: link the course's Panopto folder once.
    setupBox = new QGroupBox("Set up Find recording (once per course)", recordingBox);
    auto* setupLayout = new QVBoxLayout(setupBox);
    auto* step1 = new QLabel("1. Sign in to Panopto: this opens by itself when you press Link.", setupBox);
    step1->setWordWrap(true);
    setupLayout->addWidget(step1);
    auto* step2 = new QLabel("2. Paste any lecture link from this course's Panopto:", setupBox);
    step2->setWordWrap(true);
    setupLayout->addWidget(step2);
    auto* linkRow = new QHBoxLayout;
    lectureLinkEdit = new QLineEdit(setupBox);
    lectureLinkEdit->setPlaceholderText("https://scu.hosted.panopto.com/Panopto/Pages/Viewer.aspx?id=…");
    auto* linkButton = new QPushButton("Link", setupBox);
    linkRow->addWidget(lectureLinkEdit);
    linkRow->addWidget(linkButton);
    setupLayout->addLayout(linkRow);
    recordingLayout->addWidget(setupBox);

    auto* findRow = new QHBoxLayout;
    folderLabel = new QLabel(recordingBox);
    folderLabel->setWordWrap(true);
    findButton = new QPushButton("Find Recording", recordingBox);
    findRow->addWidget(folderLabel, 1);
    findRow->addWidget(findButton);
    recordingLayout->addLayout(findRow);

    // Fallback: paste the link and where the session starts in it.
    auto* manualLabel = new QLabel("Or paste the recording link yourself:", recordingBox);
    recordingLayout->addWidget(manualLabel);
    auto* manualForm = new QFormLayout;
    urlEdit = new QLineEdit(recordingBox);
    urlEdit->setPlaceholderText("https://…");
    startsAtEdit = new QLineEdit(recordingBox);
    startsAtEdit->setPlaceholderText("e.g. 4:30 (blank = 0:00)");
    manualForm->addRow("Link", urlEdit);
    manualForm->addRow("Session starts at", startsAtEdit);
    recordingLayout->addLayout(manualForm);
    auto* attachButton = new QPushButton("Attach", recordingBox);
    recordingLayout->addWidget(attachButton, 0, Qt::AlignRight);
    recordingError = makeErrorLabel();
    recordingLayout->addWidget(recordingError);
    leftLayout->addWidget(recordingBox);

    leftScroll->setWidget(left);
    splitter->addWidget(leftScroll);

    // ================= Right: questions and answers =================
    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(8, 0, 0, 0);
    rightLayout->addWidget(makeHeading("Questions", UiFont::kSubheading));
    auto* filterRow = new QHBoxLayout;
    searchEdit = new QLineEdit(right);
    searchEdit->setPlaceholderText("Search questions and answers…");
    searchEdit->setClearButtonEnabled(true);
    unansweredBox = new QCheckBox("Unanswered only", right);
    filterRow->addWidget(searchEdit, 1);
    filterRow->addWidget(unansweredBox);
    rightLayout->addLayout(filterRow);

    questionTree = new QTreeWidget(right);
    questionTree->setHeaderHidden(true);
    questionTree->setWordWrap(true);
    questionTree->setUniformRowHeights(false);
    questionTree->setTextElideMode(Qt::ElideNone);
    questionTree->header()->setStretchLastSection(true);
    rightLayout->addWidget(questionTree, 1);
    emptyLabel = new QLabel(right);
    emptyLabel->setWordWrap(true);
    rightLayout->addWidget(emptyLabel);

    auto* itemButtons = new QHBoxLayout;
    deleteQuestionButton = new QPushButton("Remove Question…", right);
    editAnswerButton = new QPushButton("Edit Answer…", right);
    deleteAnswerButton = new QPushButton("Delete Answer…", right);
    itemButtons->addWidget(deleteQuestionButton);
    itemButtons->addStretch();
    itemButtons->addWidget(editAnswerButton);
    itemButtons->addWidget(deleteAnswerButton);
    rightLayout->addLayout(itemButtons);

    answerEdit = new QPlainTextEdit(right);
    answerEdit->setPlaceholderText("Pick a question, then type an answer…");
    answerEdit->setMaximumHeight(80);
    rightLayout->addWidget(answerEdit);
    answerButton = new QPushButton("Post Answer", right);
    rightLayout->addWidget(answerButton, 0, Qt::AlignRight);
    questionError = makeErrorLabel();
    rightLayout->addWidget(questionError);

    splitter->addWidget(right);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    // ---- Wiring ----
    connect(publishButton, &QPushButton::clicked, this, [this] {
        if (confirm("Publish session",
                    "Publish this session? Students will see the timeline and answers. "
                    "It can't be un-published.")) {
            clearError(publishError);
            this->controller->publishSession();
        }
    });

    connect(timeline, &TimelineWidget::barClicked, controller, &AppController::jumpTo);

    connect(linkButton, &QPushButton::clicked, this, [this] {
        clearError(recordingError);
        this->controller->linkPanoptoFolder(lectureLinkEdit->text());
    });
    connect(findButton, &QPushButton::clicked, this, [this] {
        clearError(recordingError);
        this->controller->findRecording();
    });
    connect(attachButton, &QPushButton::clicked, this, [this] {
        clearError(recordingError);
        this->controller->attachRecording(urlEdit->text(), startsAtEdit->text());
    });
    connect(removeRecordingButton, &QPushButton::clicked, this, [this] {
        if (confirm("Remove recording", "Remove the recording from this session?")) {
            clearError(recordingError);
            this->controller->removeRecording();
        }
    });

    auto search = [this] {
        this->controller->setQuestionSearch(searchEdit->text(), unansweredBox->isChecked());
    };
    connect(searchEdit, &QLineEdit::textChanged, this, search);
    connect(unansweredBox, &QCheckBox::toggled, this, search);

    connect(questionTree, &QTreeWidget::currentItemChanged, this, &SessionViewerPage::updateButtons);
    connect(questionTree, &QTreeWidget::itemClicked, this, &SessionViewerPage::onItemClicked);
    connect(deleteQuestionButton, &QPushButton::clicked, this, &SessionViewerPage::deleteQuestion);
    connect(editAnswerButton, &QPushButton::clicked, this, &SessionViewerPage::editAnswer);
    connect(deleteAnswerButton, &QPushButton::clicked, this, &SessionViewerPage::deleteAnswer);
    connect(answerButton, &QPushButton::clicked, this, [this] {
        if (const int questionId = selectedQuestionId()) {
            clearError(questionError);
            this->controller->addAnswer(questionId, answerEdit->toPlainText());
        }
    });

    connect(controller, &AppController::notice, this, &SessionViewerPage::onNotice);

    UiFont::styleSectionTitles(this);  // box titles as sub-headings
}

void SessionViewerPage::play(const QUrl& url) {
    player->play(url);
}

void SessionViewerPage::refresh() {
    const auto& session = controller->getOpenSession();
    if (!session) {
        return;
    }
    const bool educator = controller->getRole() == CourseRole::Educator;

    // Another session: reset the boxes and the player (the controller already
    // reset its search and toggle).
    if (session->getId() != shownSessionId) {
        shownSessionId = session->getId();
        const QSignalBlocker blockSearch(searchEdit);
        const QSignalBlocker blockToggle(unansweredBox);
        searchEdit->clear();
        unansweredBox->setChecked(false);
        answerEdit->clear();
        urlEdit->clear();
        startsAtEdit->clear();
        lectureLinkEdit->clear();
        clearErrors();
        player->clear(session->getRecording()
                          ? "Click the timeline or a question to play that moment."
                          : "No recording attached yet.");
    }

    titleLabel->setText(QString::fromStdString(session->getName()));
    QString status = ViewText::formatStatus(session->getStatus());
    if (session->isEnded()) {
        status += "  ·  " + ViewText::formatClockTime(session->getStartTime()) + "  ·  " +
                  ViewText::formatDuration(session->getStartTime(), session->getEndTime());
    }
    statusLabel->setText(status);
    publishButton->setVisible(educator && session->isInReview());

    // ---- Timeline ----
    const std::vector<Timeline::Bucket> buckets = controller->getTimeline();
    const int peak = Timeline::getPeakIndex(buckets);
    timeline->setBuckets(buckets, peak);
    timeline->setClickable(session->getRecording() != nullptr);
    const long long width = buckets.size() > 1 ? buckets[1].startSeconds - buckets[0].startSeconds
                                               : Timeline::kBucketSeconds;
    QString peakText = peak >= 0 ? "Most confusing: " + ViewText::formatBucket(buckets[peak], width, false)
                                 : QString("Nobody pressed I'm Lost in this session.");
    if (session->getRecording()) {
        peakText += "\nClick a bar or a question to play that moment.";
    }
    peakLabel->setText(peakText);

    refreshRecordingCard(*session, educator);
    refreshQuestions(*session, educator);
}

void SessionViewerPage::refreshRecordingCard(const Session& session, bool educator) {
    recordingBox->setVisible(educator);
    if (!educator) {
        return;
    }
    const Recording* recording = session.getRecording();
    recordingLabel->setText(recording ? ViewText::formatRecording(*recording)
                                      : QString("No recording attached."));
    removeRecordingButton->setVisible(recording != nullptr);

    const auto& detail = controller->getOpenCourse();
    const bool hasFolder = detail && detail->course.hasPanoptoFolder();
    setupBox->setVisible(!hasFolder);
    findButton->setVisible(hasFolder);
    const QString folderName = controller->getPanoptoFolderName();
    folderLabel->setVisible(hasFolder);
    folderLabel->setText(folderName.isEmpty() ? QString("Panopto folder linked.")
                                              : "Panopto folder: " + folderName);
}

void SessionViewerPage::refreshQuestions(const Session& session, bool educator) {
    const int keepQuestion = selectedQuestionId();
    const int keepAnswer = selectedAnswerId();
    const std::vector<SessionQuestion> questions = controller->getVisibleQuestions();
    {
        const QSignalBlocker block(questionTree);
        questionTree->clear();
        for (const SessionQuestion& question : questions) {
            auto* questionItem = new QTreeWidgetItem(questionTree);
            questionItem->setText(0, ViewText::formatViewerQuestion(session, question));
            questionItem->setData(0, kQuestionRole, question.getId());
            questionItem->setData(0, kAnswerRole, 0);
            QFont bold = questionItem->font(0);
            bold.setBold(true);
            questionItem->setFont(0, bold);
            if (question.getId() == keepQuestion && keepAnswer == 0) {
                questionTree->setCurrentItem(questionItem);
            }
            for (const Answer& answer : question.getAnswers()) {
                auto* answerItem = new QTreeWidgetItem(questionItem);
                answerItem->setText(0, ViewText::formatAnswerRow(answer));
                answerItem->setData(0, kQuestionRole, question.getId());
                answerItem->setData(0, kAnswerRole, answer.getId());
                if (answer.isByEducator()) {
                    answerItem->setForeground(0, QColor(0x1b, 0x5e, 0x20));
                }
                if (answer.getId() == keepAnswer) {
                    questionTree->setCurrentItem(answerItem);
                }
            }
            questionItem->setExpanded(true);
        }
    }
    emptyLabel->setVisible(questions.empty());
    emptyLabel->setText(session.getQuestionCount() == 0 ? "No questions in this session."
                                                        : "No question matches your search.");
    deleteQuestionButton->setVisible(educator);
    updateButtons();
}

void SessionViewerPage::updateButtons() {
    const auto& session = controller->getOpenSession();
    const User* user = controller->getUser();
    const bool educator = controller->getRole() == CourseRole::Educator;
    const int questionId = selectedQuestionId();
    const int answerId = selectedAnswerId();

    const SessionQuestion* question = session ? session->findQuestion(questionId) : nullptr;
    const Answer* answer = question && answerId ? question->findAnswer(answerId) : nullptr;
    const bool mine = answer && user && answer->isBy(user->getId());

    deleteQuestionButton->setEnabled(educator && question != nullptr);
    editAnswerButton->setEnabled(mine);
    deleteAnswerButton->setEnabled(mine || (educator && answer != nullptr));  // educator moderates
    answerEdit->setEnabled(question != nullptr);
    answerButton->setEnabled(question != nullptr);
}

int SessionViewerPage::selectedQuestionId() const {
    QTreeWidgetItem* item = questionTree->currentItem();
    return item ? item->data(0, kQuestionRole).toInt() : 0;
}

int SessionViewerPage::selectedAnswerId() const {
    QTreeWidgetItem* item = questionTree->currentItem();
    return item ? item->data(0, kAnswerRole).toInt() : 0;
}

void SessionViewerPage::onItemClicked(QTreeWidgetItem* item) {
    // A question click plays the moment it was asked.
    const auto& session = controller->getOpenSession();
    if (!item || !session || item->data(0, kAnswerRole).toInt() != 0) {
        return;
    }
    if (const SessionQuestion* question = session->findQuestion(item->data(0, kQuestionRole).toInt())) {
        controller->jumpTo(session->getOffsetSeconds(question->getTime()));
    }
}

void SessionViewerPage::editAnswer() {
    const auto& session = controller->getOpenSession();
    const SessionQuestion* question = session ? session->findQuestion(selectedQuestionId()) : nullptr;
    const Answer* answer = question ? question->findAnswer(selectedAnswerId()) : nullptr;
    if (!answer) {
        return;
    }
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(
        this, "Edit answer", "Your answer:", QString::fromStdString(answer->getText()), &ok);
    if (ok) {
        clearError(questionError);
        controller->editAnswer(answer->getId(), text);
    }
}

void SessionViewerPage::deleteAnswer() {
    const int answerId = selectedAnswerId();
    if (answerId != 0 && confirm("Delete answer", "Delete this answer?")) {
        clearError(questionError);
        controller->deleteAnswer(answerId);
    }
}

void SessionViewerPage::deleteQuestion() {
    const int questionId = selectedQuestionId();
    if (questionId != 0 &&
        confirm("Remove question", "Remove this question and its answers from the session?")) {
        clearError(questionError);
        controller->deleteQuestion(questionId);
    }
}

bool SessionViewerPage::showError(const QString& request, Error error) {
    if (request == Request::kPublishSession) {
        setError(publishError, error);
    } else if (request == Request::kAttachRecording || request == Request::kRemoveRecording ||
               request == Request::kCheckRecording || request == Request::kLinkPanoptoFolder) {
        setError(recordingError, error);
    } else if (request == Request::kAddAnswer || request == Request::kEditAnswer ||
               request == Request::kDeleteAnswer || request == Request::kDeleteQuestion) {
        setError(questionError, error);
    } else {
        return false;
    }
    return true;
}

void SessionViewerPage::onNotice(AppController::Notice notice) {
    using N = AppController::Notice;
    switch (notice) {
        case N::AnswerSent:
            answerEdit->clear();
            clearError(questionError);
            break;
        case N::AnswerUpdated:
        case N::AnswerDeleted:
        case N::QuestionDeleted:
            clearError(questionError);
            break;
        case N::RecordingAttached:
            urlEdit->clear();
            startsAtEdit->clear();
            clearError(recordingError);
            player->clear("Click the timeline or a question to play that moment.");
            break;
        case N::RecordingRemoved:
            clearError(recordingError);
            player->clear("No recording attached yet.");
            break;
        case N::PanoptoLinked:
            lectureLinkEdit->clear();
            clearError(recordingError);
            break;
        case N::SessionPublished:
            clearError(publishError);
            break;
        default:
            break;
    }
}
