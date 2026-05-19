#include "window_manager.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

bool WindowManager::window_class_registered = false;

WindowManager::WindowManager()
	: embedded_hwnd(nullptr),
	  container_hwnd(nullptr),
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

bool WindowManager::register_container_class() {
	if (window_class_registered) {
		return true;
	}
	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.lpfnWndProc = DefWindowProcW;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = L"ExternalWindowContainer";
	if (!RegisterClassExW(&wc)) {
		return false;
	}
	window_class_registered = true;
	return true;
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

bool WindowManager::embed_window(int64_t p_child_hwnd, int64_t p_owner_hwnd, const Rect2i &p_screen_rect) {
	HWND child_hwnd = reinterpret_cast<HWND>(p_child_hwnd);
	HWND owner_hwnd = reinterpret_cast<HWND>(p_owner_hwnd);

	if (!IsWindow(child_hwnd)) {
		UtilityFunctions::push_warning("WindowManager: Invalid child window handle");
		return false;
	}

	if (is_embedded) {
		unembed_window();
	}

	// Save original state
	original_parent_hwnd = GetParent(child_hwnd);
	original_style = GetWindowLongPtrW(child_hwnd, GWL_STYLE);
	original_ex_style = GetWindowLongPtrW(child_hwnd, GWL_EXSTYLE);
	GetWindowRect(child_hwnd, &original_rect);
	original_maximized = IsZoomed(child_hwnd) != 0;

	// Create a popup container window positioned at the screen rect
	if (!register_container_class()) {
		UtilityFunctions::push_warning("WindowManager: Failed to register container window class");
		return false;
	}

	container_hwnd = CreateWindowExW(
			WS_EX_NOACTIVATE,
			L"ExternalWindowContainer", L"",
			WS_POPUP | WS_CLIPCHILDREN,
			p_screen_rect.position.x, p_screen_rect.position.y,
			p_screen_rect.size.x, p_screen_rect.size.y,
			owner_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);

	if (!container_hwnd) {
		UtilityFunctions::push_warning("WindowManager: Failed to create container window");
		return false;
	}

	// Remove frame styles from target, add child style
	LONG_PTR new_style = original_style;
	new_style &= ~(WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
	new_style |= WS_CHILD;
	SetWindowLongPtrW(child_hwnd, GWL_STYLE, new_style);

	LONG_PTR new_ex_style = original_ex_style;
	new_ex_style &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME);
	SetWindowLongPtrW(child_hwnd, GWL_EXSTYLE, new_ex_style);

	SetWindowPos(child_hwnd, nullptr, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

	// Reparent target into our container
	SetParent(child_hwnd, container_hwnd);
	MoveWindow(child_hwnd, 0, 0, p_screen_rect.size.x, p_screen_rect.size.y, TRUE);

	// Show both
	ShowWindow(child_hwnd, SW_SHOW);
	ShowWindow(container_hwnd, SW_SHOWNOACTIVATE);

	embedded_hwnd = child_hwnd;
	is_embedded = true;

	return true;
}

void WindowManager::unembed_window() {
	if (!is_embedded || !embedded_hwnd) {
		return;
	}

	if (IsWindow(embedded_hwnd)) {
		ShowWindow(embedded_hwnd, SW_HIDE);
		SetParent(embedded_hwnd, nullptr);

		// Restore original styles
		SetWindowLongPtrW(embedded_hwnd, GWL_STYLE, original_style);
		SetWindowLongPtrW(embedded_hwnd, GWL_EXSTYLE, original_ex_style);
		SetWindowPos(embedded_hwnd, nullptr, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

		// Restore original parent
		if (original_parent_hwnd && IsWindow(original_parent_hwnd)) {
			SetParent(embedded_hwnd, original_parent_hwnd);
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

		SetForegroundWindow(embedded_hwnd);
	}

	// Destroy our container
	if (container_hwnd && IsWindow(container_hwnd)) {
		DestroyWindow(container_hwnd);
	}

	container_hwnd = nullptr;
	embedded_hwnd = nullptr;
	original_parent_hwnd = nullptr;
	original_style = 0;
	original_ex_style = 0;
	original_rect = {0, 0, 0, 0};
	original_maximized = false;
	is_embedded = false;
}

void WindowManager::reposition_embedded(const Rect2i &p_screen_rect) {
	if (!is_embedded || !container_hwnd || !IsWindow(container_hwnd)) {
		return;
	}
	// Move container to new screen position
	MoveWindow(container_hwnd, p_screen_rect.position.x, p_screen_rect.position.y,
			p_screen_rect.size.x, p_screen_rect.size.y, TRUE);
	// Resize embedded window to fill container
	if (embedded_hwnd && IsWindow(embedded_hwnd)) {
		MoveWindow(embedded_hwnd, 0, 0, p_screen_rect.size.x, p_screen_rect.size.y, TRUE);
	}
}

void WindowManager::show_embedded() {
	if (!is_embedded || !container_hwnd || !IsWindow(container_hwnd)) {
		return;
	}
	ShowWindow(container_hwnd, SW_SHOWNOACTIVATE);
}

void WindowManager::hide_embedded() {
	if (!is_embedded || !container_hwnd || !IsWindow(container_hwnd)) {
		return;
	}
	ShowWindow(container_hwnd, SW_HIDE);
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
	ClassDB::bind_method(D_METHOD("embed_window", "child_hwnd", "owner_hwnd", "screen_rect"), &WindowManager::embed_window);
	ClassDB::bind_method(D_METHOD("unembed_window"), &WindowManager::unembed_window);
	ClassDB::bind_method(D_METHOD("reposition_embedded", "screen_rect"), &WindowManager::reposition_embedded);
	ClassDB::bind_method(D_METHOD("show_embedded"), &WindowManager::show_embedded);
	ClassDB::bind_method(D_METHOD("hide_embedded"), &WindowManager::hide_embedded);
	ClassDB::bind_method(D_METHOD("is_window_valid", "hwnd"), &WindowManager::is_window_valid);
	ClassDB::bind_method(D_METHOD("get_window_title", "hwnd"), &WindowManager::get_window_title);
	ClassDB::bind_method(D_METHOD("has_embedded_window"), &WindowManager::has_embedded_window);
}

} // namespace godot
