#include "mainpage.h"
#include "ui_mainpage.h"

MainPage::MainPage(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainPage) {
  ui->setupUi(this);

  connect(ui->startButton, &QPushButton::clicked, this, &MainPage::startClicked);
  connect(ui->logoutButton, &QPushButton::clicked, this, &MainPage::logoutClicked);
  connect(ui->sessionList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    emit sessionSelected(ui->sessionList->row(item));
  });
}

MainPage::~MainPage() {
  delete ui;
}

void MainPage::setStatus(const QString& text) {
  ui->statusLabel->setText(text);
}

void MainPage::setStartTime(const QString& text) {
  ui->startLabel->setText(text);
}

void MainPage::setEndTime(const QString& text) {
  ui->endLabel->setText(text);
}

void MainPage::setRunning(bool running) {
  ui->startButton->setEnabled(!running);
  ui->logoutButton->setEnabled(!running);
}

void MainPage::showPresses(const QStringList& rows) {
  ui->timestampList->clear();
  for (const QString& row : rows) {
    ui->timestampList->addItem(row);
  }
}

void MainPage::showSessions(const QStringList& rows) {
  ui->sessionList->clear();
  for (const QString& row : rows) {
    ui->sessionList->addItem(row);
  }
}
