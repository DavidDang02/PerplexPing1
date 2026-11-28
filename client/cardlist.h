// VIEW: a list that shows its rows as cards in a grid, for My Courses and a
// course's sessions. It is a QListWidget, so selecting, double-clicking,
// Enter and the arrow keys work as in any list; only the drawing changes.
// A card tints when the mouse is over it, and strongly only while the mouse
// button is held down on it (so a quick click flashes). The selected card
// (what Rename / Delete act on) keeps a thick edge. Double-click or Enter
// opens it (itemActivated).
//
//   +------------------------------+  +------------------------------+
//   | Lecture 3          [Live now]|  | Lecture 2         [Published]|
//   | Nov 15, 2:15 PM              |  | Nov 8, 2:15 PM  ·  50 min    |
//   | Lost: 3  ·  Questions: 1     |  | Lost: 0  ·  Questions: 4     |
//   +------------------------------+  +------------------------------+
//
// Each row sets the roles below; its text (Qt::DisplayRole) stays the
// one-line summary, for tooltips and screen readers. As many cards fit side
// by side as the width allows (each at least kMinCardWidth wide), and the
// list scrolls up and down when there are more than fit.

#pragma once

#include <QListWidget>

class CardList : public QListWidget {
    Q_OBJECT

public:
    static constexpr int kTitleRole = Qt::UserRole + 10;   // QString, large and bold
    static constexpr int kDetailRole = Qt::UserRole + 11;  // QString, one line per '\n'
    static constexpr int kBadgeRole = Qt::UserRole + 12;   // QString, top right ("" = none)
    static constexpr int kAccentRole = Qt::UserRole + 13;  // bool: the badge is red (live)

    static constexpr int kMinCardWidth = 420;
    static constexpr int kSpacing = 10;  // around every card

    // `detailLines`: how many detail lines every card has room for, so all
    // cards are the same height and line up.
    explicit CardList(int detailLines, QWidget* parent = nullptr);

    int cardWidth() const { return cardWidth_; }
    int detailLines() const { return detailLines_; }
    int pressedRow() const { return pressedRow_; }  // -1 unless the mouse is down on a card

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void updateCardWidth();
    // Redraws the old and new pressed card. `anchor`: this is a new press,
    // so remember the card it started on.
    void setPressedRow(int row, bool anchor = true);

    int detailLines_;
    int cardWidth_ = kMinCardWidth;
    int pressedRow_ = -1;
    int pressAnchor_ = -1;  // the card the current press started on
};
