@tool
extends Control

var plugin: EditorPlugin
var wm: RefCounted
var check_timer: Timer
var embedded_hwnd: int = 0
var _last_rect: Rect2i

var toolbar: HBoxContainer
var window_dropdown: OptionButton
var container_panel: Panel


func _ready() -> void:
	# Editor main screen is a VBoxContainer — it uses size_flags, not anchors, to allocate space
	size_flags_horizontal = Control.SIZE_EXPAND_FILL
	size_flags_vertical = Control.SIZE_EXPAND_FILL
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)

	# Instantiate WindowManager from GDExtension
	wm = ClassDB.instantiate("WindowManager")

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
	window_dropdown.item_selected.connect(_on_window_selected)
	toolbar.add_child(window_dropdown)

	var refresh_btn = Button.new()
	refresh_btn.text = "刷新"
	refresh_btn.pressed.connect(_refresh_window_list)
	toolbar.add_child(refresh_btn)

	var unembed_btn = Button.new()
	unembed_btn.text = "取消嵌入"
	unembed_btn.pressed.connect(_do_unembed)
	toolbar.add_child(unembed_btn)

	# Container panel for embedded window
	container_panel = Panel.new()
	container_panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	container_panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	container_panel.resized.connect(_on_container_resized)
	vbox.add_child(container_panel)

	# Check timer for embedded window validity and position sync
	check_timer = Timer.new()
	check_timer.wait_time = 0.5
	check_timer.one_shot = false
	check_timer.timeout.connect(_check_embedded_window)
	add_child(check_timer)

	_refresh_window_list()


func _get_editor_hwnd() -> int:
	var hwnd = DisplayServer.window_get_native_handle(DisplayServer.WINDOW_HANDLE)
	print("[EW] editor_hwnd: ", hwnd, " (0x", ("%X" % hwnd), ")")
	return hwnd


func _get_container_screen_rect() -> Rect2i:
	var scale = EditorInterface.get_editor_scale()
	var panel_rect = container_panel.get_global_rect()
	var editor_hwnd = _get_editor_hwnd()
	var client_pos = wm.get_client_position(editor_hwnd)
	var result = Rect2i(
		client_pos.x + int(panel_rect.position.x * scale),
		client_pos.y + int(panel_rect.position.y * scale),
		int(panel_rect.size.x * scale),
		int(panel_rect.size.y * scale)
	)
	print("[EW] screen_rect: scale=", scale,
		" panel_rect=(", panel_rect.position.x, ",", panel_rect.position.y, " ", panel_rect.size.x, "x", panel_rect.size.y, ")",
		" client_pos=(", client_pos.x, ",", client_pos.y, ")",
		" result=(", result.position.x, ",", result.position.y, " ", result.size.x, "x", result.size.y, ")")
	return result


func _refresh_window_list() -> void:
	window_dropdown.clear()
	window_dropdown.add_item("-- 选择窗口 --", 0)

	var editor_pid = OS.get_process_id()
	var windows = wm.get_visible_windows()

	for i in range(windows.size()):
		var entry: Dictionary = windows[i]
		var hwnd_val: int = int(entry["hwnd"])
		var title: String = str(entry["title"])
		var pid: int = int(entry["pid"])

		# Skip all windows belonging to the Godot editor process
		if pid == editor_pid:
			continue

		window_dropdown.add_item(title, hwnd_val)

	# Re-add embedded window if it's still valid (it won't appear in EnumWindows
	# because style change may hide it from enumeration)
	if embedded_hwnd != 0 and wm.is_window_valid(embedded_hwnd):
		var embedded_title = wm.get_window_title(embedded_hwnd)
		if embedded_title.is_empty():
			embedded_title = "未命名窗口"
		window_dropdown.add_item(embedded_title + " (已嵌入)", embedded_hwnd)
		window_dropdown.select(window_dropdown.item_count - 1)

	window_dropdown.disabled = window_dropdown.item_count <= 1


func _on_window_selected(index: int) -> void:
	var hwnd_val = window_dropdown.get_item_id(index)
	if hwnd_val == 0:
		return

	print("[EW] ===== SELECT WINDOW: hwnd=0x", ("%X" % hwnd_val), " title=", window_dropdown.get_item_text(index))

	# Unembed current window if any
	_do_unembed()

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
		check_timer.start()
	else:
		embedded_hwnd = 0
		window_dropdown.select(0)


func _do_unembed() -> void:
	if embedded_hwnd != 0:
		print("[EW] ===== UNEMBED: hwnd=0x", ("%X" % embedded_hwnd))
		wm.unembed_window()
		embedded_hwnd = 0
		_last_rect = Rect2i()
		if plugin:
			plugin.set_window_title("外部窗口")
	check_timer.stop()


func _check_embedded_window() -> void:
	if embedded_hwnd != 0:
		if not wm.is_window_valid(embedded_hwnd):
			_do_unembed()
			_refresh_window_list()
			return
		# Sync position when editor window moves
		var rect = _get_container_screen_rect()
		if rect != _last_rect:
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
