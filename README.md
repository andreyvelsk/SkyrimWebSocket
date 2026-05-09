# SkyrimWebSocket
> 📜 based on template by https://github.com/SkyrimScripting/SKSE_Templates
>
> frontend part of project here - https://andreyvelsk.github.io/SkyrimWebMonitor/

- [SkyrimWebSocket](#skyrimwebsocket)
  - [What does it do?](#what-does-it-do)
  - [Installation](#installation)
  - [Build requirements](#build-requirements)
  - [Project setup](#project-setup)
  - [Configuration](#configuration)
  - [Bug reports](#bug-reports)

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

## Installation

1. **Install SKSE64** — [skse.silverlock.org](https://skse.silverlock.org/)  
   > After installation, always launch Skyrim through `skse64_loader.exe`, not through Steam.

2. **Install Address Library for SKSE Plugins** — [Nexus Mods](https://www.nexusmods.com/skyrimspecialedition/mods/32444)  
   The recommended variant is **All in one (all game versions)**.

3. **Copy the plugin** — download `SkyrimWebSocket.dll` from the [latest release](https://github.com/andreyvelsk/SkyrimWebSocket/releases/latest) and place it in:
   ```
   Data/SKSE/Plugins/SkyrimWebSocket.dll
   ```

Optionally, place `SkyrimWebSocket.ini` next to the DLL to customize the server address and port (see [Configuration](#configuration)).

## Build requirements

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
| `[Debug] LogLevel` | `off` | Log verbosity: `off`, `info`, `debug`, `trace`. |

### Log levels

| Level | What is logged |
|---|---|
| `off` | Nothing (default, recommended for normal use) |
| `info` | Server start/stop, client connect/disconnect, command results |
| `debug` | All of the above + every received message, subscription details |
| `trace` | Maximum verbosity, including every subscription poll cycle |

When any level other than `off` is set, the plugin also installs a crash handler that writes a minidump (`.dmp`) next to the log file on game crash.

Log file location:
```
%USERPROFILE%\Documents\My Games\Skyrim Special Edition\SKSE\SkyrimWebSocket.log
```

An annotated template is available in the repository as `SkyrimWebSocket.ini.example`.

## Bug reports

If you encounter a bug, please [open a GitHub issue](https://github.com/andreyvelsk/SkyrimWebSocket/issues/new) and include:

1. A description of the problem and steps to reproduce it.
2. A **trace-level log** captured during reproduction — set `LogLevel=trace` in your INI file, reproduce the issue, then attach the log file:
   ```
   %USERPROFILE%\Documents\My Games\Skyrim Special Edition\SKSE\SkyrimWebSocket.log
   ```
3. If the game crashed, also attach the `.dmp` file found in the same folder.

## Credits

- *The Elder Scrolls V: Skyrim* is © Bethesda Softworks / ZeniMax. This is an unofficial fan-made plugin and is not affiliated with or endorsed by Bethesda.
