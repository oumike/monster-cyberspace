#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rc {

// The API's limit on a note's content, in characters.
constexpr size_t kNoteMaxChars = 32768;
// A list row's title is cut to this many bytes.
constexpr size_t kNoteTitleBytes = 120;

// A row of GET /v1/notes or /v1/notes/:id/revisions: the content is kept
// only as its title.
struct NoteRow {
  std::string id, title;
  int64_t updatedMs = -1;
  int revision = 0;
};

// GET /v1/notes/:id (one revision).
struct Note {
  std::string id, content;
  int64_t updatedMs = -1;
  int revision = 0;
};

// The first non-blank line, a leading '#' run and the space after it
// dropped, trimmed, cut to kNoteTitleBytes on a UTF-8 boundary.
std::string noteTitle(std::string_view content);
// Deleted notes skipped; `cursor` "" when null.
bool parseNoteList(std::string_view body, std::vector<NoteRow>& out, std::string& cursor);
// False without a noteId.
bool parseNote(std::string_view body, Note& out);
// POST/PATCH/DELETE responses: data.noteId.
bool parseNoteId(std::string_view body, std::string& id);
// {"content":"…"}: topics are never sent, so an edit keeps them.
std::string noteBodyJson(const std::string& content);
// nullptr, "Note is empty" or "Note is too long".
const char* noteError(const std::string& content);

}  // namespace rc
