#pragma once

// REST response bodies shaped after api.cyberspace.online/docs and
// /types.d.ts. Synthetic values; never captured from the live service.

namespace fx {

static const char* kLogin = R"({"data":{
  "idToken":"eyJhbGciOiJSUzI1NiJ9.e30.sig",
  "refreshToken":"AMf-refresh",
  "rtdbToken":"eyJrtdb",
  "rtdbUrl":"https://cyberspace-cyberspace-default-rtdb.europe-west1.firebasedatabase.app"
}})";

static const char* kRefresh = R"({"data":{
  "idToken":"eyJnew",
  "rtdbToken":"eyJrtdb2",
  "rtdbUrl":"https://cyberspace-cyberspace-default-rtdb.europe-west1.firebasedatabase.app"
}})";

static const char* kRooms = R"({"data":[
  {"id":"general","slug":"general","name":"General","lastMessageAt":1719700000000,"sortOrder":0,"onlineCount":12},
  {"id":"music","slug":"music","name":"Music","lastMessageAt":1719690000000,"sortOrder":1,"onlineCount":3},
  {"id":"quiet","slug":"quiet","name":"Quiet Room","lastMessageAt":0,"sortOrder":2,"onlineCount":0}
],"cursor":null})";

static const char* kHistory = R"({"data":[
  {"id":"-Nh1","userId":"u1","username":"tardibyte","isChatAdmin":false,"content":"first","timestamp":1719700000000},
  {"id":"-Nh2","userId":"u2","username":"genghis_khan","isChatAdmin":true,"content":"waves","timestamp":1719700001000,"isAction":true},
  {"id":"-Nh3","userId":"u3","username":"oumike","isChatAdmin":false,"content":"","timestamp":1719700002000,"imageUrl":"https://bunker.cyberspace.online/i.png"}
],"cursor":1719700000000})";

static const char* kHistoryStringCursor = R"({"data":[],"cursor":"1719600000000"})";

static const char* kHistoryNullCursor = R"({"data":[
  {"id":"-Nh0","userId":"u1","username":"tardibyte","isChatAdmin":false,"content":"oldest","timestamp":1719000000000}
],"cursor":null})";

static const char* kUsers = R"({"data":[
  {"userId":"u2","username":"genghis_khan","isChatAdmin":true,"lastSeen":1719700005000,"lastActivity":1719690000000},
  {"userId":"u1","username":"tardibyte","isChatAdmin":false,"lastSeen":1719700004000,"lastActivity":null}
],"cursor":null})";

static const char* kPresenceAck = R"({"data":{"roomId":"general","ok":true,"heartbeatMs":30000,"staleAfterMs":180000,"idleAfterMs":600000}})";

static const char* kSendResult = R"({"data":{"roomId":"general","messageId":"-Nsent"}})";

static const char* kHelpReply = R"({"data":{"reply":"Commands: /me /dice /8ball ..."}})";

static const char* kMentions = R"({"data":[
  {"id":"n1","userId":"me","type":"chat_mention","actorId":"u1","actorUsername":"tardibyte",
   "read":false,"createdAt":"2026-06-03T12:00:00.000Z",
   "metadata":{"roomSlug":"general","roomName":"General","messageContent":"hey @oumike"}},
  {"id":"n2","userId":"me","type":"chat_mention","read":false,"createdAt":"2026-06-03T12:01:00.000Z"},
  {"id":"n3","userId":"me","type":"reply","actorId":"u2","actorUsername":"rain","targetId":"p1",
   "read":false,"createdAt":"2026-06-03T12:02:00.000Z",
   "metadata":{"replyId":"r1","replyContent":"nice","postContent":"the post"}},
  {"id":"n4","userId":"me","type":"post_cooldown","actorUsername":"system","targetId":"p9",
   "reason":"held","read":false,"createdAt":"2026-06-03T12:03:00.000Z"}
],"cursor":null})";

static const char* kGuildNotice = R"({"data":[
  {"id":"n7","userId":"me","type":"guild_new_thread","actorUsername":"rain","targetId":"p42",
   "read":false,"createdAt":"2026-06-03T12:04:00.000Z",
   "metadata":{"guildSlug":"night-owls","guildName":"Night Owls","postContent":"hello"}}
],"cursor":null})";

static const char* kUnread = R"({"data":{"count":100,"exact":false}})";

static const char* kErrRateLimited = R"({"error":{"code":"RATE_LIMITED","message":"Too many requests"}})";

static const char* kErrNotVerified = R"({"error":{"code":"EMAIL_NOT_VERIFIED","message":"Verify your email address"}})";

static const char* kErr502Html = "<html><body><h1>502 Bad Gateway</h1></body></html>";

static const char* kConversations = R"({"data":[
  {"conversationId":"c-rain","otherUser":{"userId":"u-rain","username":"rain"},"lastMessage":"you around?","lastMessageAt":1719700050000,"unreadCount":2},
  {"conversationId":"c-moth","otherUser":{"userId":"u-moth","username":"moth","displayName":"Moth"},"lastMessage":"[GIF]","lastMessageAt":1719690000000,"unreadCount":0},
  {"conversationId":"c-gone","otherUser":{"userId":"u-gone","username":"ghost","deleted":true},"lastMessage":"","lastMessageAt":0,"unreadCount":0}
]})";

static const char* kStartConversation = R"({"data":{"conversationId":"c-kiwi","otherUser":{"userId":"u-kiwi","username":"kiwi"}}})";

static const char* kTypingAck = R"({"data":{"conversationId":"c-rain","ok":true,"heartbeatMs":2500,"staleAfterMs":8000}})";

static const char* kCmailHistory = R"({"data":[
  {"id":"-Dm1","senderId":"u-rain","senderUsername":"rain","content":"hey","timestamp":1719700000000},
  {"id":"-Dm2","senderId":"u-me","senderUsername":"oumike","content":"","timestamp":1719700001000,"gifUrl":"https://example.invalid/x.gif"}
],"cursor":1719700000000})";

}  // namespace fx
