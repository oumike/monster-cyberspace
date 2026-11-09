// LVGL's assert handler (LV_ASSERT_HANDLER in lv_conf.h): reports the file
// and line, then aborts, rather than LVGL's default silent while(1). C and
// C++; defined in lvui/lv_init.cpp, set with lvui::setAssertHandler.
#ifndef LV_ASSERT_HOOK_H
#define LV_ASSERT_HOOK_H

#ifdef __cplusplus
extern "C" {
#endif

void lvui_assert_failed(const char* file, int line);

#ifdef __cplusplus
}
#endif

#endif
