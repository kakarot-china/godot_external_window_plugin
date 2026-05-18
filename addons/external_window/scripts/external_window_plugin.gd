@tool
extends EditorPlugin

var panel: Control
var current_window_title: String = "外部窗口"


func _has_main_screen() -> bool:
	return true


func _make_visible(visible: bool) -> void:
	if panel:
		panel.visible = visible
		if visible:
			panel.on_tab_activated()
		else:
			panel.on_tab_deactivated()


func _get_plugin_name() -> String:
	return current_window_title


func _get_plugin_icon() -> Texture2D:
	return preload("res://addons/external_window/icons/external_window.svg")


func _enter_tree() -> void:
	panel = preload("res://addons/external_window/scripts/external_window_panel.gd").new()
	panel.plugin = self
	EditorInterface.get_editor_main_screen().add_child(panel)
	_make_visible(false)


func _exit_tree() -> void:
	if panel:
		panel.cleanup()
		panel.get_parent().remove_child(panel)
		panel.queue_free()
		panel = null


func set_window_title(title: String) -> void:
	current_window_title = title
