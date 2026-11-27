#include "mainpage.h"
#include "ui_mainpage.h"

#include "viewtext.h"

MainPage::MainPage(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainPage) {
  ui->setupUi(this);

  // Start Session is twice the normal button height on every platform.
  ui->startButton->setMinimumHeight(2 * ui->startButton->sizeHint().height());

  connect(ui->startButton, &QPushButton::clicked, this, &MainPage::startClicked);
  connect(ui->logoutButton, &QPushButton::clicked, this, &MainPage::logoutClicked);
  connect(ui->sessionList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    emit sessionSelected(ui->sessionList->row(item));
  });
}

MainPage::~MainPage() {
  delete ui;
}

void MainPage::showUser(const User& user) {
  ui->statusLabel->setText("Logged in as " + QString::fromStdString(user.getName()));
}

void MainPage::showSessions(const std::vector<Session>& sessions) {
  ui->sessionList->clear();
  for (const Session& session : sessions) {
    ui->sessionList->addItem(ViewText::formatSessionRow(session));
  }
}

void MainPage::selectSessionRow(int row) {
  ui->sessionList->setCurrentRow(row);
}

void MainPage::showSessionDetails(const Session& session) {
  ui->startLabel->setText("Started: " + ViewText::formatClockTime(session.getStartTime()));
  ui->endLabel->setText(session.isEnded()
                            ? "Ended: " + ViewText::formatClockTime(session.getEndTime())
                            : QString("Ended: —"));
  ui->timestampList->clear();
  for (const SessionInput& input : session.getInputs()) {
    ui->timestampList->addItem(ViewText::formatInput(session, input));
  }
}

void MainPage::clearSessionDetails() {
  ui->startLabel->setText("Started: —");
  ui->endLabel->setText("Ended: —");
  ui->timestampList->clear();
}
