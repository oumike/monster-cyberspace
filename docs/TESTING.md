# Testing

## Host unit tests

```sh
pio test -e native
```

These cover the portable logic in `lib/core`:

- response parsing, from fixtures written from the API docs
- the SSE parser and the put/patch merge
- the chat store, wrapping and the room layout
- nick colours, the rate limiter and token timing
- the keymap, art decoding, QR codes and web helpers

They never touch the network.

## Fake-API firmware (`m9_fake`)

```sh
scripts/build-upload-monitor.sh m9_fake
```

The network task is replaced by a scripted source, so no account or Wi-Fi is
needed. The web server does not run in this build.

The fake source provides:

- the rooms `general`, `linux` and `music`, with fixture history and a new
  message every 4 s;
- 12 people, 3 of them idle;
- sends that echo back after 300 ms;
- a `400` for `/dice x`;
- the real local send limit, so the 16th fast send shows the rate-limit
  banner.

Check every screen and key:

- both text sizes, and the drawer in each
- the palette and the room, message and person menus
- spoilers and art
- a QR code on a link
- the error banners
- screen off, the screensaver and wake

## Live checklist (human only)

Agents and scripts must never call the live Cyberspace API. A person runs
this checklist on the `m9` build with their own account.

- [ ] Web login: sign in from the web page; Home shows the account
- [ ] Join a room; history loads and new messages stream in
- [ ] Send a plain message, a `/me` action, and a bad `/dice` (an error banner, and the text stays in the input line)
- [ ] Scroll back past the first page of history
- [ ] Open the drawer and see yourself in it
- [ ] Turn the screen off, mention yourself from the website, and hear the buzzer
- [ ] Delete one of your own messages; the website shows the tombstone
- [ ] Turn Wi-Fi off and back on: ✕, then ● again, with no lost or duplicated messages
- [ ] Leave it running more than 70 minutes so it crosses a token refresh; chat keeps working
