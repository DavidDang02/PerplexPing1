// VIEW: the navigation bar across the top of every page after login
// (MainWindow shows it above the BasePages and hides it on the login page).
//
//   PerplexPing [<-]          Educator View          [bell]  [user]
//
//   * "PerplexPing" goes home to My Courses (AppController::goHome).
//   * The back arrow does what the page's back button used to: My Courses
//     from a course, the course from a session (leaveSession). Hidden on
//     My Courses.
//   * In the middle, "Educator View" or "Student View" inside a course
//     (and its sessions); nothing on My Courses.
//   * The bell opens the notifications menu: students joining (educator)
//     and sessions being published (student), newest first. A red badge
//     counts the unread ones; they count as read once the menu closes.
//     Clicking one goes to its course or session.
//   * The user icon opens the account menu: name, email, an Account
//     Management button (opens AccountDialog: email, password, delete
//     account) and a red Log Out.
//
// Like the pages, it keeps no copy of anything: it redraws from
// AppController's getters on stateChanged / notificationsChanged.

#pragma once

#include <QFrame>

#include "appcontroller.h"

class AccountDialog;
class QLabel;
class QListWidget;
class QPushButton;
class QToolButton;

class NavBar : public QFrame {
    Q_OBJECT

public:
    explicit NavBar(AppController* controller, QWidget* parent = nullptr);

    void refresh();  // the user menu and the bell badge
    // A failed request sent from Account Management (email, password,
    // delete account): shows it there and returns true; false otherwise.
    bool showError(const QString& request, Error error);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;  // redraw the icons when the palette changes

private:
    QFrame* makePopup();
    void showPopup(QFrame* popup, QWidget* anchor);
    void refreshNotifications();
    void refreshPageControls();  // the back arrow and the role in the middle
    void goBack();
    void updateIcons();

    AppController* controller;

    QPushButton* brandButton;
    QToolButton* backButton = nullptr;
    QLabel* viewLabel;
    QToolButton* bellButton = nullptr;  // null until the constructor makes them:
    QToolButton* userButton = nullptr;  // setStyleSheet calls changeEvent first

    // Bell menu.
    QFrame* bellPopup;
    QListWidget* notificationList;
    QLabel* noNotificationsLabel;
    QPushButton* clearButton;

    // User menu.
    QFrame* userPopup;
    QLabel* nameLabel;
    QLabel* emailLabel;
    QPushButton* accountButton;     // "Account Management"
    AccountDialog* accountDialog;
    QPushButton* logoutButton;
};
