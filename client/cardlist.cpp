#include "cardlist.h"

#include <QApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyle>
#include <QStyledItemDelegate>

#include "uifonts.h"

namespace {

constexpr int kPadding = 16;  // inside a card
constexpr int kGap = 6;       // between the title and the detail lines
constexpr qreal kRadius = 12;

QFont titleFont(const QFont& base) {
    QFont font = base;
    font.setPointSize(UiFont::kSubheading);
    font.setBold(true);
    return font;
}

QFont bodyFont(const QFont& base) {
    QFont font = base;
    font.setPointSize(UiFont::kBody);
    font.setBold(false);
    return font;
}

// Draws one card from the row's roles; the list decides where it goes.
class CardDelegate : public QStyledItemDelegate {
public:
    explicit CardDelegate(CardList* list) : QStyledItemDelegate(list), list(list) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override {
        const QFontMetrics title(titleFont(option.font));
        const QFontMetrics body(bodyFont(option.font));
        const int height = 2 * kPadding + title.height() + kGap +
                           list->detailLines() * body.lineSpacing();
        return {list->cardWidth(), height};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        const QPalette& palette = option.palette;
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool pressed = index.row() == list->pressedRow();
        const bool accent = index.data(CardList::kAccentRole).toBool();
        const QColor highlight = palette.color(QPalette::Highlight);
        const QColor live(0xd9, 0x30, 0x25);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        // ---- The card: filled, rounded, outlined ----
        // Plain: base colour, thin grey edge. Hovered: a light tint of the
        // highlight colour. Pressed (only while the mouse button is down): a
        // strong tint. Selected (the card Rename / Delete act on): a thick
        // highlight edge, which stays after the button is let go.
        const qreal border = (selected || pressed) ? 3 : (hovered ? 2 : 1);
        const QRectF card = QRectF(option.rect).adjusted(border / 2, border / 2,
                                                         -border / 2, -border / 2);
        QColor fill = palette.color(QPalette::Base);
        if (pressed) {
            fill = blend(fill, highlight, 0.34);
        } else if (hovered) {
            fill = blend(fill, highlight, 0.16);
        }
        QPainterPath path;
        path.addRoundedRect(card, kRadius, kRadius);
        painter->fillPath(path, fill);
        QColor edge = (selected || pressed) ? highlight
                    : hovered  ? blend(palette.color(QPalette::Mid), highlight, 0.7)
                    : accent   ? live
                               : palette.color(QPalette::Mid);
        painter->setPen(QPen(edge, border));
        painter->drawPath(path);

        const QRect inner = option.rect.adjusted(kPadding, kPadding, -kPadding, -kPadding);
        const QFont title = titleFont(option.font);
        const QFont body = bodyFont(option.font);
        const QFontMetrics titleMetrics(title);
        const QFontMetrics bodyMetrics(body);

        // ---- Badge, top right: "Live now" in red, others in grey ----
        int titleRight = inner.right();
        const QString badge = index.data(CardList::kBadgeRole).toString();
        if (!badge.isEmpty()) {
            const int badgeWidth = bodyMetrics.horizontalAdvance(badge) + 20;
            const int badgeHeight = bodyMetrics.height() + 4;
            const QRect badgeRect(inner.right() - badgeWidth + 1,
                                  inner.top() + (titleMetrics.height() - badgeHeight) / 2,
                                  badgeWidth, badgeHeight);
            const QColor badgeColor = accent ? live : palette.color(QPalette::Mid);
            painter->setPen(Qt::NoPen);
            painter->setBrush(accent ? live : blend(palette.color(QPalette::Base), badgeColor, 0.35));
            painter->drawRoundedRect(badgeRect, badgeHeight / 2.0, badgeHeight / 2.0);
            painter->setFont(body);
            painter->setPen(accent ? QColor(Qt::white) : palette.color(QPalette::Text));
            painter->drawText(badgeRect, Qt::AlignCenter, badge);
            titleRight = badgeRect.left() - 12;
        }

        // ---- Title, then the detail lines (cut short with "…" if too long) ----
        const QRect titleRect(inner.left(), inner.top(), titleRight - inner.left() + 1,
                              titleMetrics.height());
        painter->setFont(title);
        painter->setPen(palette.color(QPalette::Text));
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                          titleMetrics.elidedText(index.data(CardList::kTitleRole).toString(),
                                                  Qt::ElideRight, titleRect.width()));

