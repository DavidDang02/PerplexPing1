// VIEW: what every page after login has in common. A page:
//   * redraws itself from AppController's getters in refresh() (MainWindow
//     calls it on pageChanged and stateChanged; a page keeps no copy of
//     courses or sessions),
//   * calls a controller slot when the user does something,
//   * shows a failed request next to the box it came from (showError),
//   * asks before anything destructive (confirm).

#pragma once

#include <QLabel>
#include <QString>
#include <QWidget>

#include "appcontroller.h"
#include "error.h"
#include "uifonts.h"

class BasePage : public QWidget {
    Q_OBJECT

public:
    explicit BasePage(AppController* controller, QWidget* parent = nullptr);

    // Redraw everything from the controller's getters.
    virtual void refresh() = 0;

    // A request from this page failed. Show the error next to its box and
    // return true, or return false to let MainWindow show it in the status
    // line.
    virtual bool showError(const QString& request, Error error);

    // Hide every error label on the page (called when the page is shown).
    virtual void clearErrors();

protected:
    // A hidden red label for errors, styled like the login page's.
    QLabel* makeErrorLabel();
    static void setError(QLabel* label, Error error);
    static void clearError(QLabel* label);
    // A bold heading label (UiFont::kSubheading for a section inside a page).
    QLabel* makeHeading(const QString& text, int pointSize = UiFont::kHeading);
    // "Are you sure?" Yes/No box; true on Yes. Required before every
    // destructive slot (CONTRACT.md 7, step 6).
    bool confirm(const QString& title, const QString& text);

    AppController* controller;

private:
    QList<QLabel*> errorLabels;
};
