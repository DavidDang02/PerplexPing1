#include "navbar.h"

#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>

#include "accountdialog.h"
#include "protocol.h"
#include "viewtext.h"
#include "uifonts.h"

namespace {

constexpr int kIconSize = 44;  // in step with 20 pt text

// A blank pixmap for drawing an icon, sharp on high-DPI screens.
QPixmap blankIcon(qreal dpr) {
    QPixmap pixmap(QSize(kIconSize, kIconSize) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

// The standard "account" icon: a head and shoulders inside a circle.
QIcon makeUserIcon(const QColor& color, qreal dpr) {
    QPixmap pixmap = blankIcon(dpr);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF circle(1.5, 1.5, kIconSize - 3, kIconSize - 3);
    p.setPen(QPen(color, 1.8));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(circle);

    QPainterPath clip;
    clip.addEllipse(circle);
    p.setClipPath(clip);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(QPointF(kIconSize / 2.0, kIconSize * 0.40), kIconSize * 0.17, kIconSize * 0.17);
    p.drawEllipse(QPointF(kIconSize / 2.0, kIconSize * 0.98), kIconSize * 0.34, kIconSize * 0.30);
    return QIcon(pixmap);
}

// A left arrow, for going back.
QIcon makeBackIcon(const QColor& color, qreal dpr) {
    QPixmap pixmap = blankIcon(dpr);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal s = kIconSize;
    p.setPen(QPen(color, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(QPointF(s * 0.24, s * 0.50), QPointF(s * 0.78, s * 0.50));
    QPainterPath head;
    head.moveTo(s * 0.46, s * 0.26);
    head.lineTo(s * 0.22, s * 0.50);
    head.lineTo(s * 0.46, s * 0.74);
    p.drawPath(head);
    return QIcon(pixmap);
}

// A bell, with a red badge holding the unread count (none when 0).
QIcon makeBellIcon(const QColor& color, int unread, qreal dpr) {
    QPixmap pixmap = blankIcon(dpr);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal s = kIconSize;

    QPainterPath bell;
    bell.moveTo(s * 0.20, s * 0.74);
    bell.cubicTo(s * 0.28, s * 0.64, s * 0.27, s * 0.56, s * 0.27, s * 0.44);
    bell.cubicTo(s * 0.27, s * 0.24, s * 0.38, s * 0.15, s * 0.50, s * 0.15);
    bell.cubicTo(s * 0.62, s * 0.15, s * 0.73, s * 0.24, s * 0.73, s * 0.44);
    bell.cubicTo(s * 0.73, s * 0.56, s * 0.72, s * 0.64, s * 0.80, s * 0.74);
    bell.closeSubpath();
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(bell);
    p.drawLine(QPointF(s * 0.50, s * 0.08), QPointF(s * 0.50, s * 0.15));  // the hook
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(QPointF(s * 0.50, s * 0.84), s * 0.08, s * 0.08);      // the clapper

    if (unread > 0) {
        const QString text = unread > 9 ? QStringLiteral("9+") : QString::number(unread);
        // The count is drawn at UiFont::kBody (1 pt = 1 px on screen), like all text.
        const qreal h = UiFont::kBody + 6;
        const qreal w = std::max(h, h * 0.6 * text.size() + 6);
        const QRectF badge(s - w, 0, w, h);
        p.setBrush(QColor("#d32f2f"));
        p.drawRoundedRect(badge, badge.height() / 2, badge.height() / 2);
        QFont font = p.font();
        font.setPixelSize(UiFont::kBody);
        font.setBold(true);
        p.setFont(font);
        p.setPen(Qt::white);
        p.drawText(badge, Qt::AlignCenter, text);
    }
    return QIcon(pixmap);
}

QToolButton* makeIconButton(const QString& tip, QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setAutoRaise(true);  // flat until hovered, like other apps' toolbars
    button->setIconSize(QSize(kIconSize, kIconSize));
    button->setToolTip(tip);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::TabFocus);
    return button;
}

}  // namespace

NavBar::NavBar(AppController* controller, QWidget* parent)
    : QFrame(parent), controller(controller) {
    setObjectName("NavBar");
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(true);

    // The bar's row, then a 1 px line separating it from the page below.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto* layout = new QHBoxLayout;
    layout->setContentsMargins(16, 6, 12, 6);
    layout->setSpacing(4);
    outer->addLayout(layout);
    auto* separator = new QFrame(this);
    separator->setObjectName("NavBarSeparator");
    separator->setFixedHeight(1);
    separator->setStyleSheet("#NavBarSeparator { background-color: palette(mid); border: none; }");
    outer->addWidget(separator);

    // ---- Left: the app's name, which goes home ----
    brandButton = new QPushButton("PerplexPing", this);
    brandButton->setFlat(true);
    brandButton->setCursor(Qt::PointingHandCursor);
    brandButton->setToolTip("My Courses");
    brandButton->setFocusPolicy(Qt::TabFocus);
    QFont brandFont = brandButton->font();
    brandFont.setPointSize(UiFont::kBrand);
    brandFont.setBold(true);
    brandButton->setFont(brandFont);
    brandButton->setStyleSheet("QPushButton { border: none; padding: 2px 4px; text-align: left; }"
                               "QPushButton:hover { color: palette(highlight); }");
    // Then the back arrow, on every page but My Courses.
    backButton = makeIconButton("Back", this);
    auto* left = new QHBoxLayout;
    left->setSpacing(8);
    left->addWidget(brandButton);
    left->addWidget(backButton);
    left->addStretch();

    // ---- Middle: "Educator View" / "Student View" inside a course ----
    viewLabel = new QLabel(this);
    QFont viewFont = viewLabel->font();
    viewFont.setPointSize(UiFont::kSubheading);
    viewFont.setBold(true);
    viewLabel->setFont(viewFont);
    viewLabel->setAlignment(Qt::AlignCenter);

    // ---- Right: bell, then user ----
    bellButton = makeIconButton("Notifications", this);
    userButton = makeIconButton("Account", this);
    auto* right = new QHBoxLayout;
    right->addStretch();
    right->addWidget(bellButton);
    right->addWidget(userButton);

    // Three columns, the outer two always the same width, so the middle
    // label sits at the bar's centre whatever is on either side.
    layout->addLayout(left, 1);
    layout->addWidget(viewLabel);
    layout->addLayout(right, 1);

    // ---- Bell menu ----
    bellPopup = makePopup();
    bellPopup->setFixedWidth(520);
    auto* bellLayout = new QVBoxLayout(bellPopup);
    bellLayout->setContentsMargins(12, 10, 12, 10);
    auto* bellHeader = new QHBoxLayout;
    auto* bellTitle = new QLabel("Notifications", bellPopup);
    QFont titleFont = bellTitle->font();
    titleFont.setPointSize(UiFont::kSubheading);
    titleFont.setBold(true);
    bellTitle->setFont(titleFont);
    clearButton = new QPushButton("Clear all", bellPopup);
    clearButton->setFlat(true);
    clearButton->setCursor(Qt::PointingHandCursor);
    bellHeader->addWidget(bellTitle);
    bellHeader->addStretch();
    bellHeader->addWidget(clearButton);
    bellLayout->addLayout(bellHeader);
    notificationList = new QListWidget(bellPopup);
    notificationList->setWordWrap(true);
    notificationList->setFrameShape(QFrame::NoFrame);
    notificationList->setFixedHeight(420);
    notificationList->setCursor(Qt::PointingHandCursor);
    notificationList->setStyleSheet("QListWidget::item { padding: 6px 2px;"
                                    " border-bottom: 1px solid palette(midlight); }");
    bellLayout->addWidget(notificationList);
    noNotificationsLabel = new QLabel("No notifications yet.", bellPopup);
    noNotificationsLabel->setAlignment(Qt::AlignCenter);
    noNotificationsLabel->setStyleSheet("color: palette(placeholder-text); padding: 24px;");
    bellLayout->addWidget(noNotificationsLabel);

    // ---- User menu ----
    userPopup = makePopup();
    userPopup->setMinimumWidth(340);
    auto* userLayout = new QVBoxLayout(userPopup);
    userLayout->setContentsMargins(16, 14, 16, 14);
    userLayout->setSpacing(4);
    nameLabel = new QLabel(userPopup);
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(UiFont::kSubheading);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    emailLabel = new QLabel(userPopup);
    emailLabel->setStyleSheet("color: palette(placeholder-text);");
    emailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    userLayout->addWidget(nameLabel);
    userLayout->addWidget(emailLabel);
    auto addLine = [this, userLayout] {
        auto* line = new QFrame(userPopup);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        userLayout->addSpacing(8);
        userLayout->addWidget(line);
        userLayout->addSpacing(8);
    };
    addLine();

    // ---- Account Management (email, password, delete), between two lines ----
    accountButton = new QPushButton("Account Management", userPopup);
    accountButton->setCursor(Qt::PointingHandCursor);
    userLayout->addWidget(accountButton);
    accountDialog = new AccountDialog(controller, this);
    addLine();
    logoutButton = new QPushButton("Log Out", userPopup);
    logoutButton->setCursor(Qt::PointingHandCursor);
    logoutButton->setStyleSheet(
        "QPushButton { background-color: #d32f2f; color: white; font-weight: 600;"
        " border: none; border-radius: 6px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: #b71c1c; }"
        "QPushButton:pressed { background-color: #8e1414; }");
    userLayout->addWidget(logoutButton);

    // ---- Wiring ----
    connect(brandButton, &QPushButton::clicked, controller, &AppController::goHome);
    connect(backButton, &QToolButton::clicked, this, &NavBar::goBack);
    connect(bellButton, &QToolButton::clicked, this, [this] {
        refreshNotifications();
        showPopup(bellPopup, bellButton);
    });
    connect(userButton, &QToolButton::clicked, this, [this] {
        refresh();
        showPopup(userPopup, userButton);
    });
    connect(accountButton, &QPushButton::clicked, this, [this] {
        userPopup->hide();
        accountDialog->showWindow();
    });
    connect(logoutButton, &QPushButton::clicked, this, [this] {
        userPopup->hide();
        this->controller->logOut();
    });
    connect(clearButton, &QPushButton::clicked, controller, &AppController::clearNotifications);
    connect(notificationList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const int index = notificationList->row(item);
        bellPopup->hide();
        this->controller->openNotification(index);
    });
    connect(controller, &AppController::notificationsChanged, this, [this] {
        updateIcons();
        if (bellPopup->isVisible()) {
            refreshNotifications();
        }
    });
    connect(controller, &AppController::stateChanged, this, &NavBar::refresh);

    updateIcons();
    refreshNotifications();
}

QFrame* NavBar::makePopup() {
    // A Qt::Popup closes by itself on a click anywhere else, like a menu.
    auto* popup = new QFrame(this, Qt::Popup);
    popup->setFrameShape(QFrame::StyledPanel);
    popup->setAttribute(Qt::WA_NoMouseReplay);  // clicking the icon again just closes it
    popup->installEventFilter(this);
    return popup;
}

void NavBar::showPopup(QFrame* popup, QWidget* anchor) {
    popup->adjustSize();
    // Right edge under the icon's right edge, kept on screen.
    QPoint pos = anchor->mapToGlobal(QPoint(anchor->width() - popup->width(), anchor->height() + 4));
    if (const QScreen* screen = anchor->screen()) {
        const QRect area = screen->availableGeometry();
        pos.setX(std::max(area.left(), std::min(pos.x(), area.right() - popup->width())));
    }
    popup->move(pos);
    popup->show();
}

bool NavBar::showError(const QString& request, Error error) {
    return accountDialog->showError(request, error);
}

bool NavBar::eventFilter(QObject* watched, QEvent* event) {
    // Notifications count as read once the menu closes (so the unread ones
    // stay bold while it is open).
    if (watched == bellPopup && event->type() == QEvent::Hide) {
        controller->markNotificationsRead();
    }
    return QFrame::eventFilter(watched, event);
}

void NavBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        updateIcons();
    }
    QFrame::changeEvent(event);
}

