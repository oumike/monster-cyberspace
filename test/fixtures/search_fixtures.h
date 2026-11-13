// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Search response bodies shaped after api.cyberspace.online/docs.md
// (Search). Synthetic values; never captured from the live service.

namespace fx {

static const char* kSearchAll = R"({"data":{
  "users":[{"type":"user","userId":"u1","username":"neon","displayName":"Neon Rider","followersCount":4}],
  "posts":[
    {"type":"post","postId":"p1","authorId":"u1","authorUsername":"neon","title":"Neon nights",
     "content":"\n\n**Bright** lights\nsecond line","repliesCount":5,"createdAt":"2026-03-27T10:12:01.516Z"},
    {"type":"post","postId":"p2","authorUsername":"ada","content":"no title here"},
    {"type":"post","authorUsername":"x","content":"no id: skipped"}
  ],
  "replies":[{"type":"reply","replyId":"r1","postId":"p1","authorUsername":"bob",
              "content":"I like *neon* too","parentPostAuthor":"neon","createdAt":"2026-03-28T00:00:00Z"}]
}})";

// No replies group at all.
static const char* kSearchAllNoReplies = R"({"data":{"users":[],"posts":[
  {"type":"post","postId":"p3","authorUsername":"cy","content":"hi"}]}})";

static const char* kSearchPage = R"({"data":[
  {"type":"reply","replyId":"r7","postId":"p9","authorUsername":"ada","content":"a reply"},
  {"type":"reply","replyId":"r8","postId":"p9","authorUsername":"bob","content":"another"}
],"cursor":"1"})";

static const char* kSearchPageEnd = R"({"data":[
  {"type":"user","userId":"u2","username":"pixel"}
],"cursor":null})";

}  // namespace fx
