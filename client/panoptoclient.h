// CONTROLLER (external system): talks to SCU's Panopto for "Find
// recording". Runs in the APP, signed in as the educator (Panopto OAuth with
// PKCE in the browser: the app never sees their SCU password, and no client
// secret is ever in the app or on GitHub). The server never talks to
// Panopto and stores no Panopto token.
//
// This is the interface AppController uses. PanoptoOAuthClient
// (panoptooauthclient.h) is the real one; tests use a fake, so every flow
// is tested without a network or a Panopto account.
//
// Every call answers through its callback exactly once, on the event loop.

#pragma once

#include <functional>
#include <string>
#include <vector>

#include <QObject>
#include <QString>

#include "error.h"
#include "recording.h"

class PanoptoClient : public QObject {
    Q_OBJECT

public:
    using DoneHandler = std::function<void(Error error)>;
    // folderId is a Panopto folder GUID, folderName e.g.
    // "Fall 2026 Quarter - Computer Science 187-2 ... MWF 02:15 PM".
    using FolderHandler = std::function<void(Error error, const QString& folderId, const QString& folderName)>;
    using RecordingsHandler = std::function<void(Error error, const std::vector<RecordingCandidate>& recordings)>;

    explicit PanoptoClient(QObject* parent = nullptr) : QObject(parent) {}
    ~PanoptoClient() override = default;

    // False until signIn worked in this run of the app (or a saved token
    // is still good).
    virtual bool isSignedIn() const = 0;

    // Opens the SCU sign-in page in the browser and waits for Panopto to
    // send the user back. PanoptoSignInFailed if they cancel or it fails.
    virtual void signIn(DoneHandler done) = 0;

    // The folder that holds the lecture behind `lectureLink` (any Panopto
    // viewer link of the course). PanoptoLinkInvalid if it is not a Panopto
    // lecture link, PanoptoUnreachable if Panopto doesn't answer.
    virtual void findFolderOfLink(const QString& lectureLink, FolderHandler done) = 0;

    // The folder's recordings, newest first, each with its viewer link,
    // start time and length (Recording::kDefaultDurationSeconds when
    // Panopto has none yet).
    virtual void listRecordings(const QString& folderId, RecordingsHandler done) = 0;
};
