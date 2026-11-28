#include "studentlivepage.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "protocol.h"
#include "viewtext.h"
#include "uifonts.h"

namespace Request = Protocol::Request;

namespace {
// What a row in the item list is: kept in Qt::UserRole + 1.
enum ItemKind { kLostItem = 1, kQuestionItem = 2 };
}  // namespace

StudentLivePage::StudentLivePage(AppController* controller, QWidget* parent)
    : BasePage(controller, parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);

    titleLabel = makeHeading("");
    layout->addWidget(titleLabel);
    statusLabel = new QLabel(this);
    layout->addWidget(statusLabel);

    // ---- I'm Lost ----
    lostButton = new QPushButton("I'm Lost", this);
    QFont lostFont = lostButton->font();
    lostFont.setPointSize(UiFont::kLarge);
    lostFont.setBold(true);
    lostButton->setFont(lostFont);
    lostButton->setMinimumHeight(90);
    layout->addWidget(lostButton);
    lostError = makeErrorLabel();
    layout->addWidget(lostError);

    // ---- Ask a question ----
    layout->addWidget(makeHeading("Question Box", UiFont::kSubheading));
    questionEdit = new QPlainTextEdit(this);
    questionEdit->setPlaceholderText("Type a question to save for later...");
    questionEdit->setMaximumHeight(90);
    layout->addWidget(questionEdit);
    auto* sendRow = new QHBoxLayout;
    sendRow->addStretch();
    auto* sendButton = new QPushButton("Send Question", this);
    sendRow->addWidget(sendButton);
    layout->addLayout(sendRow);
    questionError = makeErrorLabel();
    layout->addWidget(questionError);

    // ---- Your items ----
    layout->addWidget(makeHeading("Your Lost presses and questions", UiFont::kSubheading));
    itemList = new QListWidget(this);
    layout->addWidget(itemList, 1);
    auto* itemButtons = new QHBoxLayout;
    itemButtons->addStretch();
    editButton = new QPushButton("Edit Question…", this);
    deleteButton = new QPushButton("Delete…", this);
    itemButtons->addWidget(editButton);
    itemButtons->addWidget(deleteButton);
    layout->addLayout(itemButtons);
    itemError = makeErrorLabel();
    layout->addWidget(itemError);

    // ---- Wiring ----
    connect(lostButton, &QPushButton::clicked, this, [this] {
        clearError(lostError);
        this->controller->pressLost();
    });
    connect(sendButton, &QPushButton::clicked, this, [this] {
        clearError(questionError);
        // Sent as typed, even empty: the server rejects it and we show why.
        this->controller->askQuestion(questionEdit->toPlainText());
    });
    connect(itemList, &QListWidget::currentRowChanged, this, &StudentLivePage::updateButtons);
    connect(itemList, &QListWidget::itemActivated, this, &StudentLivePage::editSelected);
    connect(editButton, &QPushButton::clicked, this, &StudentLivePage::editSelected);
    connect(deleteButton, &QPushButton::clicked, this, &StudentLivePage::deleteSelected);
    connect(controller, &AppController::notice, this, &StudentLivePage::onNotice);
}

void StudentLivePage::refresh() {
    const auto& session = controller->getOpenSession();
    if (!session) {
        return;
    }
    QString title = QString::fromStdString(session->getName());
    if (const auto& course = controller->getOpenCourse()) {
        title += "  ·  " + QString::fromStdString(course->course.getName());
    }
    titleLabel->setText(title);
    statusLabel->setText(ViewText::formatLiveStatus(*session));

    // Lost presses and questions merged in time order, newest first.
    struct Row {
        Session::Clock::time_point time;
        int kind;
        int id;
        QString text;
    };
    std::vector<Row> rows;
    for (const LostPing& ping : session->getLostPings()) {
        rows.push_back({ping.getTime(), kLostItem, ping.getId(), ViewText::formatLostRow(*session, ping)});
    }
    for (const SessionQuestion& question : session->getQuestions()) {
        rows.push_back({question.getTime(), kQuestionItem, question.getId(),
                        ViewText::formatQuestionRow(*session, question)});
    }
    std::stable_sort(rows.begin(), rows.end(),
                     [](const Row& a, const Row& b) { return a.time > b.time; });

    QListWidgetItem* current = itemList->currentItem();
    const int selectedKind = current ? current->data(Qt::UserRole + 1).toInt() : 0;
    const int selectedId = current ? current->data(Qt::UserRole).toInt() : 0;
    {
        const QSignalBlocker block(itemList);
        itemList->clear();
        for (const Row& row : rows) {
            auto* item = new QListWidgetItem(row.text, itemList);
            item->setData(Qt::UserRole, row.id);
            item->setData(Qt::UserRole + 1, row.kind);
            if (row.kind == selectedKind && row.id == selectedId) {
                itemList->setCurrentItem(item);
            }
        }
    }
    updateButtons();
}

bool StudentLivePage::showError(const QString& request, Error error) {
    if (request == Request::kAddLost) {
        setError(lostError, error);
    } else if (request == Request::kAddQuestion) {
        setError(questionError, error);
    } else if (request == Request::kEditQuestion || request == Request::kDeleteQuestion ||
               request == Request::kDeleteLost) {
        setError(itemError, error);
    } else {
        return false;
    }
    return true;
}

void StudentLivePage::updateButtons() {
    QListWidgetItem* item = itemList->currentItem();
    const int kind = item ? item->data(Qt::UserRole + 1).toInt() : 0;
    editButton->setEnabled(kind == kQuestionItem);
    deleteButton->setEnabled(kind != 0);
}

void StudentLivePage::editSelected() {
    QListWidgetItem* item = itemList->currentItem();
    const auto& session = controller->getOpenSession();
    if (!item || !session || item->data(Qt::UserRole + 1).toInt() != kQuestionItem) {
        return;
    }
    const int id = item->data(Qt::UserRole).toInt();
    const SessionQuestion* question = session->findQuestion(id);
    if (!question) {
        return;
    }
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(
        this, "Edit question", "Your question:", QString::fromStdString(question->getText()), &ok);
    if (ok) {
        clearError(itemError);
        controller->editQuestion(id, text);
    }
}

void StudentLivePage::deleteSelected() {
    QListWidgetItem* item = itemList->currentItem();
    if (!item) {
        return;
    }
    const int id = item->data(Qt::UserRole).toInt();
    clearError(itemError);
    if (item->data(Qt::UserRole + 1).toInt() == kQuestionItem) {
        if (confirm("Delete question", "Delete this question?")) {
            controller->deleteQuestion(id);
        }
    } else if (confirm("Delete Lost press", "Take back this I'm Lost press?")) {
        controller->deleteLost(id);
    }
}

void StudentLivePage::onNotice(AppController::Notice notice) {
    using N = AppController::Notice;
    if (notice == N::QuestionSent) {
        questionEdit->clear();
        clearError(questionError);
    } else if (notice == N::LostSent) {
        clearError(lostError);
    } else if (notice == N::SessionJoined) {
        questionEdit->clear();
        clearErrors();
    }
}
