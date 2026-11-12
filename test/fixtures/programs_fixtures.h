// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Programs response bodies shaped after api.cyberspace.online/docs.md
// (Programs). Synthetic values; never captured from the live service.

namespace fx {

static const char* kProgramPage = R"({"data":[
  {"id":"p1","name":"hello","ownerUsername":"ada","description":"Says hello","runtime":"term",
   "release":3,"publishedAt":1762000000000},
  {"id":"p2","name":"clock","ownerUsername":"bob","description":"A clock","release":1,
   "publishedAt":1761000000000},
  {"name":"no-id","ownerUsername":"x"}
],"cursor":1761000000000})";

static const char* kProgramPageEnd = R"({"data":[],"cursor":null})";

static const char* kProgramSourceUtf8 = R"({"data":{"id":"p1","name":"hello","ownerUsername":"ada",
  "description":"Says hello","runtime":"term","release":3,"encoding":"utf8",
  "source":"export default async (p) => {\n\tp.print('hi')\n}\n"}})";

// "AGFzbQEAAAA=" is the 8 bytes "\0asm\1\0\0\0".
static const char* kProgramSourceWasm = R"({"data":{"id":"p9","name":"tiny","ownerUsername":"cy",
  "description":"wasm","runtime":"wasm","release":1,"encoding":"base64","source":"AGFzbQEAAAA="}})";

}  // namespace fx
