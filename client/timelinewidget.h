// VIEW: the confusion timeline. One bar per Timeline::Bucket (Lost pings in
// red, questions stacked on top in blue); the peak bar is outlined. Hovering
// a bar shows its numbers; clicking one emits barClicked with the seconds
// into the session where it starts (the page passes that to
// AppController::jumpTo). Draws only what it is handed.

#pragma once

#include <vector>

#include <QWidget>

#include "filters.h"

class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    // `peakIndex` from Timeline::getPeakIndex (-1 for none).
    void setBuckets(const std::vector<Timeline::Bucket>& buckets, int peakIndex);
    // false: no hand cursor, no clicks (the educator's live view).
    void setClickable(bool clickable);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void barClicked(long long sessionSeconds);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    int axisHeight() const;
    int barAt(int x) const;  // -1 if none
    QRectF plotArea() const;

    std::vector<Timeline::Bucket> buckets;
    int peakIndex = -1;
    bool clickable = true;
};
