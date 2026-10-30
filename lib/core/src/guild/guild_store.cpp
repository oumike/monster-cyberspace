#include "guild/guild_store.h"

#include <algorithm>
#include <utility>

namespace rc {

namespace {

const std::string& keyOf(const Guild& g) { return g.id.empty() ? g.slug : g.id; }

void cutBio(Guild& g) {
  if (g.bio.size() <= kGuildBioBytes) return;
  size_t n = kGuildBioBytes;
  while (n > 0 && (static_cast<unsigned char>(g.bio[n]) & 0xC0) == 0x80) n--;
  g.bio.resize(n);
}

}  // namespace

void GuildStore::setMine(std::vector<MyGuild> mine) {
  mine_ = std::move(mine);
  mineLoaded_ = true;
}

const MyGuild* GuildStore::myGuild(const std::string& slug) const {
  for (const auto& m : mine_)
    if (m.slug == slug) return &m;
  return nullptr;
}

void GuildStore::setBrowse(std::vector<Guild> page, const std::string& cursor, bool first) {
  if (first) browse_.clear();
  browseLoaded_ = true;
  browseCursor_ = cursor;
  for (auto& g : page) {
    const std::string& k = keyOf(g);
    bool held = std::any_of(browse_.begin(), browse_.end(),
                            [&](const Guild& h) { return keyOf(h) == k; });
    if (held) continue;
    if (browse_.size() >= kMaxGuilds) {
      browseCursor_.clear();
      break;
    }
    cutBio(g);
    browse_.push_back(std::move(g));
  }
  if (browse_.size() >= kMaxGuilds && !cursor.empty()) browseCursor_.clear();
}

std::vector<Guild> GuildStore::others() const {
  std::vector<Guild> out;
  for (const auto& g : browse_)
    if (!myGuild(g.slug)) out.push_back(g);
  return out;
}

std::vector<Guild> GuildStore::pinned() const {
  std::vector<Guild> out;
  for (const auto& m : mine_) {
    const Guild* found = nullptr;
    if (guild_ && guild_->slug == m.slug) found = guild_.get();
    if (!found)
      for (const auto& g : browse_)
        if (g.slug == m.slug) {
          found = &g;
          break;
        }
    Guild g;
    if (found) {
      g = *found;
    } else {
      g.slug = m.slug;
      g.name = m.name;
      g.icon = m.icon;
    }
    if (g.role.empty()) g.role = m.role;
    out.push_back(std::move(g));
  }
  return out;
}

void GuildStore::openGuild(Guild g) {
  cutBio(g);
  guild_ = std::make_unique<Guild>(std::move(g));
  members_.clear();
  membersCursor_.clear();
}

void GuildStore::closeGuild() {
  guild_.reset();
  members_.clear();
  members_.shrink_to_fit();
  membersCursor_.clear();
}

void GuildStore::appendMembers(std::vector<Member> page, const std::string& cursor, bool first) {
  if (first) members_.clear();
  membersCursor_ = cursor;
  for (auto& m : page) {
    if (members_.size() >= kMaxMembers) {
      membersCursor_.clear();
      break;
    }
    members_.push_back(std::move(m));
  }
  if (members_.size() >= kMaxMembers && !cursor.empty()) membersCursor_.clear();
}

void GuildStore::clear() {
  mine_.clear();
  mineLoaded_ = false;
  browse_.clear();
  browseCursor_.clear();
  browseLoaded_ = false;
  closeGuild();
}

}  // namespace rc
