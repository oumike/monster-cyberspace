#pragma once

// Guild response bodies shaped after api.cyberspace.online/docs.
// Synthetic values; never captured from the live service.

namespace fx {

static const char* kGuildPage = R"({"data":[
  {"id":"g1","slug":"night-owls","name":"Night Owls","icon":"🦉","founderUsername":"someone","bio":"We post after dark.","link":"https://example.org","linkText":"our site","memberCount":42,"apprenticeCount":7,"createdAt":"2026-03-27T10:12:01.516Z"},
  {"id":"g2","slug":"synths","name":"Synths","icon":"🎹","founderUsername":"rain","memberCount":9,"createdAt":"2026-03-28T00:00:00.000Z"}
],"cursor":"xyz"})";

static const char* kGuildPageEnd = R"({"data":[
  {"id":"g3","slug":"last","name":"Last","icon":"🌙","founderUsername":"a","memberCount":1,"apprenticeCount":0,"createdAt":"2026-03-29T00:00:00.000Z"}
],"cursor":null})";

static const char* kGuildNotMember = R"({"data":
  {"id":"g1","slug":"night-owls","name":"Night Owls","icon":"🦉","founderUsername":"someone","bio":"We post after dark.","memberCount":42,"createdAt":"2026-03-27T10:12:01.516Z","isMember":false,"role":null}
})";

static const char* kGuildMember = R"({"data":
  {"id":"g1","slug":"night-owls","name":"Night Owls","icon":"🦉","founderUsername":"someone","memberCount":42,"apprenticeCount":7,"createdAt":"2026-03-27T10:12:01.516Z","isMember":true,"role":"member"}
})";

static const char* kMyGuilds = R"({"data":[
  {"slug":"night-owls","name":"Night Owls","icon":"🦉","role":"member"},
  {"slug":"a","name":"A","icon":"🅰","role":"apprentice"},
  {"slug":"b","name":"B","icon":"🅱","role":"apprentice"},
  {"slug":"c","name":"C","icon":"🌊","role":"apprentice"},
  {"slug":"d","name":"D","icon":"🌲","role":"apprentice"},
  {"slug":"e","name":"E","icon":"🔥","role":"apprentice"}
],"cursor":null})";

static const char* kGuildMembers = R"({"data":[
  {"membershipId":"g1_u1","userId":"u1","username":"someone","role":"founder","joinedAt":"2026-03-27T10:12:01.516Z"},
  {"membershipId":"g1_u2","userId":"u2","username":"rain","role":"member","joinedAt":"2026-03-28T00:00:00.000Z"}
],"cursor":"g1_u2"})";

static const char* kGuildMembersEnd = R"({"data":[
  {"membershipId":"guildId_uid","userId":"uid","username":"someone","role":"member","joinedAt":"2026-03-27T10:12:01.516Z"}
],"cursor":null})";

static const char* kGuildJoin = R"({"data":{"guildId":"g","role":"apprentice"}})";

}  // namespace fx
