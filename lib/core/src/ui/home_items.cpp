#include "ui/home_items.h"

namespace rc {

std::vector<HomeItem> homeItems(const Features& f) {
  std::vector<HomeItem> out;
  if (f.unread) out.push_back(HomeItem::Unread);
  if (f.circ) {
    out.push_back(HomeItem::Messages);
    out.push_back(HomeItem::Rooms);
  }
  if (f.cmail) out.push_back(HomeItem::Cmail);
  if (f.feed) out.push_back(HomeItem::Feed);
  if (f.feed && f.people) out.push_back(HomeItem::Following);
  if (f.feed && f.notes) out.push_back(HomeItem::Notes);
  if (f.guilds) out.push_back(HomeItem::Guilds);
  if (f.feed && f.programs) out.push_back(HomeItem::Programs);
  if (f.people) out.push_back(HomeItem::Profile);
  out.push_back(HomeItem::Settings);
  return out;
}

bool homeSpanFirst(const std::vector<HomeItem>& items) {
  return !items.empty() && items.front() == HomeItem::Unread && items.size() % 2 == 1;
}

}  // namespace rc
