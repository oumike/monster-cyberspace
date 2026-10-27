# Platy Cyberspace — milestone 2: C-Mail on the ThinkNode M9

Date: 2026-10-25 · Status: approved direction ("cmail"), builds on milestone 1

Milestone 1's spec (`2026-10-25-cyberspace-m9-circ-design.md`) still applies:
the constraints in §2, the architecture in §4, and the error handling in §9.
This document only covers what C-Mail adds.

## 1. Intent

C-Mail is Cyberspace's private 1:1 messaging. The device can:

1. List your conversations: unread first, with the unread count.
2. Open one, read its history, and receive new messages live.
3. Send messages and `/commands`, with the same banners and *sending…* line as cIRC.
4. Start a conversation by username.
5. Show when the other person is typing, and tell them when you are.
6. Alert you (banner and buzzer) to new C-Mail while you're elsewhere, even
   with the screen off.

There is no delete or report in C-Mail: the API has neither.

## 2. API (from api.cyberspace.online/docs)

| Use | Call |
|---|---|
| List | `GET /v1/cmail`: `conversationId`, `otherUser{userId, username}`, `lastMessage`, `lastMessageAt` (ms), `unreadCount` |
| Start or get | `POST /v1/cmail {recipientUsername}`: `{conversationId, otherUser}` (200 or 201) |
| History | `GET /v1/cmail/:id?limit=50&before=<cursor>`: oldest first, `senderId`/`senderUsername` |
| Send | `POST /v1/cmail/:id {content}`: `{messageId}` (201); `403` when blocked |
| Mark read | `POST /v1/cmail/:id/read` |
| Typing | `POST /v1/cmail/:id/typing`: `{heartbeatMs, staleAfterMs}`; `DELETE` clears it |
| Live | SSE on `dm_messages/<id>` (`orderBy="timestamp"`, `limitToLast` ≤ 100) |
| Live typing | SSE on `dm_presence/<id>`: `{<userId>: {username, typing, timestamp}}` |

**Client-side limits:**

- Message: 15/min, 150/h, 1000/day.
- Start a conversation: 5/min, 30/h, 50/day.
- Mark read: 60/min.
- Typing: 40/min.

`/art` and the `/mute` family are cIRC-only (they return `400`), so the
C-Mail palette leaves them out.

## 3. Controls

- **Fn under Home** opens the **C-Mail list**. In a conversation it returns
  to the list.
- **List:**
  - The first entry is **New conversation…**: type a username and press
    Centre, and the device starts or opens it with `POST /v1/cmail`.
  - Each conversation shows `@name`, a preview of the last message, and its
    unread count. Unread conversations are bold.
- **Conversation:** works like a cIRC room (spec M1 §5), with these
  differences:
  - There is no drawer.
  - Centre on an empty line opens a menu with Mark read, Text size and
    Close.
  - The message menu has Reveal, Show full and Show link.
  - The status bar shows `@name`. While the other person is typing, the hint
    row (Compact) or the status title (Comfortable) shows "name is typing…".

## 4. Data flow

- **Opening a conversation:** history, then the `dm_messages` stream (resuming
  at `startAfter`), then the `dm_presence` stream for typing. Mark read on
  open and while the newest message is on screen (every 10 s at most, as in
  rooms). Closing the conversation or turning the screen off stops both
  streams.
- **Typing (a write, so only from key presses):**
  - Typing a character into a non-empty line sends `POST …/typing` at most
    once per `heartbeatMs`.
  - Emptying the line (Backspace or a held Del) or leaving with text still in
    it sends `DELETE …/typing`.
  - Sending clears the flag on the server.
  - No timer sends anything, so an idle flag simply goes stale after
    `staleAfterMs`.
- **Watcher** (every 60 s, alongside rooms and mentions): `GET /v1/cmail`
  refreshes the list.
  - Alerts: each conversation with `unreadCount > 0` and a `lastMessageAt`
    newer than its last alert raises one banner, "C-Mail from @name:
    preview", plus the buzzer. The open conversation is skipped.
  - That makes 3 reads per minute in total (limit 30/min each).
- **Home** shows "c-mail N unread", summed from the list.

## 5. Implementation shape

- A conversation is a *channel* like a room. `ChatState` gains `dm` (the open
  channel is a conversation), `conversations`, `typing` and `typingCadence`.
  The chat store, layout, log, input, menus and viewers are shared.
- `RoomScreen` takes a kind: the router owns one instance for rooms and one
  for conversations.
- The net worker keeps one open channel. For a conversation, presence calls
  are skipped and the second SSE slot carries `dm_presence`.
- `mergeMessageJson` accepts `senderId`/`senderUsername` as aliases of
  `userId`/`username`.
- The `m9_fake` build scripts three conversations. A partner "types" and then
  replies after you send, and an unread DM arrives every 45 s while that
  conversation is closed.

## 6. Testing

- **Host:**
  - list, start and typing-ack parsing;
  - the sender aliases;
  - typing stream merging and the stale filter;
  - the C-Mail limiters;
  - the C-Mail palette filter.
- **`m9_fake`:** list, open, send, typing indicator, alerts, new conversation.
- **Live (human):** see `docs/TESTING.md`, C-Mail section.
