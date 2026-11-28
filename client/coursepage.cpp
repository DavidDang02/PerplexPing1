#include "coursepage.h"

#include <QCheckBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include "cardlist.h"
#include "protocol.h"
#include "viewtext.h"

namespace Request = Protocol::Request;

namespace {
constexpr int kLiveRole = Qt::UserRole + 1;  // session rows: true while live
}

CoursePage::CoursePage(AppController* controller, QWidget* parent)
    : BasePage(controller, parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);

    // ---- Top: course name, course code (back and the role are in the NavBar) ----
    titleLabel = makeHeading("");
    layout->addWidget(titleLabel);
    // The course code: one click copies it to the clipboard.
    codeRow = new QWidget(this);
    auto* codeLayout = new QHBoxLayout(codeRow);
    codeLayout->setContentsMargins(0, 0, 0, 0);
    codeButton = new QPushButton(codeRow);
    codeButton->setCursor(Qt::PointingHandCursor);
    codeButton->setToolTip("Click to copy the course code");
    codeButton->setStyleSheet(
        "QPushButton { font-weight: 600; padding: 4px 10px; border: 1px dashed palette(mid);"
        " border-radius: 6px; background: transparent; }"
        "QPushButton:hover { border-style: solid; border-color: palette(highlight); }"
        "QPushButton:pressed { background: palette(midlight); }");
    auto* codeHint = new QLabel("Students join with this. Click it to copy.", codeRow);
    codeHint->setStyleSheet("color: palette(placeholder-text);");
    codeLayout->addWidget(codeButton);
    codeLayout->addWidget(codeHint);
    codeLayout->addStretch();
    layout->addWidget(codeRow);
    copiedTimer = new QTimer(this);
    copiedTimer->setSingleShot(true);
    copiedTimer->setInterval(1500);
    connect(copiedTimer, &QTimer::timeout, this, &CoursePage::showCourseCode);
    connect(codeButton, &QPushButton::clicked, this, &CoursePage::copyCourseCode);

    // ---- Start a session (educator) ----
    startBox = new QGroupBox("Start a session", this);
    auto* startLayout = new QVBoxLayout(startBox);
    auto* startRow = new QHBoxLayout;
    sessionNameEdit = new QLineEdit(startBox);
    sessionNameEdit->setPlaceholderText("Session name, e.g. Lecture 3: Recursion");
    auto* startButton = new QPushButton("Start Session", startBox);
    startButton->setMinimumHeight(2 * startButton->sizeHint().height());
    startRow->addWidget(sessionNameEdit);
    startRow->addWidget(startButton);
    startLayout->addLayout(startRow);
    startError = makeErrorLabel();
    startLayout->addWidget(startError);
    layout->addWidget(startBox);

    // ---- Sessions ----
    layout->addWidget(makeHeading("Sessions", UiFont::kSubheading));
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Search sessions by name…");
    searchEdit->setClearButtonEnabled(true);
    layout->addWidget(searchEdit);
    sessionList = new CardList(2, this);  // session cards; scrolls when they don't fit
    layout->addWidget(sessionList, 1);
    emptyLabel = new QLabel(this);
    emptyLabel->setWordWrap(true);
    layout->addWidget(emptyLabel);

    auto* sessionButtons = new QHBoxLayout;
    renameButton = new QPushButton("Rename…", this);
    deleteButton = new QPushButton("Delete…", this);
    sessionButtons->addStretch();
    sessionButtons->addWidget(renameButton);
    sessionButtons->addWidget(deleteButton);
    layout->addLayout(sessionButtons);
    sessionError = makeErrorLabel();
    layout->addWidget(sessionError);

    // ---- Course settings (educator) ----
    settingsBox = new QGroupBox("Course settings", this);
    auto* settingsLayout = new QVBoxLayout(settingsBox);
    courseNameEdit = new QLineEdit(settingsBox);
    courseNameEdit->setPlaceholderText("Course name");
    settingsLayout->addWidget(courseNameEdit);
    anonymousBox = new QCheckBox("Anonymous: hide students' names from other students (you still see them)", settingsBox);
    sharedBox = new QCheckBox("Questions shared: once published, students see everyone's questions",
                              settingsBox);
    settingsLayout->addWidget(anonymousBox);
    settingsLayout->addWidget(sharedBox);
    auto* settingsButtons = new QHBoxLayout;
    auto* saveButton = new QPushButton("Save Settings", settingsBox);
    auto* deleteCourseButton = new QPushButton("Delete Course…", settingsBox);
    deleteCourseButton->setStyleSheet("color:#b00020;");
    settingsButtons->addWidget(saveButton);
    settingsButtons->addStretch();
    settingsButtons->addWidget(deleteCourseButton);
    settingsLayout->addLayout(settingsButtons);
    settingsError = makeErrorLabel();
    settingsLayout->addWidget(settingsError);
    layout->addWidget(settingsBox);

    // ---- Wiring ----

    auto start = [this] {
        clearError(startError);
        this->controller->startSession(sessionNameEdit->text());
    };
    connect(startButton, &QPushButton::clicked, this, start);
    connect(sessionNameEdit, &QLineEdit::returnPressed, this, start);

    connect(searchEdit, &QLineEdit::textChanged, controller, &AppController::setSessionSearch);
    // Double-click (or Enter on) a card to open it.
    connect(sessionList, &QListWidget::itemActivated, this, &CoursePage::openSelected);
    connect(sessionList, &QListWidget::currentRowChanged, this, [this](int row) {
        renameButton->setEnabled(row >= 0);
        deleteButton->setEnabled(row >= 0);
    });
    connect(renameButton, &QPushButton::clicked, this, &CoursePage::renameSelected);
    connect(deleteButton, &QPushButton::clicked, this, &CoursePage::deleteSelected);

    connect(saveButton, &QPushButton::clicked, this, [this] {
        clearError(settingsError);
        this->controller->updateCourse(courseNameEdit->text(), anonymousBox->isChecked(),
                                       sharedBox->isChecked());
    });
    connect(deleteCourseButton, &QPushButton::clicked, this, [this] {
        const auto& detail = this->controller->getOpenCourse();
        const QString name = detail ? QString::fromStdString(detail->course.getName()) : "";
        if (confirm("Delete course",
                    "Delete \"" + name + "\" with all of its sessions, questions and answers? "
                    "Your students lose access too. This can't be undone.")) {
            this->controller->deleteCourse();
        }
    });

    connect(controller, &AppController::notice, this, &CoursePage::onNotice);

    UiFont::styleSectionTitles(this);  // box titles as sub-headings
}

