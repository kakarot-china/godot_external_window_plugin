@tool
extends Control

var plugin: EditorPlugin
var wm: RefCounted
var check_timer: Timer
var refresh_timer: Timer
var embedded_hwnd: int = 0
var _last_rect: Rect2i
var frequent_windows: Dictionary = {}

var toolbar: HBoxContainer
var window_dropdown: OptionButton
var action_btn: Button
var container_panel: Panel


func _ready() -> void:
	# Editor main screen is a VBoxContainer — it uses size_flags, not anchors, to allocate space
	size_flags_horizontal = Control.SIZE_EXPAND_FILL
	size_flags_vertical = Control.SIZE_EXPAND_FILL
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)

	# Instantiate WindowManager from GDExtension
	wm = ClassDB.instantiate("WindowManager")

	# Load persistent frequency data
	_load_frequent_windows()

	# Build UI
	var vbox = VBoxContainer.new()
	vbox.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(vbox)

	# Toolbar
	toolbar = HBoxContainer.new()
	vbox.add_child(toolbar)

	var label = Label.new()
	label.text = "选择窗口："
	toolbar.add_child(label)

	window_dropdown = OptionButton.new()
	window_dropdown.custom_minimum_size = Vector2i(300, 0)
	toolbar.add_child(window_dropdown)

	action_btn = Button.new()
	action_btn.text = "嵌入"
	action_btn.pressed.connect(_on_action_btn_pressed)
	toolbar.add_child(action_btn)

	# Container panel for embedded window
	container_panel = Panel.new()
	container_panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	container_panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	container_panel.resized.connect(_on_container_resized)
	vbox.add_child(container_panel)

	# Check timer for embedded window validity and position lock
	check_timer = Timer.new()
	check_timer.wait_time = 0.5
	check_timer.one_shot = false
	check_timer.timeout.connect(_check_embedded_window)
	add_child(check_timer)

	# Auto-refresh timer for window list
	refresh_timer = Timer.new()
	refresh_timer.wait_time = _get_refresh_interval()
	refresh_timer.one_shot = false
	refresh_timer.timeout.connect(_refresh_window_list)
	add_child(refresh_timer)
	refresh_timer.start()

	_refresh_window_list()


func _get_editor_hwnd() -> int:
	return DisplayServer.window_get_native_handle(DisplayServer.WINDOW_HANDLE)


func _get_container_screen_rect() -> Rect2i:
	var panel_rect = container_panel.get_global_rect()
	var editor_hwnd = _get_editor_hwnd()
	var client_pos = wm.get_client_position(editor_hwnd)
	return Rect2i(
		client_pos.x + int(panel_rect.position.x),
		client_pos.y + int(panel_rect.position.y),
		int(panel_rect.size.x),
		int(panel_rect.size.y)
	)


# --- Editor Settings helpers ---

func _get_refresh_interval() -> float:
	var settings = EditorInterface.get_editor_settings()
	if settings.has_setting("external_window/refresh_interval"):
		var val = settings.get_setting("external_window/refresh_interval")
		if val is float and val > 0.0:
			return val
	return 5.0


func _get_sort_mode() -> int:
	# 0 = recent (default), 1 = by count
	var settings = EditorInterface.get_editor_settings()
	if settings.has_setting("external_window/sort_mode"):
		var val = settings.get_setting("external_window/sort_mode")
		if val is int:
			return val
	return 0


func _load_frequent_windows() -> void:
	var settings = EditorInterface.get_editor_settings()
	if settings.has_setting("external_window/frequent_windows"):
		var val = settings.get_setting("external_window/frequent_windows")
		if val is Dictionary:
			frequent_windows = val


func _save_frequent_windows() -> void:
	var settings = EditorInterface.get_editor_settings()
	settings.set_setting("external_window/frequent_windows", frequent_windows)


func _extract_match_key(title: String) -> String:
	var idx = title.rfind(" - ")
	if idx >= 0:
		return title.substr(idx + 3).strip_edges()
	return title.strip_edges()


# --- Window list ---

