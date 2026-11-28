#include "timelinewidget.h"

#include <algorithm>

#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

#include "viewtext.h"

namespace {
const QColor kLostColor(0xd9, 0x48, 0x41);
const QColor kQuestionColor(0x3b, 0x7d, 0xd8);
const QColor kPeakOutline(0x20, 0x20, 0x20);
}  // namespace

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void TimelineWidget::setBuckets(const std::vector<Timeline::Bucket>& newBuckets, int newPeak) {
    buckets = newBuckets;
    peakIndex = newPeak;
    update();
}

void TimelineWidget::setClickable(bool value) {
    clickable = value;
    setCursor(clickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

QSize TimelineWidget::sizeHint() const {
    return {400, 120 + axisHeight()};
}

QSize TimelineWidget::minimumSizeHint() const {
    return {150, 80 + axisHeight()};
}

int TimelineWidget::axisHeight() const {
    return fontMetrics().height() + 2;  // the 0:00 ... 50:00 labels under the bars
}

QRectF TimelineWidget::plotArea() const {
    return QRectF(4, 4, width() - 8, height() - 8 - axisHeight());
}

int TimelineWidget::barAt(int x) const {
    const QRectF area = plotArea();
    if (buckets.empty() || x < area.left() || x >= area.right()) {
        return -1;
    }
    const int index = static_cast<int>((x - area.left()) / (area.width() / buckets.size()));
    return std::clamp(index, 0, static_cast<int>(buckets.size()) - 1);
}

void TimelineWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF area = plotArea();

    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(area.bottomLeft(), area.bottomRight());

    if (buckets.empty()) {
        painter.drawText(area, Qt::AlignCenter, "No activity yet");
        return;
    }

    int highest = 1;
    for (const Timeline::Bucket& bucket : buckets) {
        highest = std::max(highest, bucket.lostCount + bucket.questionCount);
    }

    const double slot = area.width() / buckets.size();
    const double gap = std::min(2.0, slot * 0.15);
    for (std::size_t i = 0; i < buckets.size(); ++i) {
        const Timeline::Bucket& bucket = buckets[i];
        const double x = area.left() + i * slot + gap / 2;
        const double w = std::max(1.0, slot - gap);
        const double lostH = area.height() * bucket.lostCount / highest;
        const double questionH = area.height() * bucket.questionCount / highest;

        painter.setPen(Qt::NoPen);
        painter.setBrush(kLostColor);
        painter.drawRect(QRectF(x, area.bottom() - lostH, w, lostH));
        painter.setBrush(kQuestionColor);
        painter.drawRect(QRectF(x, area.bottom() - lostH - questionH, w, questionH));

        if (static_cast<int>(i) == peakIndex) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(kPeakOutline, 2));
            painter.drawRect(QRectF(x, area.top(), w, area.height()));
        }
    }

    // Axis: start and end times under the bars.
    painter.setPen(palette().color(QPalette::Text));
    const QRectF axis(area.left(), area.bottom() + 2, area.width(), axisHeight());
    const long long width = buckets.size() > 1
                                ? buckets[1].startSeconds - buckets[0].startSeconds
                                : Timeline::kBucketSeconds;
    painter.drawText(axis, Qt::AlignLeft | Qt::AlignVCenter,
                     ViewText::formatOffset(buckets.front().startSeconds));
    painter.drawText(axis, Qt::AlignRight | Qt::AlignVCenter,
                     ViewText::formatOffset(buckets.back().startSeconds + width));
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event) {
    const int index = barAt(static_cast<int>(event->position().x()));
    if (index < 0) {
        QToolTip::hideText();
        return;
    }
    const long long width = buckets.size() > 1
                                ? buckets[1].startSeconds - buckets[0].startSeconds
                                : Timeline::kBucketSeconds;
    QToolTip::showText(event->globalPosition().toPoint(),
                       ViewText::formatBucket(buckets[index], width, index == peakIndex), this);
}

void TimelineWidget::mousePressEvent(QMouseEvent* event) {
    if (!clickable || event->button() != Qt::LeftButton) {
        return;
    }
    const int index = barAt(static_cast<int>(event->position().x()));
    if (index >= 0) {
        emit barClicked(buckets[index].startSeconds);
    }
}
