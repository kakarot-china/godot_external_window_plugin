# External Window Plugin for Godot

A Godot 4.6+ editor plugin that embeds external Windows system windows directly into the Godot editor as a main screen tab.

## Features

- Embed any visible Windows system window into the Godot editor
- Drop-down list to select from all open windows
- Automatic position sync when the editor window moves or resizes
- Hide embedded window when switching to other editor tabs (2D/3D/Script)
- Clean unembed with original window position and style restoration

## Requirements

- Godot 4.6+
- Windows x86_64

## Installation

1. Copy the `addons/external_window/` folder into your project's `addons/` directory
2. Open your project in Godot
3. Go to **Project → Project Settings → Plugins**
4. Enable the "External Window" plugin

## Usage

1. Click the **"外部窗口"** (External Window) tab in the editor's main screen area
2. Select a window from the drop-down list
3. The selected window will be embedded into the editor panel
4. Click **"刷新"** (Refresh) to update the window list
5. Click **"取消嵌入"** (Unembed) to release the embedded window and restore it

## Building from Source

```bash
git clone https://github.com/kakarot-china/godot_external_window_plugin.git
cd godot_external_window_plugin
git submodule update --init --recursive
pip install scons
scons target=template_debug    # Debug build
scons target=template_release  # Release build
```

Requires Visual Studio Build Tools (MSVC) for Windows compilation.

## Limitations

- **Windows only** — relies on Win32 API
- Some Chromium/Electron-based applications (e.g., VS Code) may not render correctly when embedded due to GPU-accelerated rendering

## License

MIT License. See [LICENSE](LICENSE) for details.
