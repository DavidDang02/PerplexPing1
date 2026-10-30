// VIEW: the login window. Collects a username and password and announces the
// Log In click as a signal. Does not check credentials itself.

#pragma once

#include <QMainWindow>
#include <QString>

namespace Ui {
class LoginPage;
}

class LoginPage : public QMainWindow {
  Q_OBJECT

 public:
  explicit LoginPage(QWidget* parent = nullptr);
  ~LoginPage();

  QString username() const;
  QString password() const;
  void clearFields();  // empties both fields, e.g. after logging out

 signals:
  void loginClicked();

 private:
  Ui::LoginPage* ui;
};
