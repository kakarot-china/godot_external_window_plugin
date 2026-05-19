#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/rect2i.hpp>

#include <windows.h>

namespace godot {

class WindowManager : public RefCounted {
	GDCLASS(WindowManager, RefCounted)

private:
	HWND embedded_hwnd;
	HWND container_hwnd;
	HWND original_parent_hwnd;
	LONG_PTR original_style;
	LONG_PTR original_ex_style;
	RECT original_rect;
	bool original_maximized;
	bool is_embedded;

	static bool window_class_registered;
	static BOOL CALLBACK enum_windows_proc(HWND hWnd, LPARAM lParam);
	bool register_container_class();

public:
	WindowManager();
	~WindowManager() override;

	Array get_visible_windows() const;
	bool embed_window(int64_t p_child_hwnd, int64_t p_owner_hwnd, const Rect2i &p_screen_rect);
	void unembed_window();
	void reposition_embedded(const Rect2i &p_screen_rect);
	void show_embedded();
	void hide_embedded();
	bool is_window_valid(int64_t p_hwnd) const;
	String get_window_title(int64_t p_hwnd) const;
	bool has_embedded_window() const;

protected:
	static void _bind_methods();
};

} // namespace godot

#endif // WINDOW_MANAGER_H
