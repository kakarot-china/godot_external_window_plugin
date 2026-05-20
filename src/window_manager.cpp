#include "window_manager.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

WindowManager::WindowManager()
	: embedded_hwnd(nullptr),
	  original_owner(nullptr),
	  original_style(0),
	  original_ex_style(0),
	  original_placement({sizeof(WINDOWPLACEMENT)}),
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

	UtilityFunctions::print(vformat("[EW] embed_window: child=0x%X owner=0x%X rect=(%d,%d %dx%d)",
			(unsigned int)(uintptr_t)child_hwnd, (unsigned int)(uintptr_t)owner_hwnd,
			p_screen_rect.position.x, p_screen_rect.position.y,
			p_screen_rect.size.x, p_screen_rect.size.y));

	if (!IsWindow(child_hwnd)) {
		UtilityFunctions::push_warning("[EW] child hwnd is NOT a valid window");
		return false;
	}
	if (!IsWindow(owner_hwnd)) {
		UtilityFunctions::push_warning("[EW] owner hwnd is NOT a valid window");
		return false;
	}

	if (is_embedded) {
		unembed_window();
	}

	// Save original state
	original_style = GetWindowLongPtrW(child_hwnd, GWL_STYLE);
	original_ex_style = GetWindowLongPtrW(child_hwnd, GWL_EXSTYLE);
	original_owner = (HWND)GetWindowLongPtrW(child_hwnd, GWLP_HWNDPARENT);
	original_placement.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(child_hwnd, &original_placement);

	UtilityFunctions::print(vformat("[EW] saved: style=0x%X ex_style=0x%X owner=0x%X showCmd=%d",
			(unsigned int)original_style, (unsigned int)original_ex_style,
			(unsigned int)(uintptr_t)original_owner, original_placement.showCmd));

	// Restyle: frameless popup
	LONG_PTR new_style = WS_POPUP | WS_CLIPCHILDREN | WS_VISIBLE;
	SetWindowLongPtrW(child_hwnd, GWL_STYLE, new_style);

	LONG_PTR new_ex_style = original_ex_style;
	new_ex_style &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME);
	SetWindowLongPtrW(child_hwnd, GWL_EXSTYLE, new_ex_style);

	// Set owner to editor window so popup stays above it
	SetWindowLongPtrW(child_hwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner_hwnd));

	// Position at screen rect and apply style changes
	MoveWindow(child_hwnd, p_screen_rect.position.x, p_screen_rect.position.y,
			p_screen_rect.size.x, p_screen_rect.size.y, TRUE);

	SetWindowPos(child_hwnd, HWND_TOP, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);

	ShowWindow(child_hwnd, SW_SHOWNOACTIVATE);

	// Verify final state
	LONG_PTR verify_style = GetWindowLongPtrW(child_hwnd, GWL_STYLE);
	HWND verify_owner = (HWND)GetWindowLongPtrW(child_hwnd, GWLP_HWNDPARENT);
	BOOL visible = IsWindowVisible(child_hwnd);
	RECT final_rect;
	GetWindowRect(child_hwnd, &final_rect);
	UtilityFunctions::print(vformat("[EW] after embed: style=0x%X owner=0x%X visible=%d rect=(%d,%d %dx%d)",
			(unsigned int)verify_style, (unsigned int)(uintptr_t)verify_owner, visible,
			final_rect.left, final_rect.top,
			final_rect.right - final_rect.left, final_rect.bottom - final_rect.top));

	embedded_hwnd = child_hwnd;
	is_embedded = true;

	return true;
}

void WindowManager::unembed_window() {
	if (!is_embedded || !embedded_hwnd) {
		return;
	}

	UtilityFunctions::print(vformat("[EW] unembed: hwnd=0x%X", (unsigned int)(uintptr_t)embedded_hwnd));

	if (IsWindow(embedded_hwnd)) {
		ShowWindow(embedded_hwnd, SW_HIDE);

		// Restore original styles FIRST
		SetWindowLongPtrW(embedded_hwnd, GWL_STYLE, original_style);
		SetWindowLongPtrW(embedded_hwnd, GWL_EXSTYLE, original_ex_style);

		// Then restore owner
		SetWindowLongPtrW(embedded_hwnd, GWLP_HWNDPARENT,
				reinterpret_cast<LONG_PTR>(original_owner));

		// Apply style changes
		SetWindowPos(embedded_hwnd, nullptr, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

		// Restore original placement but show in normal (restored) state.
		// Force SW_SHOWNORMAL so minimized/maximized windows come back visible.
		WINDOWPLACEMENT restore_placement = original_placement;
		restore_placement.showCmd = SW_SHOWNORMAL;
		SetWindowPlacement(embedded_hwnd, &restore_placement);

		// Verify
		LONG_PTR v_style = GetWindowLongPtrW(embedded_hwnd, GWL_STYLE);
		HWND v_owner = (HWND)GetWindowLongPtrW(embedded_hwnd, GWLP_HWNDPARENT);
		RECT v_rect;
		GetWindowRect(embedded_hwnd, &v_rect);
		UtilityFunctions::print(vformat("[EW] after unembed: style=0x%X owner=0x%X rect=(%d,%d %dx%d)",
				(unsigned int)v_style, (unsigned int)(uintptr_t)v_owner,
				v_rect.left, v_rect.top,
				v_rect.right - v_rect.left, v_rect.bottom - v_rect.top));

		SetForegroundWindow(embedded_hwnd);
	}

	embedded_hwnd = nullptr;
	original_owner = nullptr;
	original_style = 0;
	original_ex_style = 0;
	original_placement = {sizeof(WINDOWPLACEMENT)};
	is_embedded = false;
}

void WindowManager::reposition_embedded(const Rect2i &p_screen_rect) {
	if (!is_embedded || !embedded_hwnd || !IsWindow(embedded_hwnd)) {
		return;
	}
	MoveWindow(embedded_hwnd, p_screen_rect.position.x, p_screen_rect.position.y,
			p_screen_rect.size.x, p_screen_rect.size.y, TRUE);
}

void WindowManager::show_embedded() {
	if (!is_embedded || !embedded_hwnd || !IsWindow(embedded_hwnd)) {
		return;
	}
	SetWindowPos(embedded_hwnd, HWND_TOP, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	ShowWindow(embedded_hwnd, SW_SHOWNOACTIVATE);
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

Vector2i WindowManager::get_client_position(int64_t p_hwnd) const {
	HWND hwnd = reinterpret_cast<HWND>(p_hwnd);
	if (!IsWindow(hwnd)) {
		return Vector2i();
	}
	POINT pt = { 0, 0 };
	ClientToScreen(hwnd, &pt);
	return Vector2i(pt.x, pt.y);
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
	ClassDB::bind_method(D_METHOD("get_client_position", "hwnd"), &WindowManager::get_client_position);
	ClassDB::bind_method(D_METHOD("has_embedded_window"), &WindowManager::has_embedded_window);
}

} // namespace godot
