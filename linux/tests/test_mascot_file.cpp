// SPDX-License-Identifier: GPL-3.0-or-later
// The Linux mascot store: the partition's blob in a file in the config dir.
#include <sys/stat.h>

#include <fstream>

#include "app/queues.h"
#include "check.h"
#include "store/mascot_store.h"

namespace {

std::string file;

bool fileExists() {
  struct stat st {};
  return stat(file.c_str(), &st) == 0;
}

void noFileLoadsDefaults() {
  rc::Mascot m;
  m.art = {"stale"};
  CHECK(mascotstore::load(m));
  CHECK(m.art.empty());
  CHECK(m.phrases.empty());
}

void saveLoadsBack() {
  rc::Mascot m;
  m.art = {" /\\_/\\ ", "( o.o )", " > ^ < "};
  m.phrases = {"hello", "beep"};
  CHECK_EQ(rc::validateMascot(m), std::string());
  CHECK(mascotstore::save(m));
  CHECK(fileExists());
  struct stat st {};
  stat(file.c_str(), &st);
  CHECK_EQ(size_t(st.st_size), rc::kMascotBlobSize);
  CHECK_EQ(st.st_mode & 0777, mode_t(0600));

  rc::Event e{rc::EvtType::RoomChanged};
  bool changed = false;
  while (rc::nextEvent(e)) changed |= e.type == rc::EvtType::MascotChanged;
  CHECK(changed);

  rc::Mascot back;
  CHECK(mascotstore::load(back));
  CHECK(back.art == m.art);
  CHECK(back.phrases == m.phrases);
}

void emptyOrBadFileLoadsDefaults() {
  { std::ofstream f(file, std::ios::trunc); }
  rc::Mascot m;
  m.phrases = {"stale"};
  CHECK(mascotstore::load(m));
  CHECK(m.phrases.empty());

  { std::ofstream f(file, std::ios::trunc); f << "not a mascot blob"; }
  m.phrases = {"stale"};
  CHECK(mascotstore::load(m));
  CHECK(m.phrases.empty());
}

}  // namespace

int main() {
  const std::string dir = tempDir() + "/cfg";  // not made yet: save creates it
  setenv("MONSTER_CONFIG_DIR", dir.c_str(), 1);
  file = dir + "/mascot.bin";
  noFileLoadsDefaults();
  saveLoadsBack();
  emptyOrBadFileLoadsDefaults();
  return checkResult();
}