func _refresh_window_list() -> void:
	refresh_timer.wait_time = _get_refresh_interval()

	# Save current selection
	var saved_title = ""
	if window_dropdown.selected > 0:
		saved_title = window_dropdown.get_item_text(window_dropdown.selected)

	window_dropdown.clear()
	window_dropdown.add_item("-- 选择窗口 --", 0)

	var editor_pid = OS.get_process_id()
	var windows = wm.get_visible_windows()

	# Build candidates with frequency scores
	var candidates: Array = []
	for i in range(windows.size()):
		var entry: Dictionary = windows[i]
		var hwnd_val: int = int(entry["hwnd"])
		var title: String = str(entry["title"])
		var pid: int = int(entry["pid"])

		if pid == editor_pid:
			continue

		var match_key = _extract_match_key(title)
		var score: float = 0.0
		if frequent_windows.has(match_key):
			var freq = frequent_windows[match_key]
			if _get_sort_mode() == 1:
				score = float(freq.get("count", 0))
			else:
				score = float(freq.get("last_used", 0.0))
		candidates.append({"hwnd": hwnd_val, "title": title, "score": score})

	# Sort: higher score first
	candidates.sort_custom(func(a, b): return a["score"] > b["score"])

	for entry in candidates:
		window_dropdown.add_item(entry["title"], entry["hwnd"])

	# Re-add embedded window if it's still valid
	if embedded_hwnd != 0 and wm.is_window_valid(embedded_hwnd):
		var embedded_title = wm.get_window_title(embedded_hwnd)
		if embedded_title.is_empty():
			embedded_title = "未命名窗口"
		window_dropdown.add_item(embedded_title + " (已嵌入)", embedded_hwnd)
		window_dropdown.select(window_dropdown.item_count - 1)
	elif not saved_title.is_empty():
		# Restore previous selection
		for i in range(window_dropdown.item_count):
			if window_dropdown.get_item_text(i) == saved_title:
				window_dropdown.select(i)
				break

	window_dropdown.disabled = window_dropdown.item_count <= 1
	_update_action_btn()


# --- Action button (embed/cancel toggle) ---

func _update_action_btn() -> void:
	if action_btn == null:
		return
	if embedded_hwnd != 0:
		action_btn.text = "取消"
	else:
		action_btn.text = "嵌入"


func _on_action_btn_pressed() -> void:
	if embedded_hwnd != 0:
		_do_unembed()
	else:
		_do_embed_selected()


func _do_embed_selected() -> void:
	var index = window_dropdown.selected
	if index <= 0:
		return
	var hwnd_val = window_dropdown.get_item_id(index)
	if hwnd_val == 0:
		return

	print("[EW] ===== EMBED: hwnd=0x", ("%X" % hwnd_val),
		" title=", window_dropdown.get_item_text(index))

	var editor_hwnd = _get_editor_hwnd()
	var rect = _get_container_screen_rect()

	if wm.embed_window(hwnd_val, editor_hwnd, rect):
		embedded_hwnd = hwnd_val
		_last_rect = rect
		var title = wm.get_window_title(hwnd_val)
		if title.is_empty():
			title = "未命名窗口"
		if plugin:
			plugin.set_window_title(title)

		# Track frequency
		var match_key = _extract_match_key(title)
		if frequent_windows.has(match_key):
			var freq = frequent_windows[match_key]
			freq["count"] = int(freq.get("count", 0)) + 1
			freq["last_used"] = Time.get_unix_time_from_system()
		else:
			frequent_windows[match_key] = {
				"count": 1,
				"last_used": Time.get_unix_time_from_system()
			}
		_save_frequent_windows()

		check_timer.start()
		_update_action_btn()
	else:
		embedded_hwnd = 0
		window_dropdown.select(0)
		_update_action_btn()


func _do_unembed() -> void:
	if embedded_hwnd != 0:
		print("[EW] ===== UNEMBED: hwnd=0x", ("%X" % embedded_hwnd))
		wm.unembed_window()
		embedded_hwnd = 0
		_last_rect = Rect2i()
		if plugin:
			plugin.set_window_title("外部窗口")
	check_timer.stop()
	_update_action_btn()


# --- Timers ---

func _check_embedded_window() -> void:
	if embedded_hwnd != 0:
		if not wm.is_window_valid(embedded_hwnd):
			_do_unembed()
			_refresh_window_list()
			return
		# Always reposition to lock window in place
		var rect = _get_container_screen_rect()
		wm.reposition_embedded(rect)
		_last_rect = rect


func _on_container_resized() -> void:
	if embedded_hwnd != 0 and wm.has_embedded_window():
		var rect = _get_container_screen_rect()
		wm.reposition_embedded(rect)
		_last_rect = rect


func on_tab_activated() -> void:
	if embedded_hwnd != 0 and wm.has_embedded_window():
		var rect = _get_container_screen_rect()
		wm.reposition_embedded(rect)
		_last_rect = rect
		wm.show_embedded()


func on_tab_deactivated() -> void:
	if wm.has_embedded_window():
		wm.hide_embedded()


func cleanup() -> void:
	_do_unembed()
	if check_timer:
		check_timer.stop()
	if refresh_timer:
		refresh_timer.stop()
