#include "ui/home_items.h"

namespace rc {

std::vector<HomeItem> homeItems(const Features& f) {
  std::vector<HomeItem> out;
  if (f.unread) out.push_back(HomeItem::Unread);
  out.push_back(HomeItem::Messages);
  out.push_back(HomeItem::Rooms);
  if (f.cmail) out.push_back(HomeItem::Cmail);
  if (f.feed) out.push_back(HomeItem::Feed);
  if (f.guilds) out.push_back(HomeItem::Guilds);
  out.push_back(HomeItem::Settings);
  return out;
}

}  // namespace rc
