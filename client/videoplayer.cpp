#include "videoplayer.h"

#include <QDesktopServices>
#include <QLabel>
#include <QStackedLayout>

#ifdef PERPLEXPING_HAS_WEBENGINE
#include <QWebEngineView>
#endif

VideoPlayer::VideoPlayer(QWidget* parent) : QWidget(parent) {
    stack = new QStackedLayout(this);

    message = new QLabel(this);
    message->setAlignment(Qt::AlignCenter);
    message->setWordWrap(true);
    message->setStyleSheet("background:#111; color:#ddd; padding:12px;");
    message->setMinimumHeight(180);
    stack->addWidget(message);

#ifdef PERPLEXPING_HAS_WEBENGINE
    web = new QWebEngineView(this);
    web->setMinimumHeight(240);
    stack->addWidget(web);
#endif
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void VideoPlayer::play(const QUrl& url) {
#ifdef PERPLEXPING_HAS_WEBENGINE
    web->load(url);
    stack->setCurrentWidget(web);
#else
    QDesktopServices::openUrl(url);
    message->setText("Opened the recording in your browser.");
    stack->setCurrentWidget(message);
#endif
}

void VideoPlayer::clear(const QString& text) {
#ifdef PERPLEXPING_HAS_WEBENGINE
    web->setUrl(QUrl("about:blank"));
#endif
    message->setText(text);
    stack->setCurrentWidget(message);
}
