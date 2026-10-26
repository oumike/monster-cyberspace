#pragma once

// REST response bodies shaped after api.cyberspace.online/docs and
// /types.d.ts. Synthetic values; never captured from the live service.

namespace fx {

static const char* kFeedPage = R"({"data":[
  {"postId":"p1","authorId":"u1","authorUsername":"tardibyte","content":"first entry","repliesCount":2,"bookmarksCount":0,"isPublic":true,"deleted":false,"createdAt":"2026-03-27T10:12:01.516Z"},
  {"postId":"p2","authorId":"u2","authorUsername":"genghis_khan","content":"# heading\nbody","title":"On spiders","slug":"on-spiders","topics":["music","linux"],"repliesCount":0,"bookmarksCount":1,"isNSFW":true,"contentWarning":"spiders","deleted":false,"createdAt":"2026-03-27T09:00:00.000Z"},
  {"postId":"p3","authorId":"u3","authorUsername":"oumike","content":"guild post","repliesCount":5,"bookmarksCount":0,"deleted":false,"createdAt":"2026-03-26T23:59:59Z","isGuildThread":true,"guildSlug":"synths"}
],"cursor":"p3"})";

static const char* kFeedLast = R"({"data":[
  {"postId":"p0","authorId":"u1","authorUsername":"tardibyte","content":"oldest","repliesCount":0,"bookmarksCount":0,"deleted":false,"createdAt":"2026-01-01T00:00:00.000Z"}
],"cursor":null})";

static const char* kEntry = R"({"data":{
  "postId":"p5","authorId":"u2","authorUsername":"genghis_khan","content":"listen to this","title":"Tune","slug":"tune",
  "repliesCount":1,"bookmarksCount":0,"isPublic":true,"deleted":false,
  "createdAt":"2026-03-27T10:12:01.516Z","editedAt":"2026-03-27T11:00:00.000Z",
  "attachments":[{"type":"audio","src":"https://bunker.cyberspace.online/a.mp3","artist":"Boards","title":"Roygbiv"},{"type":"image","src":"https://bunker.cyberspace.online/i.png"}]
}})";

static const char* kEntrySparse = R"({"data":{"postId":"p9","authorId":"u","authorUsername":"x","content":"","repliesCount":0,"bookmarksCount":0,"deleted":true,"createdAt":"2026-03-27T10:12:01.516Z","attachments":[{"type":"video","src":"s"}]}})";

static const char* kReplies = R"({"data":[
  {"replyId":"r1","postId":"p1","authorId":"u2","authorUsername":"genghis_khan","content":"nice","deleted":false,"createdAt":"2026-03-27T10:20:00.000Z"},
  {"replyId":"r2","postId":"p1","authorId":"u3","authorUsername":"oumike","content":"agreed","deleted":false,"createdAt":"2026-03-27T10:21:00.000Z","editedAt":"2026-03-27T10:22:00.000Z"},
  {"replyId":"r3","postId":"p1","parentReplyId":"r1","parentReplyAuthor":"tardibyte","authorId":"u1","authorUsername":"tardibyte","content":"thanks","deleted":false,"createdAt":"2026-03-27T10:23:00.000Z","attachments":[{"type":"audio","src":"s","title":"Only title"}]}
],"cursor":"r3"})";

static const char* kReply = R"({"data":{"replyId":"r7","postId":"p4","authorId":"u2","authorUsername":"genghis_khan","content":"a reply","deleted":false,"createdAt":"2026-03-27T10:30:00.000Z"}})";

static const char* kCreatedPost = R"({"data":{"postId":"p10","slug":"hello"}})";

static const char* kCreatedReply = R"({"data":{"replyId":"r11"}})";

static const char* kCreatedBookmark = R"({"data":{"bookmarkId":"b12"}})";

static const char* kBookmarks = R"({"data":[
  {"bookmarkId":"b1","postId":"p1","type":"post","createdAt":"2026-03-27T10:00:00.000Z"},
  {"bookmarkId":"b2","replyId":"r2","type":"reply","createdAt":"2026-03-27T10:05:00.000Z"}
],"cursor":null})";

static const char* kWatches = R"({"data":[
  {"id":"w1","postId":"p1","createdAt":"2026-03-27T10:00:00.000Z"},
  {"id":"w2","postId":"p5","createdAt":"2026-03-27T10:01:00.000Z"}
],"cursor":"w2"})";

static const char* kWatching = R"({"data":{"watching":true}})";

}  // namespace fx
