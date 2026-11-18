#pragma once
// Cue opens the same way every time (15/11): its default size, in the middle of the screen the
// pointer is on. Where the window was left is not brought back (it used to be, for a 16:9 home).
inline bool restore_saved_window(bool /*opening_media*/, int /*width*/, int /*height*/) { return false; }
