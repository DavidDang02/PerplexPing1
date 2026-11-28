#include "educatorlivepage.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "protocol.h"
#include "timelinewidget.h"
#include "uifonts.h"
#include "viewtext.h"

namespace Request = Protocol::Request;

EducatorLivePage::EducatorLivePage(AppController* controller, QWidget* parent)
    : BasePage(controller, parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);

    titleLabel = makeHeading("");
    layout->addWidget(titleLabel);

    codeLabel = new QLabel(this);
    codeLabel->setAlignment(Qt::AlignCenter);
    codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    codeLabel->setStyleSheet(QString("font-size:%1pt; font-weight:700; padding:12px;"
                                     " border:2px dashed palette(mid); border-radius:8px;")
                                 .arg(UiFont::kBrand));
    layout->addWidget(codeLabel);

    statusLabel = new QLabel(this);
    statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(statusLabel);

    layout->addWidget(makeHeading("Timeline so far", UiFont::kSubheading));
    timeline = new TimelineWidget(this);
    timeline->setClickable(false);  // nothing to play while it's live
    layout->addWidget(timeline);

    participantsLabel = makeHeading("", UiFont::kSubheading);
    layout->addWidget(participantsLabel);
    participantList = new QListWidget(this);
    layout->addWidget(participantList, 1);

    auto* endButton = new QPushButton("End Session…", this);
    endButton->setMinimumHeight(2 * endButton->sizeHint().height());
    endButton->setStyleSheet("font-weight:700;");
    layout->addWidget(endButton);
    endError = makeErrorLabel();
    layout->addWidget(endError);

    connect(endButton, &QPushButton::clicked, this, [this] {
        if (confirm("End session",
                    "End the session for everyone? Students can't press I'm Lost or ask "
                    "anything after this. You'll review it next, then publish it.")) {
            clearError(endError);
            this->controller->endSession();
        }
    });
    connect(controller, &AppController::participantsChanged, this,
            &EducatorLivePage::refreshParticipants);
}

void EducatorLivePage::refresh() {
    const auto& session = controller->getOpenSession();
    if (!session) {
        return;
    }
    titleLabel->setText(QString::fromStdString(session->getName()));
    codeLabel->setText("Session code: " + QString::fromStdString(session->getJoinCode()));
    statusLabel->setText(ViewText::formatLiveStatus(*session));

    const std::vector<Timeline::Bucket> buckets = controller->getTimeline();
    timeline->setBuckets(buckets, Timeline::getPeakIndex(buckets));
    refreshParticipants();
}

void EducatorLivePage::refreshParticipants() {
    const std::vector<Participant>& participants = controller->getParticipants();
    participantsLabel->setText(QString("Students here now: %1").arg(participants.size()));
    participantList->clear();
    for (const Participant& participant : participants) {
        // Named in every course: anonymous only hides names from classmates (C7).
        participantList->addItem(QString::fromStdString(participant.name));
    }
}

bool EducatorLivePage::showError(const QString& request, Error error) {
    if (request == Request::kEndSession) {
        setError(endError, error);
        return true;
    }
    return false;
}
