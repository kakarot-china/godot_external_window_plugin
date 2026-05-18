#!/usr/bin/env python
import os

env = SConscript("godot-cpp/SConstruct")

env.Append(CPPPATH=["src/"])
env.Append(LIBS=["user32", "kernel32"])

sources = Glob("src/*.cpp")

platform_str = str(env["platform"])
target_str = str(env["target"])
arch_str = "x86_64"

output_name = "addons/external_window/bin/libexternal_window.{}.{}.{}".format(
    platform_str, target_str, arch_str
)

library = env.SharedLibrary(output_name, source=sources)

Default(library)
