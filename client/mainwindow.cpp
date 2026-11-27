#include "mainwindow.h"

#include <QVBoxLayout>

#include "loginpage.h"
#include "protocol.h"
#include "viewtext.h"

MainWindow::MainWindow(AppController* controller, QWidget* parent)
    : QMainWindow(parent), controller(controller) {
    setWindowTitle("PerplexPing");
    resize(1000, 700);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    banner = new QLabel(ViewText::getErrorText(Error::ServerUnreachable), central);
    banner->setAlignment(Qt::AlignCenter);
    banner->setStyleSheet("background:#fde7e9; color:#b00020; padding:8px; font-weight:600;");
    banner->setVisible(!controller->isConnected());
    layout->addWidget(banner);

    stack = new QStackedWidget(central);
    layout->addWidget(stack, 1);

    status = new QLabel(central);
    status->setStyleSheet("padding:6px 12px;");
    status->setWordWrap(true);
    layout->addWidget(status);
    setCentralWidget(central);

    loginPage = new LoginPage(stack);
    pages.insert(AppController::Page::Login, loginPage);
    stack->addWidget(loginPage);
    connect(loginPage, &LoginPage::loginClicked, controller, [this] {
        this->controller->logIn(loginPage->username(), loginPage->password());
    });
    connect(loginPage, &LoginPage::registerClicked, controller, [this] {
        this->controller->registerUser(loginPage->username(), loginPage->password(), loginPage->email());
    });

    using P = AppController::Page;
    for (const auto& [page, name] : std::initializer_list<std::pair<P, QString>>{
             {P::Courses, "My Courses"},
             {P::Course, "Course"},
             {P::StudentLive, "Live session (student)"},
             {P::EducatorLive, "Live session (educator)"},
             {P::SessionViewer, "Session viewer"}}) {
        QWidget* placeholder = makePlaceholder(name);
        pages.insert(page, placeholder);
        stack->addWidget(placeholder);
    }

    connect(controller, &AppController::pageChanged, this, &MainWindow::showPage);
    connect(controller, &AppController::connectionChanged, banner, [this](bool up) {
        banner->setVisible(!up);
    });
    connect(controller, &AppController::notice, this, [this](AppController::Notice notice) {
        status->setText(ViewText::getNoticeText(notice));
    });
    connect(controller, &AppController::requestFailed, this, [this](const QString& request, Error error) {
        if (request == Protocol::Request::kLogin || request == Protocol::Request::kRegister) {
            loginPage->showError(error);
            return;
        }
        status->setText(ViewText::getErrorText(error));
    });
    connect(controller, &AppController::stateChanged, this, [this] {
        if (this->controller->getPage() == AppController::Page::Login) {
            return;
        }
        if (const User* user = this->controller->getUser()) {
            setWindowTitle("PerplexPing: " + QString::fromStdString(user->getName()));
        }
    });

    showPage(controller->getPage());
}

void MainWindow::showPage(AppController::Page page) {
    if (QWidget* widget = pages.value(page)) {
        stack->setCurrentWidget(widget);
    }
    if (page == AppController::Page::Login) {
        loginPage->clearFields();
        setWindowTitle("PerplexPing");
    }
}

QWidget* MainWindow::makePlaceholder(const QString& name) {
    auto* widget = new QWidget(stack);
    auto* layout = new QVBoxLayout(widget);
    auto* label = new QLabel(name + "\n(page coming: built on AppController's signals)", widget);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("font-size:18px; color:palette(mid);");
    layout->addWidget(label);
    return widget;
}
