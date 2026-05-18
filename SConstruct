#!/usr/bin/env python
import os

env = SConscript("godot-cpp/SConstruct")

env.Append(CPPPATH=["src/"])
env.Append(LIBS=["user32", "kernel32"])

sources = Glob("src/*.cpp")

library = env.SharedLibrary(
    "addons/external_window/bin/libexternal_window" + env["suffix"],
    source=sources,
)

Default(library)
