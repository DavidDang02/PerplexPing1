# Requirements

PerplexPing lets students tell their instructor, during a lecture, when they're lost, without raising a hand. Afterwards the instructor sees **where** the lecture lost people on a timeline, answers the questions, and lines the timeline up with the lecture recording on Panopto, so a click on a spike plays that moment of the video.

There are no "teacher accounts". Whoever creates a course is that course's **educator**; everyone who joins it is a **student** of it. The same account can teach one course and take another.

## What users can do

### Accounts (everyone)
- Create an account with a username, password and SCU email.
- Log in, log out. Be logged in on a laptop and a phone at once; both stay in sync.
- Change the password (needs the current one), change the email.
- Delete the account (needs the password). Every other device logged in as you is logged out.

### Courses
- Create a course (you become its educator). It gets a 6-character **course code**.
- Join a course by typing its code (you become a student of it).
- See **My Courses**: every course you teach or take.
- Educator: rename the course, switch **Anonymous** and **Questions shared** on or off, see the course code, delete the course (after a confirmation).

### A lecture (session)
- Educator: start a session in the course. It gets a 6-digit **session code** to put on the board.
- Student: join it from the course page or by typing the session code.
- Student, while live: press **I'm Lost**; ask a question; see, edit and delete their own presses and questions. Nobody else's are shown.
- Educator, while live: see the counts, the timeline filling in, and who is connected. The live view is read-only.
- Educator: **End** the session (after a confirmation). Students' input is frozen and they go back to the course page.

