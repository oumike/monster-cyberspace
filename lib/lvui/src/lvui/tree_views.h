#pragma once

#include <lvgl.h>

namespace lvui {

// A screen's views, on the heap only while the object tree they were built
// into is up (static members would hold every screen's views for good, which
// a board without PSRAM can't spare). make() gives fresh views for a new
// root; they are freed when that root is deleted. ScreenHost builds the new
// tree beside the old one, so the old views stay until their own tree goes,
// and come back if the new tree is thrown away (out of memory).
template <class V>
class TreeViews {
 public:
  TreeViews() = default;
  TreeViews(const TreeViews&) = delete;
  TreeViews& operator=(const TreeViews&) = delete;

  V& make(lv_obj_t* root) {
    Node* n = new Node(this);
    prev_ = cur_;
    cur_ = n;
    lv_obj_add_event_cb(root, deleted, LV_EVENT_DELETE, n);
    return n->views;
  }
  // The views of the newest tree still up; null before the first build or
  // once every tree is gone.
  V* get() const { return cur_ ? &cur_->views : nullptr; }
  V* operator->() const { return get(); }
  explicit operator bool() const { return cur_ != nullptr; }

 private:
  struct Node {
    explicit Node(TreeViews* o) : owner(o) {}
    TreeViews* owner;
    V views;
  };

  static void deleted(lv_event_t* e) {
    Node* n = static_cast<Node*>(lv_event_get_user_data(e));
    TreeViews* o = n->owner;
    if (n == o->cur_) {
      o->cur_ = o->prev_;
      o->prev_ = nullptr;
    } else if (n == o->prev_) {
      o->prev_ = nullptr;
    }
    delete n;
  }

  Node* cur_ = nullptr;
  Node* prev_ = nullptr;
};

}  // namespace lvui
