#include "basepage.h"

#include <QFont>
#include <QMessageBox>

#include "viewtext.h"

BasePage::BasePage(AppController* controller, QWidget* parent)
    : QWidget(parent), controller(controller) {}

bool BasePage::showError(const QString& /*request*/, Error /*error*/) {
    return false;
}

void BasePage::clearErrors() {
    for (QLabel* label : errorLabels) {
        clearError(label);
    }
}

QLabel* BasePage::makeErrorLabel() {
    auto* label = new QLabel(this);
    label->setStyleSheet(
        "color: #b00020; background-color: #fde7e9; border: 1px solid #b00020;"
        " border-radius: 4px; padding: 6px;");
    label->setWordWrap(true);
    label->hide();
    errorLabels.append(label);
    return label;
}

void BasePage::setError(QLabel* label, Error error) {
    label->setText(ViewText::getErrorText(error));
    label->setVisible(error != Error::None);
}

void BasePage::clearError(QLabel* label) {
    label->clear();
    label->hide();
}

QLabel* BasePage::makeHeading(const QString& text, int pointSize) {
    auto* label = new QLabel(text, this);
    QFont font = label->font();
    font.setPointSize(pointSize);
    font.setBold(true);
    label->setFont(font);
    return label;
}

bool BasePage::confirm(const QString& title, const QString& text) {
    return QMessageBox::question(this, title, text, QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) == QMessageBox::Yes;
}
