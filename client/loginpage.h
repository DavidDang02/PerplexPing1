// VIEW: the login page (a page inside MainWindow). Collects a username and password and announces the
// Log In and Create Account clicks as signals. Does not check credentials itself.

#pragma once

#include <QWidget>
#include <QString>

#include "error.h"

namespace Ui {
class LoginPage;
}

class LoginPage : public QWidget {
  Q_OBJECT

 public:
  explicit LoginPage(QWidget* parent = nullptr);
  ~LoginPage();

  QString username() const;
  QString password() const;
  QString email() const;  // only used by Create Account
  void clearFields();  // empties both fields and hides the error, e.g. after logging out

  // Shows the red error box with the sentence for `error`
  // (ViewText::getErrorText), e.g. "Wrong username or password."
  void showError(Error error);
  void clearError();  // hides the red error box

 signals:
  void loginClicked();
  void registerClicked();  // "Create Account"

 private:
  Ui::LoginPage* ui;
};
