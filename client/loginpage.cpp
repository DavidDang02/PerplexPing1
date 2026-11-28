#include "loginpage.h"
#include "ui_loginpage.h"

#include "viewtext.h"
#include "uifonts.h"

LoginPage::LoginPage(QWidget* parent) : QWidget(parent), ui(new Ui::LoginPage) {
  ui->setupUi(this);
  QFont titleFont = ui->titleLabel->font();
  titleFont.setPointSize(UiFont::kBrand);
  titleFont.setBold(true);
  ui->titleLabel->setFont(titleFont);

  connect(ui->loginButton, &QPushButton::clicked, this, &LoginPage::loginClicked);
  connect(ui->registerButton, &QPushButton::clicked, this, &LoginPage::registerClicked);
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

QString LoginPage::email() const {
  return ui->emailEdit->text();
}

void LoginPage::clearFields() {
  ui->usernameEdit->clear();
  ui->passwordEdit->clear();
  ui->emailEdit->clear();
  clearError();
  ui->usernameEdit->setFocus();
}

void LoginPage::showError(Error error) {
  ui->errorLabel->setText(ViewText::getErrorText(error));
  ui->errorLabel->show();
}

void LoginPage::clearError() {
  ui->errorLabel->clear();
  ui->errorLabel->hide();
}
