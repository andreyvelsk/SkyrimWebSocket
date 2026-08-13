#include "ConsoleHook.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace ConsoleHook
{
    // ─── Internal state ───────────────────────────────────────────────────

    // True once Install() has patched the trampoline.
    static std::atomic<bool> s_installed{ false };

    // Mutex protecting the capture state.
    static std::mutex s_mutex;

    // Whether a capture session is currently active.
    static bool s_capturing = false;

    // Accumulated output for the current capture session.
    static std::string s_buffer;

    // Signature of ConsoleLog::VPrint (member function, __thiscall on x86,
    // but on x64 Windows the first arg is `this`).
    using VPrint_t = void(RE::ConsoleLog*, const char*, std::va_list);

    // Pointer to the original VPrint code, set by write_branch (trampoline).
    static VPrint_t* s_originalVPrint = nullptr;

    // ─── Hook function ────────────────────────────────────────────────────

    // Our replacement for ConsoleLog::VPrint.
    // Calls the original first (so the console UI still shows output),
    // then reads lastMessage (which the original just wrote) into our buffer.
    static void Hook_VPrint(RE::ConsoleLog* self, const char* fmt, std::va_list args)
    {
        // Always call the original so the console UI still shows the output.
        if (s_originalVPrint)
            s_originalVPrint(self, fmt, args);

        // Capture if a session is active.
        if (!self)
            return;

        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_capturing)
            return;

        // The original VPrint writes the formatted result into lastMessage.
        if (self->lastMessage[0] != '\0') {
            if (!s_buffer.empty())
                s_buffer += '\n';
            s_buffer += self->lastMessage;
        }
    }

    // ─── Public API ───────────────────────────────────────────────────────

    void Install()
    {
        // Only install once.
        bool expected = false;
        if (!s_installed.compare_exchange_strong(expected, true))
            return;

        // Allocate trampoline space.
        // write_branch<5> needs 14 bytes for the trampoline stub.
        SKSE::AllocTrampoline(14);

        // write_branch<5> at the address of VPrint:
        //   - Writes a JMP rel32 at the start of VPrint → Hook_VPrint
        //   - Returns the address of the original code (trampoline stub that
        //     executes the overwritten bytes then jumps back)
        REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(50180, 51110) };
        const auto originalAddr = SKSE::GetTrampoline().write_branch<5>(
            target.address(),
            reinterpret_cast<std::uintptr_t>(&Hook_VPrint));

        s_originalVPrint = reinterpret_cast<VPrint_t*>(originalAddr);
    }

    void BeginCapture()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_capturing = true;
        s_buffer.clear();
    }

    std::optional<std::string> EndCapture()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_capturing)
            return std::nullopt;
        s_capturing = false;
        return std::move(s_buffer);
    }
}
