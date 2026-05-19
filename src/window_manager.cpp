#include "window_manager.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

WindowManager::WindowManager()
	: embedded_hwnd(nullptr),
	  original_parent_hwnd(nullptr),
	  original_style(0),
	  original_ex_style(0),
	  original_rect({0, 0, 0, 0}),
	  original_maximized(false),
	  is_embedded(false) {
}

WindowManager::~WindowManager() {
	if (is_embedded) {
		unembed_window();
	}
}

BOOL CALLBACK WindowManager::enum_windows_proc(HWND hWnd, LPARAM lParam) {
	if (!IsWindowVisible(hWnd)) {
		return TRUE;
	}

	wchar_t title[256] = { 0 };
	GetWindowTextW(hWnd, title, 256);
	if (wcslen(title) == 0) {
		return TRUE;
	}

	// Skip windows with no meaningful size
	RECT rect;
	if (!GetWindowRect(hWnd, &rect)) {
		return TRUE;
	}
	if ((rect.right - rect.left) <= 0 || (rect.bottom - rect.top) <= 0) {
		return TRUE;
	}

	DWORD pid = 0;
	GetWindowThreadProcessId(hWnd, &pid);

	wchar_t class_name[256] = { 0 };
	GetClassNameW(hWnd, class_name, 256);

	auto *windows = reinterpret_cast<Array *>(lParam);
	Dictionary entry;
	entry["hwnd"] = (int64_t)hWnd;
	entry["title"] = String(title);
	entry["pid"] = (int64_t)pid;
	entry["class_name"] = String(class_name);
	windows->append(entry);

	return TRUE;
}

Array WindowManager::get_visible_windows() const {
	Array windows;
	EnumWindows(enum_windows_proc, reinterpret_cast<LPARAM>(&windows));
	return windows;
}

