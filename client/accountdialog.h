// VIEW: the Account Management window, opened from the NavBar's user menu.
// Change email, change password, delete the account (asks first). Each part
// shows its own error.

#pragma once

#include <QDialog>

#include "appcontroller.h"
#include "error.h"

class QLabel;
class QLineEdit;

class AccountDialog : public QDialog {
    Q_OBJECT

public:
    explicit AccountDialog(AppController* controller, QWidget* parent = nullptr);

    void showWindow();  // opens it fresh: current email, empty password boxes
    void refresh();     // fills the email box from getUser()
    bool showError(const QString& request, Error error);

private:
    void onNotice(AppController::Notice notice);
    QLabel* makeErrorLabel();
    void submitPasswordChange();
    void resetPasswordSection();  // empty boxes, no message
    void setPasswordMessage(const QString& text, bool isError);  // "" hides it

    AppController* controller;
    QLineEdit* emailEdit;
    QLabel* emailError;
    QLineEdit* currentPasswordEdit;
    QLineEdit* newPasswordEdit;
    QLineEdit* repeatPasswordEdit;
    QLabel* passwordMessage;        // the error, or "Password changed."
    QLineEdit* deletePasswordEdit;
    QLabel* deleteError;
};
