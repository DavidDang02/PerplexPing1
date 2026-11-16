# Requirements

The user should be able to…

## Manage their account
- Create an account with a username and password
- Log in to their account
- Log out

## Run a session
- Start a session
- Press "I'm Lost" during the session
- Write and send questions during the session
- See how many times they pressed "I'm Lost" and how many questions they sent in the running session
- End the session

## Review past sessions
- See a list of their previous sessions, newest first. Each row shows:
  - the date and time it started
  - how long it lasted (or "no end time" if the app was killed during it)
  - how many times they pressed "I'm Lost"
  - how many questions they sent
- Open a previous session and see:
  - its start and end time
  - every "I'm Lost" press and question, in order, each with how far into the session it happened (for example "12:34 in · I'm lost" or "05:40 in · Q: what is a foreign key?")

## Rules
| Rule | Value |
|---|---|
| Username length | 3–20 characters |
| Username characters | letters, digits, `.` `_` `-` (no spaces) |
| Username case | not case-sensitive: "Ana" and "ana" are the same account; it is shown as first typed |
| Username spaces at the ends | removed (" ana " is "ana") |
| Password length | 8–64 characters, any characters |
| Password case | case-sensitive |
| Login errors | never say whether the username or the password was wrong |
| Question length | 1–500 characters after removing spaces at the ends |
| Empty question | not saved; the user sees "Type a question before sending." |
| After a session ends | no more presses or questions can be added to it |
| Visibility | a user sees only their own sessions |
| One session at a time | a second session can't start while one is running |

Existing accounts created before these limits can still log in.
