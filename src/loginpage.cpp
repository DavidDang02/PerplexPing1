#include "loginpage.h"
#include "ui_loginpage.h"

LoginPage::LoginPage(QWidget* parent) : QMainWindow(parent), ui(new Ui::LoginPage) {
  ui->setupUi(this);

  connect(ui->loginButton, &QPushButton::clicked, this, &LoginPage::loginClicked);
  // Pressing Enter in the password field also logs in.
  connect(ui->passwordEdit, &QLineEdit::returnPressed, this, &LoginPage::loginClicked);
}

LoginPage::~LoginPage() {
  delete ui;
}

QString LoginPage::username() const {
  return ui->usernameEdit->text();
}

QString LoginPage::password() const {
  return ui->passwordEdit->text();
}

void LoginPage::clearFields() {
  ui->usernameEdit->clear();
  ui->passwordEdit->clear();
  ui->usernameEdit->setFocus();
}
