#include "mainpage.h"

#include "ui_mainpage.h"

MainPage::MainPage(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainPage) {
  ui->setupUi(this);

  connect(ui->startButton, &QPushButton::clicked, this, &MainPage::startClicked);
  connect(ui->lostButton, &QPushButton::clicked, this, &MainPage::lostClicked);
  connect(ui->endButton, &QPushButton::clicked, this, &MainPage::endClicked);
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
  ui->lostButton->setEnabled(running);
  ui->endButton->setEnabled(running);
}

void MainPage::showPresses(const QStringList& rows) {
  ui->timestampList->clear();
  for (const QString& row : rows) {
    ui->timestampList->addItem(row);
  }
}