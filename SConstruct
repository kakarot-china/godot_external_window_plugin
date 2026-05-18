#!/usr/bin/env python
import os

env = SConscript("godot-cpp/SConstruct")

env.Append(CPPPATH=["src/"])
env.Append(LIBS=["user32", "kernel32"])

sources = Glob("src/*.cpp")

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "addons/external_window/bin/libexternal_window.{}.{}.{}".format(
            env["platform"], env["target"], env["arch"]
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "addons/external_window/bin/libexternal_window{}{}".format(
            env["suffix"], env["SHLIBSUFFIX"]
        ),
        source=sources,
    )

Default(library)
