#pragma once

// People response bodies shaped after api.cyberspace.online/docs and
// /types.d.ts. Synthetic values; never captured from the live service.

namespace fx {

static const char* kProfileFull = R"({"data":{
  "userId":"u42","username":"ann","displayName":"Ann Example","bio":"I like **night** walks.",
  "serialNumber":117,"isSupporter":true,"followersCount":40,"followingCount":12,
  "publicPostsCount":5,"hasPublicPosts":true,"websiteUrl":"https://ann.example",
  "websiteName":"my site","locationName":"London, UK","guildId":"g1","guildSlug":"night-owls",
  "guildIcon":"🦉","guildName":"Night Owls","createdAt":"2026-03-27T10:12:01.516Z",
  "lastActiveAt":"2026-10-31T09:00:00.000Z","someNewField":{"x":1}
}})";

static const char* kProfileBare = R"({"data":{"userId":"u7","username":"bob"}})";

static const char* kProfileNoName = R"({"data":{"userId":"u7"}})";

static const char* kOwnProfile = R"({"data":{
  "userId":"me1","username":"oumike","blockedUsers":["troll","Spammer"],"mutedUsers":["loud"]
}})";

static const char* kFollowPage = R"({"data":[
  {"followId":"f1","followerId":"me1","followedId":"u42","createdAt":"2026-05-01T00:00:00.000Z"},
  {"followId":"f2","followerId":"me1","followedId":"u43","createdAt":"2026-05-02T00:00:00.000Z"}
],"cursor":"f2"})";

static const char* kFollowPageEnd = R"({"data":[
  {"followId":"f3","followerId":"me1","followedId":"u44","createdAt":"2026-05-03T00:00:00.000Z"}
],"cursor":null})";

static const char* kFollowCreated = R"({"data":{"followId":"f9"}})";

static const char* kUserReplies = R"({"data":[
  {"replyId":"r1","postId":"p1","authorId":"u42","authorUsername":"ann","content":"First **reply**\nsecond line",
   "deleted":false,"createdAt":"2026-10-30T12:00:00.000Z"},
  {"replyId":"rx","postId":"p1","authorId":"u42","authorUsername":"ann","content":"","deleted":true,
   "createdAt":"2026-10-30T11:00:00.000Z"},
  {"replyId":"r2","postId":"p2","authorId":"u42","authorUsername":"ann","deleted":false,
   "createdAt":"2026-10-29T12:00:00.000Z"}
],"cursor":"r2"})";

static const char* kUserRepliesEnd = R"({"data":[],"cursor":null})";

static const char* kUserGuilds = R"({"data":[
  {"guildId":"g1","slug":"night-owls","name":"Night Owls","icon":"🦉","role":"member","joinedAt":"2026-05-01T00:00:00.000Z"},
  {"guildId":"g2","slug":"early-birds","name":"Early Birds","icon":"🐦","role":"apprentice","joinedAt":"2026-06-01T00:00:00.000Z"}
],"cursor":null})";

static const char* kUserGuildsNoIcon = R"({"data":[
  {"guildId":"g3","slug":"plain","name":"Plain","role":"founder","joinedAt":"2026-05-01T00:00:00.000Z"}
],"cursor":null})";

}  // namespace fx
