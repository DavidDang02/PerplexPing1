#include "sessionpage.h"
#include "ui_sessionpage.h"

SessionPage::SessionPage(QWidget* parent) : QMainWindow(parent), ui(new Ui::SessionPage) {
  ui->setupUi(this);

  connect(ui->lostButton, &QPushButton::clicked, this, &SessionPage::lostClicked);
  connect(ui->endButton, &QPushButton::clicked, this, &SessionPage::endClicked);
  connect(ui->submitButton, &QPushButton::clicked, this, &SessionPage::submitQuestion);
}

SessionPage::~SessionPage() {
  delete ui;
}

void SessionPage::clearQuestion() {
  ui->questionEdit->clear();
}

void SessionPage::submitQuestion() {
  const QString text = ui->questionEdit->toPlainText().trimmed();
  if (text.isEmpty()) {
    return;
  }
  emit questionSubmitted(text);
  clearQuestion();
}