void CoursePage::refresh() {
    const auto& detail = controller->getOpenCourse();
    if (!detail) {
        return;
    }
    const Course& course = detail->course;
    const bool educator = controller->getRole() == CourseRole::Educator;

    titleLabel->setText(QString::fromStdString(course.getName()));
    codeRow->setVisible(educator && !course.getJoinCode().empty());
    if (!copiedTimer->isActive()) {
        showCourseCode();
    }

    startBox->setVisible(educator);
    settingsBox->setVisible(educator);
    renameButton->setVisible(educator);
    deleteButton->setVisible(educator);

    // A new course: the controller cleared its search, so clear the box too,
    // and fill the settings boxes from it.
    if (course.getId() != shownCourseId) {
        shownCourseId = course.getId();
        const QSignalBlocker block(searchEdit);
        searchEdit->clear();
        sessionNameEdit->clear();
        clearErrors();
        fillSettings(course);
    } else if (shownSettings != std::make_tuple(course.getName(), course.isAnonymous(),
                                                course.areQuestionsShared())) {
        // The course's settings changed on the server (saved here, or by the
        // educator elsewhere): show them. Otherwise leave the boxes alone, so
        // edits not yet saved survive a redraw.
        fillSettings(course);
    }

    // ---- Session list, keeping the highlighted row ----
    const int selectedId = selectedSessionId();
    {
        const QSignalBlocker block(sessionList);
        sessionList->clear();
        for (const SessionSummary& session : controller->getVisibleSessions()) {
            auto* item = new QListWidgetItem(ViewText::formatSessionRow(session), sessionList);
            item->setData(Qt::UserRole, session.id);
            item->setData(kLiveRole, session.isLive());
            item->setData(CardList::kTitleRole, QString::fromStdString(session.name));
            item->setData(CardList::kDetailRole, ViewText::formatSessionCardDetail(session));
            item->setData(CardList::kBadgeRole, ViewText::formatStatus(session.status));
            item->setData(CardList::kAccentRole, session.isLive());
            item->setToolTip(session.isLive() && !educator
                                 ? "Live now. Join it with its session code on My Courses."
                                 : "Double-click to open");
            if (session.id == selectedId) {
                sessionList->setCurrentItem(item);
            }
        }
    }
    const bool anySessions = !detail->sessions.empty();
    emptyLabel->setVisible(sessionList->count() == 0);
    emptyLabel->setText(!anySessions ? (educator ? "No sessions yet. Start one above when class begins."
                                                 : "No sessions yet.")
                                     : "No session matches your search.");

    const bool hasSelection = sessionList->currentItem() != nullptr;
    renameButton->setEnabled(hasSelection);
    deleteButton->setEnabled(hasSelection);
}

