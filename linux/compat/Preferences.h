// SPDX-License-Identifier: GPL-3.0-or-later
// Arduino's Preferences (ESP32 NVS) over files: one file per namespace in
// $MONSTER_CONFIG_DIR, else $XDG_CONFIG_HOME/monster-cyberspace, else
// ~/.config/monster-cyberspace (directory 0700, files 0600). Every write
// replaces the file atomically (a temporary, then rename). Namespaces are
// shared by every Preferences in the process, as NVS's are.
#pragma once

#include <Arduino.h>

#include <string>

class Preferences {
 public:
  ~Preferences() { end(); }
  // Read-only fails if the namespace was never created; read-write creates it.
  bool begin(const char* name, bool readOnly = false);
  void end();

  bool clear();
  bool remove(const char* key);
  bool isKey(const char* key);

  // Each returns the bytes stored, 0 on failure (read-only, key over 15).
  size_t putBool(const char* key, bool v);
  size_t putUChar(const char* key, uint8_t v);
  size_t putUShort(const char* key, uint16_t v);
  size_t putUInt(const char* key, uint32_t v);
  size_t putString(const char* key, const char* v);
  size_t putString(const char* key, const String& v) { return putString(key, v.c_str()); }

  // The default when the key is missing or holds another type (bool and
  // uchar share NVS's u8).
  bool getBool(const char* key, bool def = false);
  uint8_t getUChar(const char* key, uint8_t def = 0);
  uint16_t getUShort(const char* key, uint16_t def = 0);
  uint32_t getUInt(const char* key, uint32_t def = 0);
  String getString(const char* key, const String& def = String());

 private:
  std::string ns_;
  bool open_ = false;
  bool readOnly_ = true;
};

namespace compat {
// The directory the namespaces live in.
std::string configDir();
// Writes `name` in configDir() whole or not at all (a temporary file, fsync,
// rename), creating the directory 0700 and the file 0600.
bool writeConfigFile(const std::string& name, const std::string& bytes);
// Drops the in-memory namespaces so the next begin() reads the files (tests).
void preferencesForget();
}  // namespace compat
