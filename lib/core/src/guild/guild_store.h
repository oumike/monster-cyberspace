#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "guild/guild.h"

namespace rc {

constexpr size_t kMaxGuilds = 200, kMaxMembers = 200, kGuildBioBytes = 280;

// My guilds, the browse list, the open guild and its members. References
// returned here are invalidated by the next mutation.
class GuildStore {
 public:
  void setMine(std::vector<MyGuild> mine);  // sets mineLoaded
  const std::vector<MyGuild>& mine() const { return mine_; }
  bool mineLoaded() const { return mineLoaded_; }
  const MyGuild* myGuild(const std::string& slug) const;  // nullptr when not in it

  // Browse list. first=true replaces; else appends, skipping held ids. Bios cut.
  void setBrowse(std::vector<Guild> page, const std::string& cursor, bool first);
  // The browse list without guilds in mine(), in server order.
  std::vector<Guild> others() const;
  // Every guild in mine() resolved from the open guild or the browse list
  // (else a Guild built from the MyGuild), in mine() order.
  std::vector<Guild> pinned() const;
  const std::string& browseCursor() const { return browseCursor_; }
  bool browseLoaded() const { return browseLoaded_; }
  bool browseEnd() const { return browseLoaded_ && browseCursor_.empty(); }

  void openGuild(Guild g);  // clears members; bio cut
  void closeGuild();        // frees the open guild and members
  const Guild* guild() const { return guild_.get(); }
  void appendMembers(std::vector<Member> page, const std::string& cursor, bool first);
  const std::vector<Member>& members() const { return members_; }
  const std::string& membersCursor() const { return membersCursor_; }

  void clear();  // sign-out: everything

 private:
  std::vector<MyGuild> mine_;
  bool mineLoaded_ = false;
  std::vector<Guild> browse_;
  std::string browseCursor_;
  bool browseLoaded_ = false;
  std::unique_ptr<Guild> guild_;
  std::vector<Member> members_;
  std::string membersCursor_;
};

}  // namespace rc