bool CoursePage::showError(const QString& request, Error error) {
    if (request == Request::kStartSession) {
        setError(startError, error);
    } else if (request == Request::kUpdateCourse || request == Request::kDeleteCourse) {
        setError(settingsError, error);
    } else if (request == Request::kOpenSession || request == Request::kRenameSession ||
               request == Request::kDeleteSession) {
        setError(sessionError, error);
    } else {
        return false;
    }
    return true;
}

int CoursePage::selectedSessionId() const {
    QListWidgetItem* item = sessionList->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : 0;
}

void CoursePage::showCourseCode() {
    const auto& detail = controller->getOpenCourse();
    const QString code = detail ? QString::fromStdString(detail->course.getJoinCode()) : QString();
    codeButton->setText("Course code: " + code + "  ⧉");
    codeButton->setMinimumWidth(codeButton->sizeHint().width());  // no jump on "Copied!"
}

void CoursePage::copyCourseCode() {
    const auto& detail = controller->getOpenCourse();
    if (!detail || detail->course.getJoinCode().empty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(QString::fromStdString(detail->course.getJoinCode()));
    codeButton->setText("Copied!  ✓");
    copiedTimer->start();  // back to the code shortly
}

bool CoursePage::canOpenSelected() const {
    const QListWidgetItem* item = sessionList->currentItem();
    if (!item) {
        return false;
    }
    return controller->getRole() == CourseRole::Educator || !item->data(kLiveRole).toBool();
}

void CoursePage::openSelected() {
    if (!canOpenSelected()) {
        return;
    }
    if (const int id = selectedSessionId()) {
        clearError(sessionError);
        controller->openSession(id);
    }
}

void CoursePage::renameSelected() {
    QListWidgetItem* item = sessionList->currentItem();
    if (item == nullptr) {
        return;
    }
    QString current;
    for (const SessionSummary& session : controller->getVisibleSessions()) {
        if (session.id == selectedSessionId()) {
            current = QString::fromStdString(session.name);
        }
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Rename session", "New name:",
                                               QLineEdit::Normal, current, &ok);
    if (ok) {
        clearError(sessionError);
        controller->renameSession(selectedSessionId(), name);  // empty too: the server says why
    }
}

void CoursePage::deleteSelected() {
    QListWidgetItem* item = sessionList->currentItem();
    if (item == nullptr) {
        return;
    }
    if (confirm("Delete session",
                "Delete this session with all of its Lost presses, questions and answers? "
                "This can't be undone.")) {
        clearError(sessionError);
        controller->deleteSession(selectedSessionId());
    }
}

void CoursePage::fillSettings(const Course& course) {
    shownSettings = {course.getName(), course.isAnonymous(), course.areQuestionsShared()};
    courseNameEdit->setText(QString::fromStdString(course.getName()));
    anonymousBox->setChecked(course.isAnonymous());
    sharedBox->setChecked(course.areQuestionsShared());
}

void CoursePage::onNotice(AppController::Notice notice) {
    using N = AppController::Notice;
    if (notice == N::SessionStarted) {
        sessionNameEdit->clear();
        clearError(startError);
    } else if (notice == N::CourseUpdated) {
        // Not fillSettings here: the open course still holds the old values
        // until refreshCourse's reply arrives, and refresh() fills the boxes then.
        clearError(settingsError);
    } else if (notice == N::SessionRenamed || notice == N::SessionDeleted) {
        clearError(sessionError);
    }
}
