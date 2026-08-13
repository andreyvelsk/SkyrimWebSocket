#pragma once

#include <optional>
#include <string>

// ConsoleHook — intercepts ConsoleLog::VPrint to capture console output.
//
// Usage:
//   1. Call ConsoleHook::Install() once during plugin load (after SKSE::Init).
//   2. Before executing a console command, call ConsoleHook::BeginCapture().
//   3. Execute the command via RE::Console::ExecuteCommand().
//   4. Call ConsoleHook::EndCapture() to retrieve accumulated output.
//
// Thread safety: capture state is guarded by a mutex; only one capture session
// can be active at a time. Concurrent calls to BeginCapture() will block until
// the previous session ends.
//
// All captured lines are joined with '\n'. The returned string is empty when
// the command produced no output.

namespace ConsoleHook
{
    // Install the VPrint hook. Must be called once on the game thread during
    // plugin load (kDataLoaded or earlier). Safe to call multiple times — only
    // the first call installs the hook.
    void Install();

    // Begin accumulating console output into an internal buffer.
    // Clears any previously accumulated text.
    void BeginCapture();

    // Stop accumulating and return everything printed since BeginCapture().
    // Returns std::nullopt when no capture session was active.
    std::optional<std::string> EndCapture();
}
