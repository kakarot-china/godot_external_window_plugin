#include "register_types.h"
#include "window_manager.h"

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_external_window_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_class<WindowManager>();
}

void uninitialize_external_window_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

extern "C" {
GDExtensionBool GDE_EXPORT gdextension_initialize(GDExtensionInterface *p_interface, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_interface, p_library, r_initialization);
	init_obj.register_initializer(initialize_external_window_module);
	init_obj.register_terminator(uninitialize_external_window_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
	return init_obj.init();
}
}
