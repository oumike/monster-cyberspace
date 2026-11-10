#pragma once

// Notes response bodies shaped after api.cyberspace.online/docs and
// /types.d.ts. Synthetic values; never captured from the live service.

namespace fx {

static const char* kNoteList = R"({"data":[
  {"noteId":"n1","authorId":"me1","content":"\n\n# Shopping\nmilk\neggs","revisionNumber":3,"deleted":false,
   "createdAt":"2026-11-01T10:00:00.000Z","updatedAt":"2026-11-02T10:00:00.000Z"},
  {"noteId":"nx","authorId":"me1","content":"gone","deleted":true,
   "createdAt":"2026-11-01T09:00:00.000Z","updatedAt":"2026-11-01T09:00:00.000Z"},
  {"noteId":"n2","authorId":"me1","content":"Plain idea","revisionNumber":1,"deleted":false,
   "createdAt":"2026-11-01T08:00:00.000Z","updatedAt":"2026-11-01T08:00:00.000Z"}
],"cursor":"n2"})";

static const char* kNoteListBare = R"({"data":[
  {"noteId":"n5","content":"hello","deleted":false,"createdAt":"2026-11-01T08:00:00.000Z"}
],"cursor":null})";

static const char* kNote = R"({"data":{"noteId":"n1","authorId":"me1","content":"# Shopping\nmilk\neggs",
  "topics":["list"],"revisionNumber":3,"deleted":false,
  "createdAt":"2026-11-01T10:00:00.000Z","updatedAt":"2026-11-02T10:00:00.000Z"}})";

static const char* kNoteNoId = R"({"data":{"content":"x"}})";

static const char* kNoteCreated = R"({"data":{"noteId":"n9"}})";

}  // namespace fx
