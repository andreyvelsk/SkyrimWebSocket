# SkyrimWebSocket

- [SkyrimWebSocket](#skyrimwebsocket)
  - [What does it do?](#what-does-it-do)
  - [Requirements](#requirements)
  - [Project setup](#project-setup)
  - [Configuration](#configuration)

---

An SKSE plugin for Skyrim that provides a WebSocket server interface for remote game state monitoring and control. Built with:

- C++
- CMake
- [CommonLibSSE NG](https://github.com/CharmedBaryon/CommonLibSSE-NG)
  - _automatically downloaded using vcpkg integration of CMake_

> This plugin supports Skyrim SSE, AE, GOG, and VR through CommonLibSSE NG.

## What does it do?

SkyrimWebSocket runs a WebSocket server that allows external clients to:

- **Subscribe** to game data fields with configurable push intervals (min 50ms)
- **Query** game data on demand
- **Receive notifications** when game values change
- **Monitor** player state, world data, and other game information in real-time

See [PROTOCOL.md](PROTOCOL.md) for detailed message specifications and examples.

## Requirements

- [Visual Studio 2022](https://visualstudio.microsoft.com/) (Community edition is fine)
- [CMake](https://cmake.org/download/) 3.25.1+
- [`vcpkg`](https://github.com/microsoft/vcpkg):
  1. Clone or download vcpkg repository
  2. Run `bootstrap-vcpkg.bat`
  3. Set environment variable `VCPKG_ROOT` to the vcpkg folder path

You can open this project in VS Code, CLion, or Visual Studio. For VS Code, ensure you have the [C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) and [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extensions installed.

The project will automatically download CommonLibSSE NG and all dependencies via CMake and vcpkg.

## Project setup

By default, the compiled plugin DLL is output to `build/`. 

To deploy to your Skyrim installation automatically, set environment variables:

- `SKYRIM_FOLDER`: Path to your Skyrim Special Edition install  
  Example: `C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition`

- `SKYRIM_MODS_FOLDER`: Path to your mods folder (for Mod Organizer 2 or Vortex)  
  Example: `C:\Users\<user>\AppData\Local\ModOrganizer\Skyrim Special Edition\mods`

## Configuration

The plugin reads an optional INI file placed next to the DLL:

```
Data/SKSE/Plugins/SkyrimWebSocket.ini
```

If the file is absent, the plugin uses safe defaults (`127.0.0.1:8765`).

| Key | Default | Description |
|---|---|---|
| `[Server] ListenAddress` | `127.0.0.1` | Bind address. Use `0.0.0.0` to accept remote connections (e.g. for debugging). |
| `[Server] Port` | `8765` | TCP port the WebSocket server listens on. |

An annotated template is available in the repository as `SkyrimWebSocket.ini.example`.

> 📜 based on template by https://github.com/SkyrimScripting/SKSE_Templates