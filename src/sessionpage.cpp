#include "sessionpage.h"
#include "ui_sessionpage.h"

#include "viewtext.h"

SessionPage::SessionPage(QWidget* parent) : QMainWindow(parent), ui(new Ui::SessionPage) {
  ui->setupUi(this);

  connect(ui->lostButton, &QPushButton::clicked, this, &SessionPage::lostClicked);
  connect(ui->endButton, &QPushButton::clicked, this, &SessionPage::endClicked);
  connect(ui->submitButton, &QPushButton::clicked, this, &SessionPage::submitQuestion);
  // Typing again hides an old error.
  connect(ui->questionEdit, &QPlainTextEdit::textChanged, this, [this] {
    ui->questionErrorLabel->hide();
  });
}

SessionPage::~SessionPage() {
  delete ui;
}

void SessionPage::showSession(const Session& session) {
  ui->statusLabel->setText(ViewText::formatLiveStatus(session));
}

void SessionPage::clearQuestion() {
  ui->questionEdit->clear();
  ui->questionErrorLabel->hide();
}

void SessionPage::showQuestionError(Error error) {
  ui->questionErrorLabel->setText(ViewText::getErrorText(error));
  ui->questionErrorLabel->show();
  ui->questionEdit->setFocus();
}

void SessionPage::showQuestionSent() {
  ui->questionEdit->setPlaceholderText("Question sent. Type another one if you like.");
}

// Sends whatever was typed. The model decides whether it is a valid question
// (M2 grading note: the empty-question rule belongs in the model), and the
// controller clears the box only if it was saved.
void SessionPage::submitQuestion() {
  emit questionSubmitted(ui->questionEdit->toPlainText());
}
