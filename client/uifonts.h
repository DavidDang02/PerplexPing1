// VIEW: the app's font sizes, in points, in one place. kBody is the
// smallest text anywhere in the app; everything else is bigger.
// applyTo() makes kBody the default for every widget, dialog and tooltip,
// so only text that should be larger sets a size.

#pragma once

#include <QApplication>
#include <QFont>
#include <QGroupBox>
#include <QToolTip>

namespace UiFont {

constexpr int kBody = 20;        // labels, buttons, boxes, lists, hints, tooltips
constexpr int kSubheading = 22;  // bold: "Sessions", "Questions", ...
constexpr int kHeading = 26;     // bold: page titles ("My Courses", a course's name)
constexpr int kLarge = 28;       // the I'm Lost button
constexpr int kBrand = 32;       // "PerplexPing" in the nav bar and on the login page

inline void applyTo(QApplication& app) {
    QFont font = app.font();
    font.setPointSize(kBody);
    QApplication::setFont(font);
    QToolTip::setFont(font);
}

// Makes every titled box (QGroupBox) under `root` show its title like a
// sub-heading (kSubheading, bold) while what's inside stays body text. Qt
// draws a box's title in the box's own font, which its children would
// otherwise inherit, so the children get the body font back. Call it at the
// end of a constructor, once the boxes and their contents exist.
inline void styleSectionTitles(QWidget* root) {
    QFont title = QApplication::font();
    title.setPointSize(kSubheading);
    title.setBold(true);
    QFont body = QApplication::font();
    body.setPointSize(kBody);  // set both explicitly, or the children would
    body.setBold(false);       // still inherit the title's size and weight
    for (QGroupBox* box : root->findChildren<QGroupBox*>()) {
        if (box->title().isEmpty()) {
            continue;
        }
        box->setFont(title);
        for (QWidget* child : box->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
            const auto* inner = qobject_cast<QGroupBox*>(child);
            if (!inner || inner->title().isEmpty()) {
                child->setFont(body);  // a titled inner box gets its own turn
            }
        }
    }
}

}  // namespace UiFont
