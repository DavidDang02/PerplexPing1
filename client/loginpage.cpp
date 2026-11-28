#include "loginpage.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "viewtext.h"
#include "uifonts.h"

LoginPage::LoginPage(QWidget* parent) : QWidget(parent) {
  // A column in the middle of the page: stretch | form | stretch.
  auto* outerLayout = new QHBoxLayout(this);
  auto* formPanel = new QWidget(this);
  formPanel->setMinimumWidth(640);
  formPanel->setMaximumWidth(760);
  outerLayout->addStretch();
  outerLayout->addWidget(formPanel);
  outerLayout->addStretch();

  // Inside it, top to bottom, centred vertically.
  auto* mainLayout = new QVBoxLayout(formPanel);
  mainLayout->addStretch();

  titleLabel = new QLabel("PerplexPing", formPanel);
  titleLabel->setAlignment(Qt::AlignCenter);
  QFont titleFont = titleLabel->font();
  titleFont.setPointSize(UiFont::kBrand);
  titleFont.setBold(true);
  titleLabel->setFont(titleFont);
  mainLayout->addWidget(titleLabel);

  auto* formLayout = new QFormLayout;
  formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  usernameEdit = new QLineEdit(formPanel);
  usernameEdit->setPlaceholderText("Username");
  passwordEdit = new QLineEdit(formPanel);
  passwordEdit->setEchoMode(QLineEdit::Password);
  passwordEdit->setPlaceholderText("Password");
  emailEdit = new QLineEdit(formPanel);
  emailEdit->setPlaceholderText("you@scu.edu (only for Create Account)");
  formLayout->addRow("Username", usernameEdit);
  formLayout->addRow("Password", passwordEdit);
  formLayout->addRow("School email", emailEdit);
  mainLayout->addLayout(formLayout);

  errorLabel = new QLabel(formPanel);
  errorLabel->setStyleSheet(
      "color: #b00020; background-color: #fde7e9; border: 1px solid #b00020;"
      " border-radius: 4px; padding: 6px;");
  errorLabel->setAlignment(Qt::AlignCenter);
  errorLabel->setWordWrap(true);
  errorLabel->hide();
  mainLayout->addWidget(errorLabel);

  loginButton = new QPushButton("Log In", formPanel);
  registerButton = new QPushButton("Create Account", formPanel);
  mainLayout->addWidget(loginButton);
  mainLayout->addWidget(registerButton);
  mainLayout->addStretch();

  connect(loginButton, &QPushButton::clicked, this, &LoginPage::loginClicked);
  connect(registerButton, &QPushButton::clicked, this, &LoginPage::registerClicked);
  // Pressing Enter in the password field also logs in.
  connect(passwordEdit, &QLineEdit::returnPressed, this, &LoginPage::loginClicked);
}

QString LoginPage::username() const {
  return usernameEdit->text();
}

QString LoginPage::password() const {
  return passwordEdit->text();
}

QString LoginPage::email() const {
  return emailEdit->text();
}

void LoginPage::clearFields() {
  usernameEdit->clear();
  passwordEdit->clear();
  emailEdit->clear();
  clearError();
  usernameEdit->setFocus();
}

void LoginPage::showError(Error error) {
  errorLabel->setText(ViewText::getErrorText(error));
  errorLabel->show();
}

void LoginPage::clearError() {
  errorLabel->clear();
  errorLabel->hide();
}
