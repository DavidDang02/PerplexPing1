#include "accountdialog.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "protocol.h"
#include "uifonts.h"
#include "viewtext.h"

namespace Request = Protocol::Request;

AccountDialog::AccountDialog(AppController* controller, QWidget* parent)
    : QDialog(parent), controller(controller) {
    setWindowTitle("Account Management");
    setMinimumWidth(600);
    auto* layout = new QVBoxLayout(this);

    // ---- Change email ----
    auto* emailBox = new QGroupBox("School email", this);
    auto* emailForm = new QFormLayout(emailBox);
    emailEdit = new QLineEdit(emailBox);
    emailEdit->setPlaceholderText("you@scu.edu");
    auto* emailButton = new QPushButton("Update Email", emailBox);
    emailError = makeErrorLabel();
    emailForm->addRow("Email", emailEdit);
    emailForm->addRow(emailButton);
    emailForm->addRow(emailError);
    layout->addWidget(emailBox);

    // ---- Change password ----
    auto* passwordBox = new QGroupBox("Change password", this);
    auto* passwordForm = new QFormLayout(passwordBox);
    auto makePasswordEdit = [passwordBox] {
        auto* edit = new QLineEdit(passwordBox);
        edit->setEchoMode(QLineEdit::Password);
        return edit;
    };
    currentPasswordEdit = makePasswordEdit();
    newPasswordEdit = makePasswordEdit();
    repeatPasswordEdit = makePasswordEdit();
    auto* passwordButton = new QPushButton("Change Password", passwordBox);
    passwordMessage = new QLabel(passwordBox);
    passwordMessage->setWordWrap(true);
    passwordMessage->hide();
    passwordForm->addRow("Your current password", currentPasswordEdit);
    passwordForm->addRow("Your new password", newPasswordEdit);
    passwordForm->addRow("Your new password again", repeatPasswordEdit);
    passwordForm->addRow(passwordButton);
    passwordForm->addRow(passwordMessage);
    layout->addWidget(passwordBox);

    // ---- Delete account ----
    auto* deleteBox = new QGroupBox("Delete account", this);
    auto* deleteForm = new QFormLayout(deleteBox);
    auto* deleteNote = new QLabel(
        "This can't be undone. If you teach a course, delete it first.", deleteBox);
    deleteNote->setWordWrap(true);
    deletePasswordEdit = new QLineEdit(deleteBox);
    deletePasswordEdit->setEchoMode(QLineEdit::Password);
    auto* deleteButton = new QPushButton("Delete My Account…", deleteBox);
    deleteButton->setStyleSheet("color:#b00020;");
    deleteError = makeErrorLabel();
    deleteForm->addRow(deleteNote);
    deleteForm->addRow("Password", deletePasswordEdit);
    deleteForm->addRow(deleteButton);
    deleteForm->addRow(deleteError);
    layout->addWidget(deleteBox);

    auto* closeButton = new QPushButton("Close", this);
    layout->addWidget(closeButton, 0, Qt::AlignRight);

    connect(closeButton, &QPushButton::clicked, this, &QDialog::hide);
    connect(emailButton, &QPushButton::clicked, this, [this] {
        emailError->hide();
        this->controller->updateEmail(emailEdit->text());
    });
    connect(passwordButton, &QPushButton::clicked, this, &AccountDialog::submitPasswordChange);
    connect(repeatPasswordEdit, &QLineEdit::returnPressed, this,
            &AccountDialog::submitPasswordChange);
    connect(deleteButton, &QPushButton::clicked, this, [this] {
        deleteError->hide();
        if (QMessageBox::question(this, "Delete account",
                                  "Delete your account? Your questions and answers stay, "
                                  "shown as \"Deleted user\". This can't be undone.",
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) == QMessageBox::Yes) {
            this->controller->deleteAccount(deletePasswordEdit->text());
        }
    });
    connect(controller, &AppController::notice, this, &AccountDialog::onNotice);

    UiFont::styleSectionTitles(this);  // box titles as sub-headings
}

QLabel* AccountDialog::makeErrorLabel() {
    auto* label = new QLabel(this);
    label->setStyleSheet(
        "color: #b00020; background-color: #fde7e9; border: 1px solid #b00020;"
        " border-radius: 4px; padding: 6px;");
    label->setWordWrap(true);
    label->hide();
    return label;
}

void AccountDialog::showWindow() {
    emailEdit->clear();  // so refresh() fills it even if it had focus
    emailError->hide();
    deleteError->hide();
    deletePasswordEdit->clear();
    resetPasswordSection();
    refresh();
    show();
    raise();
    activateWindow();
}

void AccountDialog::refresh() {
    if (const User* user = controller->getUser()) {
        if (!emailEdit->hasFocus()) {
            emailEdit->setText(QString::fromStdString(user->getEmail()));
        }
    }
}

void AccountDialog::submitPasswordChange() {
    // Comparing the two new boxes is the only check made here; the server
    // decides everything else (wrong current password, too short, ...).
    if (newPasswordEdit->text() != repeatPasswordEdit->text()) {
        setPasswordMessage("The new passwords don't match.", true);
        repeatPasswordEdit->clear();
        repeatPasswordEdit->setFocus();
        return;
    }
    setPasswordMessage("", false);
    controller->changePassword(currentPasswordEdit->text(), newPasswordEdit->text());
}

void AccountDialog::resetPasswordSection() {
    currentPasswordEdit->clear();
    newPasswordEdit->clear();
    repeatPasswordEdit->clear();
    setPasswordMessage("", false);
}

void AccountDialog::setPasswordMessage(const QString& text, bool isError) {
    passwordMessage->setText(text);
    passwordMessage->setStyleSheet(
        isError ? "color: #b00020; background-color: #fde7e9; border: 1px solid #b00020;"
                  " border-radius: 4px; padding: 6px;"
                : "color: #1b5e20; background-color: #e8f5e9; border: 1px solid #2e7d32;"
                  " border-radius: 4px; padding: 6px;");
    passwordMessage->setVisible(!text.isEmpty());
}

bool AccountDialog::showError(const QString& request, Error error) {
    if (request == Request::kChangePassword && isVisible()) {
        setPasswordMessage(ViewText::getErrorText(error), true);
        return true;
    }
    QLabel* label = request == Request::kUpdateEmail    ? emailError
                    : request == Request::kDeleteAccount ? deleteError
                                                         : nullptr;
    if (label == nullptr || !isVisible()) {
        return false;
    }
    label->setText(ViewText::getErrorText(error));
    label->show();
    return true;
}

void AccountDialog::onNotice(AppController::Notice notice) {
    using N = AppController::Notice;
    if (notice == N::EmailUpdated) {
        emailError->hide();
        refresh();
    } else if (notice == N::PasswordChanged) {
        resetPasswordSection();
        setPasswordMessage(ViewText::getNoticeText(notice), false);
    } else if (notice == N::AccountDeleted || notice == N::LoggedOut ||
               notice == N::LoggedOutElsewhere) {
        deletePasswordEdit->clear();
        resetPasswordSection();
        emailError->hide();
        deleteError->hide();
        hide();
    }
}
