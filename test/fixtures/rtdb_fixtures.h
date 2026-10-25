#pragma once

// RTDB stream payloads (the `data:` of each SSE event), shaped after the
// Firebase REST streaming protocol and the Cyberspace docs. Synthetic; never
// captured from the live service.

namespace fx {

static const char* kWindowPut = R"({"path":"/","data":{
  "-Nm1":{"userId":"u1","username":"tardibyte","content":"anyone else on the 4.0 beta yet?","timestamp":1719700000000,"isChatAdmin":false},
  "-Nm2":{"userId":"u2","username":"genghis_khan","content":"rolls","timestamp":1719700005000,"isDice":true,"style":"bold","gifUrl":"https://example.invalid/x.gif"}
}})";

static const char* kChildPut = R"({"path":"/-Nm3","data":
  {"userId":"u3","username":"oumike","content":"hello @tardibyte","timestamp":1719700010000}
})";

static const char* kDeletePatch = R"({"path":"/-Nm2","data":{"content":"[DELETED]","deleted":true}})";

static const char* kNullChildPut = R"({"path":"/-Nm1","data":null})";

static const char* kNullWindowPut = R"({"path":"/","data":null})";

static const char* kArtPut = R"({"path":"/-Nart","data":
  {"userId":"u1","username":"tardibyte","content":"IC9cXy9cCiggby5vICkKID4gXiA8","timestamp":1719700020000,"style":"art"}
})";

static const char* kStyleArrayPut = R"({"path":"/-Nsty","data":
  {"userId":"u2","username":"genghis_khan","content":"hi","timestamp":1719700030000,"style":["bold","italic"],
   "audioAttachment":{"src":"https://example.invalid/a.mp3","origin":"upload","cover":"c.jpg","artist":"A","title":"T","genre":"G"}}
})";

static const char* kWindowPatch = R"({"path":"/","data":{
  "-Nm1":{"content":"edited"},
  "-Nm9":{"userId":"u9","username":"newbie","content":"new","timestamp":1719700040000}
}})";

// now = 1,000,000 ms; staleAfter 180,000; idleAfter 600,000.
static const char* kPresenceWindow = R"({"path":"/","data":{
  "u1":{"username":"tardibyte","online":true,"isChatAdmin":false,"lastSeen":990000,"lastActivity":980000},
  "u2":{"username":"Genghis_Khan","online":true,"isChatAdmin":true,"lastSeen":995000,"lastActivity":300000},
  "u3":{"username":"stale_sam","online":true,"lastSeen":700000,"lastActivity":690000},
  "u4":{"username":"ann","online":true,"lastSeen":999000}
}})";

}  // namespace fx