void NavBar::goBack() {
    using Page = AppController::Page;
    switch (controller->getPage()) {
        case Page::Course:
            controller->goHome();
            break;
        case Page::StudentLive:
        case Page::EducatorLive:  // the session keeps running
        case Page::SessionViewer:
            controller->leaveSession();
            break;
        default:
            break;
    }
}

void NavBar::refreshPageControls() {
    using Page = AppController::Page;
    const Page page = controller->getPage();
    // Where the arrow goes, as its tooltip ("" = nowhere: hidden).
    QString back;
    switch (page) {
        case Page::Course:        back = "Back to My Courses"; break;
        case Page::StudentLive:   back = "Leave session"; break;
        case Page::EducatorLive:
        case Page::SessionViewer: back = "Back to course"; break;
        default: break;
    }
    backButton->setToolTip(back);
    backButton->setVisible(!back.isEmpty());

    const bool inCourse = page != Page::Login && page != Page::Courses;
    viewLabel->setText(inCourse ? ViewText::formatRoleView(controller->getRole()) : QString());
}

void NavBar::refresh() {
    refreshPageControls();
    if (const User* user = controller->getUser()) {
        nameLabel->setText(QString::fromStdString(user->getName()));
        emailLabel->setText(QString::fromStdString(user->getEmail()));
        if (accountDialog->isVisible()) {
            accountDialog->refresh();
        }
    } else {
        nameLabel->clear();
        emailLabel->clear();
        accountDialog->hide();
        userPopup->hide();
        bellPopup->hide();
    }
}

void NavBar::refreshNotifications() {
    const std::vector<AppNotification>& notifications = controller->getNotifications();
    notificationList->clear();
    for (const AppNotification& n : notifications) {
        auto* item = new QListWidgetItem(
            ViewText::formatNotification(n) + "\n" + ViewText::formatTimeAgo(n.time), notificationList);
        if (!n.read) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
        }
    }
    const bool empty = notifications.empty();
    notificationList->setVisible(!empty);
    noNotificationsLabel->setVisible(empty);
    clearButton->setEnabled(!empty);
    bellPopup->adjustSize();
}

void NavBar::updateIcons() {
    if (!bellButton || !userButton || !backButton) {
        return;  // still being built
    }
    const QColor color = palette().color(QPalette::WindowText);
    const qreal dpr = devicePixelRatioF();
    const int unread = controller->getUnreadCount();
    bellButton->setIcon(makeBellIcon(color, unread, dpr));
    bellButton->setToolTip(unread > 0 ? QString("Notifications (%1 unread)").arg(unread)
                                      : QString("Notifications"));
    userButton->setIcon(makeUserIcon(color, dpr));
    backButton->setIcon(makeBackIcon(color, dpr));
}