bool WindowManager::embed_window(int64_t p_child_hwnd, int64_t p_parent_hwnd, const Rect2i &p_rect) {
	HWND child_hwnd = reinterpret_cast<HWND>(p_child_hwnd);
	HWND parent_hwnd = reinterpret_cast<HWND>(p_parent_hwnd);

	if (!IsWindow(child_hwnd) || !IsWindow(parent_hwnd)) {
		UtilityFunctions::push_warning("WindowManager: Invalid window handle");
		return false;
	}

	// Unembed any currently embedded window first
	if (is_embedded) {
		unembed_window();
	}

	// Save original state
	original_parent_hwnd = GetParent(child_hwnd);
	original_style = GetWindowLongPtrW(child_hwnd, GWL_STYLE);
	original_ex_style = GetWindowLongPtrW(child_hwnd, GWL_EXSTYLE);
	GetWindowRect(child_hwnd, &original_rect);
	original_maximized = IsZoomed(child_hwnd) != 0;

	// Remove frame styles, add child style
	LONG_PTR new_style = original_style;
	new_style &= ~(WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
	new_style |= WS_CHILD;
	SetWindowLongPtrW(child_hwnd, GWL_STYLE, new_style);

	// Remove extended frame styles
	LONG_PTR new_ex_style = original_ex_style;
	new_ex_style &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME);
	SetWindowLongPtrW(child_hwnd, GWL_EXSTYLE, new_ex_style);

	// Force style changes
	SetWindowPos(child_hwnd, nullptr, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

	// Reparent to the editor window
	SetParent(child_hwnd, parent_hwnd);

	// Position and resize within parent
	MoveWindow(child_hwnd, p_rect.position.x, p_rect.position.y,
			p_rect.size.x, p_rect.size.y, TRUE);

	// Show the embedded window
	ShowWindow(child_hwnd, SW_SHOW);

	// Bring embedded window to foreground within parent
	SetWindowPos(child_hwnd, HWND_TOP, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	embedded_hwnd = child_hwnd;
	is_embedded = true;

	return true;
}

void WindowManager::unembed_window() {
	if (!is_embedded || !embedded_hwnd) {
		return;
	}

	if (IsWindow(embedded_hwnd)) {
		// Hide first
		ShowWindow(embedded_hwnd, SW_HIDE);

		// Detach from current parent
		SetParent(embedded_hwnd, nullptr);

		// Restore original styles
		SetWindowLongPtrW(embedded_hwnd, GWL_STYLE, original_style);
		SetWindowLongPtrW(embedded_hwnd, GWL_EXSTYLE, original_ex_style);

		// Force style changes
		SetWindowPos(embedded_hwnd, nullptr, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

		// Restore original parent
		if (original_parent_hwnd && IsWindow(original_parent_hwnd)) {
			SetParent(embedded_hwnd, original_parent_hwnd);
		} else {
			SetParent(embedded_hwnd, nullptr);
		}

		// Restore original position and size
		if (original_maximized) {
			ShowWindow(embedded_hwnd, SW_SHOWMAXIMIZED);
		} else {
			MoveWindow(embedded_hwnd,
					original_rect.left, original_rect.top,
					original_rect.right - original_rect.left,
					original_rect.bottom - original_rect.top,
					TRUE);
			ShowWindow(embedded_hwnd, SW_SHOW);
		}

		// Bring back to foreground
		SetForegroundWindow(embedded_hwnd);
	}

	embedded_hwnd = nullptr;
	original_parent_hwnd = nullptr;
	original_style = 0;
	original_ex_style = 0;
	original_rect = {0, 0, 0, 0};
	original_maximized = false;
	is_embedded = false;
}

void WindowManager::reposition_embedded(const Rect2i &p_rect) {
	if (!is_embedded || !embedded_hwnd || !IsWindow(embedded_hwnd)) {
		return;
	}
	MoveWindow(embedded_hwnd, p_rect.position.x, p_rect.position.y,
			p_rect.size.x, p_rect.size.y, TRUE);
}

void WindowManager::show_embedded() {
	if (!is_embedded || !embedded_hwnd || !IsWindow(embedded_hwnd)) {
		return;
	}
	ShowWindow(embedded_hwnd, SW_SHOW);
}

void WindowManager::hide_embedded() {
	if (!is_embedded || !embedded_hwnd || !IsWindow(embedded_hwnd)) {
		return;
	}
	ShowWindow(embedded_hwnd, SW_HIDE);
}

bool WindowManager::is_window_valid(int64_t p_hwnd) const {
	return IsWindow(reinterpret_cast<HWND>(p_hwnd)) != 0;
}

String WindowManager::get_window_title(int64_t p_hwnd) const {
	HWND hwnd = reinterpret_cast<HWND>(p_hwnd);
	if (!IsWindow(hwnd)) {
		return "";
	}
	wchar_t title[256] = { 0 };
	GetWindowTextW(hwnd, title, 256);
	return String(title);
}

bool WindowManager::has_embedded_window() const {
	return is_embedded;
}

void WindowManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_visible_windows"), &WindowManager::get_visible_windows);
	ClassDB::bind_method(D_METHOD("embed_window", "child_hwnd", "parent_hwnd", "rect"), &WindowManager::embed_window);
	ClassDB::bind_method(D_METHOD("unembed_window"), &WindowManager::unembed_window);
	ClassDB::bind_method(D_METHOD("reposition_embedded", "rect"), &WindowManager::reposition_embedded);
	ClassDB::bind_method(D_METHOD("show_embedded"), &WindowManager::show_embedded);
	ClassDB::bind_method(D_METHOD("hide_embedded"), &WindowManager::hide_embedded);
	ClassDB::bind_method(D_METHOD("is_window_valid", "hwnd"), &WindowManager::is_window_valid);
	ClassDB::bind_method(D_METHOD("get_window_title", "hwnd"), &WindowManager::get_window_title);
	ClassDB::bind_method(D_METHOD("has_embedded_window"), &WindowManager::has_embedded_window);
}

} // namespace godot
