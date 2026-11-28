// VIEW: plays a Panopto link (AppController::playRecording). With Qt
// WebEngine installed (CMake sets PERPLEXPING_HAS_WEBENGINE) it loads the
// full Panopto viewer page inside the app, as in the Nov 20 video spike.
// Without it (e.g. CI, which installs only qt6-base), it opens the link in
// the default browser instead, so the app still builds everywhere.

#pragma once

#include <QUrl>
#include <QWidget>

class QLabel;
class QStackedLayout;

#ifdef PERPLEXPING_HAS_WEBENGINE
class QWebEngineView;
#endif

class VideoPlayer : public QWidget {
    Q_OBJECT

public:
    explicit VideoPlayer(QWidget* parent = nullptr);

    void play(const QUrl& url);
    // Back to the "nothing playing" message (e.g. another session opened).
    void clear(const QString& message);

private:
    QStackedLayout* stack;
    QLabel* message;
#ifdef PERPLEXPING_HAS_WEBENGINE
    QWebEngineView* web;
#endif
};