### Review and publish (after the session)
- Educator, in review (students can't see the session yet): look at the timeline and its peak minute, answer questions, remove questions, attach the recording, rename or delete the session, then **Publish**.
- Everyone, once published: see the timeline, the questions (all of them if the course shares questions, otherwise only their own), and the answers; add answers; edit and delete their own answers.
- Click a moment on the timeline or a question: the recording plays from just before that moment.
- Search the course's sessions by name; search the questions and answers of a session; show only unanswered questions.

### The recording (educator)
- **One-time setup per course:** sign in to Panopto with SCU sign-in, paste any lecture link of the course; the app finds the course's Panopto folder and shows its name.
- **Find recording:** after a lecture, one button finds that lecture's recording in the folder and lines it up automatically. Panopto posts recordings some time after class, so if it isn't there yet the app says so and the educator tries again later.
- **Fallback:** paste the recording link and type where the session starts in the video (e.g. `4:30`).

## Rules

Text is trimmed (spaces, tabs and line breaks at both ends) before it is checked or saved, except passwords. Lengths count characters, not bytes. The **server** checks every rule; the app only shows the result. Each rule has an id that the code's comments use.

### Accounts (U)
| # | Rule | Error |
|---|---|---|
| U1 | Username: 3–20 characters from `A-Z a-z 0-9 . _ -` | EmptyName, NameTooShort, NameTooLong, NameBadCharacters |
| U2 | Usernames are unique **ignoring case** ("Ana" = "ana"), shown as first typed. **Passwords are case-sensitive.** | NameTaken |
| U3 | Password: 8–64 characters, any characters, never trimmed | EmptyPassword, PasswordTooShort, PasswordTooLong |
| U4 | Passwords are stored only as salted PBKDF2-HMAC-SHA256 hashes, never in plain text, never logged | – |
| U5 | A failed login never says whether the name or the password was wrong | WrongLogin |
| U6 | Email is required: ≤254 characters, one `@`, no spaces, ends in `@scu.edu`, unique ignoring case | EmptyEmail, EmailInvalid, EmailNotSchool, EmailTaken |
| U7 | Changing the password needs the current one | WrongPassword |
| U8 | Deleting the account needs the password and is refused while you are running a live session or still teach a course (delete your courses first, so none is left without an educator). Your presses, questions and answers stay, shown as "Deleted user". | WrongPassword, HostingLiveSession, TeachesCourse |
| U9 | Everything except sign-up and log-in needs a login | NotLoggedIn |

### Courses (C)
| # | Rule | Error |
|---|---|---|
| C1 | Anyone logged in can create a course; the creator is its one educator | – |
| C2 | Course name: 1–60 characters (need not be unique) | EmptyCourseName, CourseNameTooLong |
| C3 | Course code: 6 characters from `A-Z 2-9` without `0 O 1 I`, unique | – |
| C4 | Joining with a code makes you a student (spaces at the ends and lower case are forgiven). Joining twice changes nothing. | NoSuchCourseCode |
| C5 | Someone not in a course can't see it or anything in it; they get the same answer as for something that doesn't exist | NoSuchCourse, NoSuchSession |
| C6 | Only the course's educator changes it, runs its sessions, or deletes it | NotCourseEducator |
| C7 | **Anonymous** (default off) hides each student's name from the other students; the educator still sees every name. **Questions shared** (default on) lets students see each other's questions once a session is published. Changes apply at once to every session. | – |
| C8 | Only the educator sees the course code and the Panopto folder | – |

### Sessions (S)
| # | Rule | Error |
|---|---|---|
| S1 | Only the educator starts a session | NotCourseEducator |
| S2 | Session name: 1–60 characters | EmptySessionName, SessionNameTooLong |
| S3 | At most one live session per course | CourseHasLiveSession |
| S4 | A live session's 6-digit code is shown only to the educator and stops working when it ends | – |
| S5 | A session code works only while that session is live | NoSuchSessionCode |
| S6 | Only members of the course open its sessions | NoSuchSession |
| S7 | Only the educator ends a live session; it goes to **in review**. Closing the app never ends a session. | SessionEnded |
| S8 | The educator renames a session at any time | S2 errors |
| S9 | The educator deletes a session (with everything in it) once it has ended; anyone who has it open is moved to the course page | SessionNotEnded |
| S10 | Every time is the **server's** clock, to the second | – |
| S11 | Status only moves live → in review → published. Publish is the educator's, from in review, with or without a recording. No un-publish. | SessionNotEnded, SessionAlreadyPublished |
| S12 | Students can't open a session in review; the list shows it as a placeholder with no counts | SessionNotPublished |

### Lost presses and questions (I)
| # | Rule | Error |
|---|---|---|
| I1 | Only students send them, never the educator | NotAStudent |
| I2 | Only while the session is live | SessionEnded |
| I3 | Question: 1–500 characters | EmptyQuestion, QuestionTooLong |
| I4 | A student edits their own question while live | NotYourInput |
| I5 | A student deletes their own press or question while live | NotYourInput |
| I6 | Frozen when the session ends | SessionEnded |
| I7 | After it ends, the educator can remove any question (not while live) | SessionNotEnded |
| I8 | One I'm Lost press per student every 5 seconds | LostTooSoon |
| I9 | Removing a question removes its answers | – |

### Answers (A)
| # | Rule | Error |
|---|---|---|
| A1 | Nobody answers while live. In review: the educator. Published: everyone in the course. | SessionNotEnded |
| A2 | You can only answer a question you can see | NoSuchInput |
| A3 | Answer: 1–1000 characters; only its author edits it | EmptyAnswer, AnswerTooLong, NotYourAnswer |
| A4 | A question can have any number of answers | – |
| A5 | The educator's answers come first, then oldest first | – |
| A7 | An answer is deleted by its author or by the educator | NotYourAnswer |

### Who sees whose name (V)
| # | Rule |
|---|---|
| V1 | While live, a student sees only their own presses and questions |
| V2 | I'm Lost presses never show a name (except your own) |
| V3 | Questions and student answers show names in a named course. In an anonymous course, other students see "Anonymous" while the educator still sees the name; the educator's answers always show the name |
| V4 | A hidden name never leaves the server (it is removed per person before sending) |
| V5 | Only the educator sees who is connected to a live session, with names, in every course |

### Recording (R)
| # | Rule | Error |
|---|---|---|
| R1 | Only the educator attaches, replaces or removes it, once the session has ended | NotCourseEducator, SessionNotEnded |
| R2 | Link: 1–2048 characters, no spaces, starts with `https://` or `http://` | RecordingUrlInvalid |
| R3 | "Session starts at" is `m:ss`, `h:mm:ss` or seconds, may be negative, at most 24 h | RecordingOffsetInvalid |
| R4 | A moment opens the video 10 seconds before it, never before 0:00 | – |
| R6 | Find recording picks the folder's recording whose time overlaps the session; none yet → nothing changes, try again later. A match more than 24 h off is refused. SCU's Panopto quirks are handled: times are local, a missing length counts as 70 min. | RecordingNotReady, RecordingOffsetInvalid, PanoptoFolderNotLinked |
| R7 | Panopto sign-in happens in the browser (the app never sees the password). The app uses no client secret, keeps the token in memory only, and never sends it to our server. | PanoptoSignInFailed, PanoptoNotConfigured |

### Network (N)
| # | Rule | Error |
|---|---|---|
| N1 | Every message is one JSON object with a `type`; unknown types, missing or wrong fields are refused, never guessed | BadRequest, UnknownRequest |
| N2 | Every request gets exactly one reply | – |
| N3 | If the server is down or doesn't answer within 10 s, the app says so, keeps trying every 2 s, and logs back in by itself when it is back | ServerUnreachable |

## Limits at a glance
| | Min | Max |
|---|---|---|
| Username | 3 | 20 |
| Password | 8 | 64 |
| Email | – | 254 |
| Course / session name | 1 | 60 |
| Question | 1 | 500 |
| Answer | 1 | 1000 |
| Recording link | 1 | 2048 |
| Time between Lost presses | 5 s | – |
| Recording offset | −24 h | +24 h |

## Not in this version (future work)
- Several educators or TAs per course.
- Email confirmation and password reset.
- Changing a username.
- Automatic recording matching without the educator pressing Find recording.