        painter->setFont(body);
        painter->setPen(palette.color(QPalette::PlaceholderText));
        int y = titleRect.bottom() + 1 + kGap;
        const QStringList lines = index.data(CardList::kDetailRole).toString().split('\n');
        for (int i = 0; i < lines.size() && i < list->detailLines(); ++i) {
            const QRect lineRect(inner.left(), y, inner.width(), bodyMetrics.height());
            painter->drawText(lineRect, Qt::AlignLeft | Qt::AlignVCenter,
                              bodyMetrics.elidedText(lines[i], Qt::ElideRight, inner.width()));
            y += bodyMetrics.lineSpacing();
        }

        painter->restore();
    }

private:
    static QColor blend(const QColor& a, const QColor& b, double t) {
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                                a.greenF() + (b.greenF() - a.greenF()) * t,
                                a.blueF() + (b.blueF() - a.blueF()) * t);
    }

    CardList* list;
};

}  // namespace

CardList::CardList(int detailLines, QWidget* parent)
    : QListWidget(parent), detailLines_(detailLines) {
    setItemDelegate(new CardDelegate(this));
    // Cards left to right, wrapping into rows; scroll up and down only.
    setViewMode(QListView::ListMode);
    setFlow(QListView::LeftToRight);
    setWrapping(true);
    setResizeMode(QListView::Adjust);
    setMovement(QListView::Static);
    setSpacing(kSpacing);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    verticalScrollBar()->setSingleStep(24);
    // No box around the list: the cards sit on the page's background.
    setFrameShape(QFrame::NoFrame);
    viewport()->setBackgroundRole(QPalette::Window);
    viewport()->setAttribute(Qt::WA_Hover);
}

void CardList::mousePressEvent(QMouseEvent* event) {
    setPressedRow(indexAt(event->position().toPoint()).row());
    QListWidget::mousePressEvent(event);
}

void CardList::mouseDoubleClickEvent(QMouseEvent* event) {
    // The second press of a double-click: flash again.
    setPressedRow(indexAt(event->position().toPoint()).row());
    QListWidget::mouseDoubleClickEvent(event);
}

void CardList::mouseMoveEvent(QMouseEvent* event) {
    // Like a button: dragging off the card un-presses it, back on re-presses.
    if (event->buttons() & Qt::LeftButton && pressAnchor_ >= 0) {
        const int under = indexAt(event->position().toPoint()).row();
        setPressedRow(under == pressAnchor_ ? under : -1, false);
    }
    QListWidget::mouseMoveEvent(event);
}

void CardList::mouseReleaseEvent(QMouseEvent* event) {
    QListWidget::mouseReleaseEvent(event);
    setPressedRow(-1);
}

void CardList::leaveEvent(QEvent* event) {
    QListWidget::leaveEvent(event);
    if (!(QGuiApplication::mouseButtons() & Qt::LeftButton)) {
        setPressedRow(-1);
    }
}

void CardList::setPressedRow(int row, bool anchor) {
    if (anchor) {
        pressAnchor_ = row;
    }
    if (row == pressedRow_) {
        return;
    }
    const int old = pressedRow_;
    pressedRow_ = row;
    for (int r : {old, row}) {
        if (QListWidgetItem* it = item(r)) {
            viewport()->update(visualItemRect(it));
        }
    }
}

void CardList::resizeEvent(QResizeEvent* event) {
    QListWidget::resizeEvent(event);
    updateCardWidth();
}

void CardList::updateCardWidth() {
    // Leave room for the scroll bar whether or not it shows, so it appearing
    // doesn't change the number of columns (and back again).
    // (A couple of pixels spare, so rounding never pushes the last card of a
    // row onto the next.)
    const int scrollBar = verticalScrollBar()->isVisible()
                              ? 0
                              : style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr,
                                                     verticalScrollBar());
    const int available = viewport()->width() - scrollBar - 2;
    // QListView leaves kSpacing on each side of every card, so n cards in a
    // row take n * (width + 2 * kSpacing).
    const int columns = std::max(1, available / (kMinCardWidth + 2 * kSpacing));
    const int width = std::max(kMinCardWidth / 2, available / columns - 2 * kSpacing);
    if (width != cardWidth_) {
        cardWidth_ = width;
        scheduleDelayedItemsLayout();
    }
}
