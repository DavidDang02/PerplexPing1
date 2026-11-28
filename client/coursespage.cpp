#include "coursespage.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "cardlist.h"
#include "protocol.h"
#include "viewtext.h"

namespace Request = Protocol::Request;

CoursesPage::CoursesPage(AppController* controller, QWidget* parent)
    : BasePage(controller, parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);

    // ---- My Courses (the account menu and Log Out are in the NavBar) ----
    layout->addWidget(makeHeading("My Courses"));
    courseList = new CardList(1, this);  // course cards; scrolls when they don't fit
    layout->addWidget(courseList, 1);
    emptyLabel = new QLabel("You aren't in any courses yet. Join one with its course code, "
                            "or create your own below.", this);
    emptyLabel->setWordWrap(true);
    layout->addWidget(emptyLabel);

    // ---- Join a course / join a live session ----
    auto* joinRow = new QHBoxLayout;

    auto* joinCourseBox = new QGroupBox("Join a course", this);
    auto* joinCourseLayout = new QVBoxLayout(joinCourseBox);
    auto* joinCourseRow = new QHBoxLayout;
    courseCodeEdit = new QLineEdit(joinCourseBox);
    courseCodeEdit->setPlaceholderText("Course code, e.g. K7M2QX");
    auto* joinCourseButton = new QPushButton("Join Course", joinCourseBox);
    joinCourseRow->addWidget(courseCodeEdit);
    joinCourseRow->addWidget(joinCourseButton);
    joinCourseLayout->addLayout(joinCourseRow);
    joinCourseError = makeErrorLabel();
    joinCourseLayout->addWidget(joinCourseError);
    joinRow->addWidget(joinCourseBox);

    auto* joinSessionBox = new QGroupBox("Join a live session", this);
    auto* joinSessionLayout = new QVBoxLayout(joinSessionBox);
    auto* joinSessionRow = new QHBoxLayout;
    sessionCodeEdit = new QLineEdit(joinSessionBox);
    sessionCodeEdit->setPlaceholderText("Session code, e.g. 482913");
    auto* joinSessionButton = new QPushButton("Join Session", joinSessionBox);
    joinSessionRow->addWidget(sessionCodeEdit);
    joinSessionRow->addWidget(joinSessionButton);
    joinSessionLayout->addLayout(joinSessionRow);
    joinSessionError = makeErrorLabel();
    joinSessionLayout->addWidget(joinSessionError);
    joinRow->addWidget(joinSessionBox);
    layout->addLayout(joinRow);

    // ---- Create a course ----
    auto* createBox = new QGroupBox("Create a course (you'll be its educator)", this);
    auto* createLayout = new QVBoxLayout(createBox);
    auto* createRow = new QHBoxLayout;
    newCourseEdit = new QLineEdit(createBox);
    newCourseEdit->setPlaceholderText("Course name, e.g. CSCI 187");
    auto* createButton = new QPushButton("Create", createBox);
    createRow->addWidget(newCourseEdit);
    createRow->addWidget(createButton);
    createLayout->addLayout(createRow);
    auto* toggles = new QHBoxLayout;
    anonymousBox = new QCheckBox("Anonymous (hide students' names from classmates)", createBox);
    sharedBox = new QCheckBox("Share questions with the class once published", createBox);
    sharedBox->setChecked(true);
    toggles->addWidget(anonymousBox);
    toggles->addWidget(sharedBox);
    toggles->addStretch();
    createLayout->addLayout(toggles);
    createError = makeErrorLabel();
    createLayout->addWidget(createError);
    layout->addWidget(createBox);

    // ---- Wiring: every box is sent as typed, even empty (the server decides) ----
    // Double-click (or Enter on) a card to open it.
    connect(courseList, &QListWidget::itemActivated, this, &CoursesPage::openSelected);

    auto joinCourse = [this] {
        clearError(joinCourseError);
        this->controller->joinCourse(courseCodeEdit->text());
    };
    connect(joinCourseButton, &QPushButton::clicked, this, joinCourse);
    connect(courseCodeEdit, &QLineEdit::returnPressed, this, joinCourse);

    auto joinSession = [this] {
        clearError(joinSessionError);
        this->controller->joinSession(sessionCodeEdit->text());
    };
    connect(joinSessionButton, &QPushButton::clicked, this, joinSession);
    connect(sessionCodeEdit, &QLineEdit::returnPressed, this, joinSession);

    auto create = [this] {
        clearError(createError);
        this->controller->createCourse(newCourseEdit->text(), anonymousBox->isChecked(),
                                       sharedBox->isChecked());
    };
    connect(createButton, &QPushButton::clicked, this, create);
    connect(newCourseEdit, &QLineEdit::returnPressed, this, create);

    connect(controller, &AppController::notice, this, &CoursesPage::onNotice);

    UiFont::styleSectionTitles(this);  // box titles as sub-headings
}

void CoursesPage::refresh() {
    // Keep the highlighted course across redraws.
    const int selectedId = courseList->currentItem()
                               ? courseList->currentItem()->data(Qt::UserRole).toInt()
                               : 0;
    courseList->clear();
    for (const Course& course : controller->getCourses()) {
        auto* item = new QListWidgetItem(ViewText::formatCourseRow(course), courseList);
        item->setData(Qt::UserRole, course.getId());
        item->setData(CardList::kTitleRole, QString::fromStdString(course.getName()));
        item->setData(CardList::kBadgeRole, ViewText::formatRole(course.getRole()));
        item->setData(CardList::kDetailRole, ViewText::formatCourseCardDetail(course));
        item->setToolTip("Double-click to open");
        if (course.getId() == selectedId) {
            courseList->setCurrentItem(item);
        }
    }
    emptyLabel->setVisible(controller->getCourses().empty());
}

bool CoursesPage::showError(const QString& request, Error error) {
    if (request == Request::kJoinCourse) {
        setError(joinCourseError, error);
    } else if (request == Request::kJoinSession) {
        setError(joinSessionError, error);
    } else if (request == Request::kCreateCourse) {
        setError(createError, error);
    } else {
        return false;
    }
    return true;
}

void CoursesPage::openSelected() {
    if (QListWidgetItem* item = courseList->currentItem()) {
        controller->openCourse(item->data(Qt::UserRole).toInt());
    }
}

void CoursesPage::onNotice(AppController::Notice notice) {
    using N = AppController::Notice;
    switch (notice) {
        case N::CourseJoined:
            courseCodeEdit->clear();
            clearError(joinCourseError);
            break;
        case N::SessionJoined:
        case N::EducatorJoined:
            sessionCodeEdit->clear();
            clearError(joinSessionError);
            break;
        case N::CourseCreated:
            newCourseEdit->clear();
            anonymousBox->setChecked(false);
            sharedBox->setChecked(true);
            clearError(createError);
            break;
        case N::LoggedOut:
        case N::AccountDeleted:
        case N::LoggedOutElsewhere:
            courseCodeEdit->clear();
            sessionCodeEdit->clear();
            newCourseEdit->clear();
            clearErrors();
            break;
        default:
            break;
    }
}
