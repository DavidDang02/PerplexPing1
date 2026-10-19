#include "mainpage.h"
#include "ui_mainpage.h"

#include <QDateTime>

MainPage::MainPage(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainPage) {
    ui->setupUi(this);

    connect(ui->startButton, &QPushButton::clicked, this, &MainPage::createSession);
    connect(ui->lostButton, &QPushButton::clicked, this, &MainPage::clickButton);
    connect(ui->endButton, &QPushButton::clicked, this, &MainPage::endSession);

    refreshView();
}

MainPage::~MainPage() {
    delete session;
    delete ui;
}

// ---- Controller: forward each click to the model, then refresh the view ----

void MainPage::createSession() {
    delete session;  // a new session replaces the old one
    session = new Session();
    refreshView();
}

void MainPage::clickButton() {
    if (session != nullptr) {
        session->buttonClicked();
    }
    refreshView();
}

void MainPage::endSession() {
    if (session != nullptr) {
        session->endSession();
    }
    refreshView();
}

// ---- View: show the model's current state on the widgets ----

// Formats seconds since the start as "mm:ss in", e.g. 754 -> "12:34 in".
static QString formatOffset(long long seconds) {
    return QString("%1:%2 in")
        .arg(seconds / 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
}

// Formats a real (wall clock) time as e.g. "2:47:05 PM".
static QString formatClockTime(Session::Clock::time_point time) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        time.time_since_epoch());
    return QDateTime::fromMSecsSinceEpoch(ms.count()).toString("h:mm:ss AP");
}

void MainPage::refreshView() {
    const bool running = session != nullptr && !session->isEnded();

    // "I'm lost" and End only work during a session; Start only outside one.
    ui->startButton->setEnabled(!running);
    ui->lostButton->setEnabled(running);
    ui->endButton->setEnabled(running);

    ui->timestampList->clear();

    if (session == nullptr) {
        ui->statusLabel->setText("No session");
        ui->startLabel->setText("Started: —");
        ui->endLabel->setText("Ended: —");
        return;
    }

    ui->startLabel->setText("Started: " + formatClockTime(session->getStartTime()));

    if (running) {
        ui->statusLabel->setText(
            QString("Session running, %1 presses").arg(session->pressCount()));
        ui->endLabel->setText("Ended: —");
        return;
    }

    ui->statusLabel->setText(
        QString("Session ended, %1 presses").arg(session->pressCount()));
    ui->endLabel->setText("Ended: " + formatClockTime(session->getEndTime()));

    // Each press as "mm:ss in" plus its wall clock time, e.g. "12:34 in (2:47:05 PM)".
    const std::vector<long long> offsets = session->pressOffsetsSeconds();
    const std::vector<Session::Clock::time_point> &times = session->getButtonTimestamps();
    for (size_t i = 0; i < offsets.size(); ++i) {
        ui->timestampList->addItem(
            formatOffset(offsets[i]) + " (" + formatClockTime(times[i]) + ")");
    }
}
