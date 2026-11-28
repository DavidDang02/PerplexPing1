#include "mainwindow.h"

#include <QVBoxLayout>

#include "coursepage.h"
#include "coursespage.h"
#include "educatorlivepage.h"
#include "loginpage.h"
#include "navbar.h"
#include "protocol.h"
#include "sessionviewerpage.h"
#include "studentlivepage.h"
#include "viewtext.h"

namespace {
// How long the app may be disconnected before the banner shows. Connecting
// at startup takes a moment; without this the banner flashes every launch.
constexpr int kBannerDelayMs = 3000;
}  // namespace

MainWindow::MainWindow(AppController* controller, QWidget* parent)
    : QMainWindow(parent), controller(controller) {
    setWindowTitle("PerplexPing");
    resize(1440, 960);  // room for 20 pt text

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    navBar = new NavBar(controller, central);
    layout->addWidget(navBar);

    banner = new QLabel(ViewText::getErrorText(Error::ServerUnreachable), central);
    banner->setAlignment(Qt::AlignCenter);
    banner->setStyleSheet("background:#fde7e9; color:#b00020; padding:8px; font-weight:600;");
    banner->hide();  // shown only if still disconnected after kBannerDelayMs
    layout->addWidget(banner);
    bannerDelay = new QTimer(this);
    bannerDelay->setSingleShot(true);
    bannerDelay->setInterval(kBannerDelayMs);
    connect(bannerDelay, &QTimer::timeout, banner, [this] {
        banner->setVisible(!this->controller->isConnected());
    });
    if (!controller->isConnected()) {
        bannerDelay->start();  // the first connection is usually up well before this
    }

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
    viewerPage = new SessionViewerPage(controller, stack);
    for (const auto& [page, widget] : std::initializer_list<std::pair<P, BasePage*>>{
             {P::Courses, new CoursesPage(controller, stack)},
             {P::Course, new CoursePage(controller, stack)},
             {P::StudentLive, new StudentLivePage(controller, stack)},
             {P::EducatorLive, new EducatorLivePage(controller, stack)},
             {P::SessionViewer, viewerPage}}) {
        pages.insert(page, widget);
        stack->addWidget(widget);
    }
    connect(controller, &AppController::playRecording, viewerPage, &SessionViewerPage::play);

    connect(controller, &AppController::pageChanged, this, &MainWindow::showPage);
    connect(controller, &AppController::connectionChanged, banner, [this](bool up) {
        if (up) {
            bannerDelay->stop();
            banner->hide();
        } else if (!banner->isVisible() && !bannerDelay->isActive()) {
            bannerDelay->start();
        }
    });
    connect(controller, &AppController::notice, this, [this](AppController::Notice notice) {
        status->setText(ViewText::getNoticeText(notice));
    });
    connect(controller, &AppController::requestFailed, this, [this](const QString& request, Error error) {
        if (request == Protocol::Request::kLogin || request == Protocol::Request::kRegister) {
            loginPage->showError(error);
            return;
        }
        if (navBar->showError(request, error)) {  // sent from Account Management
            status->clear();
            return;
        }
        if (BasePage* page = currentPage(); page && page->showError(request, error)) {
            status->clear();
            return;
        }
        status->setText(ViewText::getErrorText(error));
    });
    connect(controller, &AppController::stateChanged, this, [this] {
        if (this->controller->getPage() == AppController::Page::Login) {
            return;
        }
        if (BasePage* page = currentPage()) {
            page->refresh();
        }
        if (const User* user = this->controller->getUser()) {
            setWindowTitle("PerplexPing: " + QString::fromStdString(user->getName()));
        }
    });

    showPage(controller->getPage());
}

void MainWindow::showPage(AppController::Page page) {
    if (QWidget* widget = pages.value(page)) {
        if (auto* basePage = qobject_cast<BasePage*>(widget)) {
            basePage->clearErrors();
            basePage->refresh();
        }
        stack->setCurrentWidget(widget);
    }
    navBar->setVisible(page != AppController::Page::Login);
    navBar->refresh();
    if (page == AppController::Page::Login) {
        loginPage->clearFields();
        setWindowTitle("PerplexPing");
    }
}

BasePage* MainWindow::currentPage() const {
    return qobject_cast<BasePage*>(stack->currentWidget());
}
