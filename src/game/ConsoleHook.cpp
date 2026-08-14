#include "ConsoleHook.h"

#include "../../logger.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

    // Pointer to the original VPrint code, set by Install().
    //
    // This is EITHER the real VPrint function (when the RELOCATION_ID points
    // at a `jmp rel32` thunk) OR a small trampoline we built ourselves (when
    // the RELOCATION_ID points directly at the function body).
    static VPrint_t* s_originalVPrint = nullptr;

    // ─── Hook function ────────────────────────────────────────────────────

    // Our replacement for ConsoleLog::VPrint.
    // Calls the original first (so the console UI still shows output),
    // then reads lastMessage (which the original just wrote) into our buffer.
    static void Hook_VPrint(RE::ConsoleLog* self, const char* fmt, std::va_list args)
    {
        // Log the very first invocation so a crash here can be distinguished
        // from a crash inside Install().
        static std::atomic<bool> s_firstCallLogged{ false };
        if (!s_firstCallLogged.exchange(true)) {
            logger::info("[ConsoleHook] Hook_VPrint first call: self=0x{:016X} "
                         "fmt={} original=0x{:016X}",
                         reinterpret_cast<std::uintptr_t>(self),
                         fmt ? fmt : "(null)",
                         reinterpret_cast<std::uintptr_t>(s_originalVPrint));
        }

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

        REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(50180, 51110) };
        const auto                       targetAddr = target.address();

        logger::info("[ConsoleHook] Install() begin");
        logger::info("[ConsoleHook] VPrint target address: 0x{:016X}", targetAddr);

        if (targetAddr == 0) {
            logger::error("[ConsoleHook] RELOCATION_ID resolved to null; "
                          "ConsoleHook NOT installed (game will keep running)");
            return;
        }

        // Determine whether the address is a `jmp rel32` thunk (E9 xx xx xx xx)
        // or the real function prologue. write_branch<5> only returns a valid
        // original function for the thunk case.
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(targetAddr);
        logger::info("[ConsoleHook] First bytes at target: "
                     "{:02X} {:02X} {:02X} {:02X} {:02X}",
                     static_cast<unsigned>(bytes[0]),
                     static_cast<unsigned>(bytes[1]),
                     static_cast<unsigned>(bytes[2]),
                     static_cast<unsigned>(bytes[3]),
                     static_cast<unsigned>(bytes[4]));

        // Reserve space for:
        //   * the 14-byte indirect-jump stub used by write_branch<5>, and
        //   * (real-function case) 14 bytes saved prologue + 14 bytes jmp-back.
        SKSE::AllocTrampoline(64);

        std::uintptr_t originalAddr = 0;

        if (bytes[0] == 0xE9) {
            // Case A: the address is a 5-byte `jmp rel32` thunk. write_branch<5>
            // overwrites the thunk and returns the real function (its target).
            logger::info("[ConsoleHook] Target looks like a JMP thunk (E9); "
                         "using write_branch<5> result as original");
            originalAddr = SKSE::GetTrampoline().write_branch<5>(
                targetAddr,
                reinterpret_cast<std::uintptr_t>(&Hook_VPrint));
        } else {
            // Case B: the address is the real function body. write_branch<5> can
            // still redirect the entry point, but its return value is garbage, so
            // build a trampoline that re-executes the overwritten prologue.
            logger::info("[ConsoleHook] Target is a real function; "
                         "building prologue-preserving trampoline");

            constexpr std::size_t kPrologueSize = 14;  // 3 typical x64 prologue movs
            constexpr std::size_t kJmpBackSize  = 14;  // FF 25 [rip+0] + abs64

            auto* cave = static_cast<std::uint8_t*>(
                SKSE::GetTrampoline().allocate(kPrologueSize + kJmpBackSize));

            // Save the overwritten prologue.
            std::memcpy(cave, bytes, kPrologueSize);

            // Append `FF 25 00000000 <abs64>` = jmp [rip+0] → targetAddr + kPrologueSize.
            std::uint8_t jmpBack[kJmpBackSize] = {
                0xFF, 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            };
            const auto resume64 =
                static_cast<std::uint64_t>(targetAddr + kPrologueSize);
            std::memcpy(jmpBack + 6, &resume64, sizeof(resume64));
            std::memcpy(cave + kPrologueSize, jmpBack, kJmpBackSize);

            // Redirect the function entry to our hook (ignore the return value).
            SKSE::GetTrampoline().write_branch<5>(
                targetAddr,
                reinterpret_cast<std::uintptr_t>(&Hook_VPrint));

            originalAddr = reinterpret_cast<std::uintptr_t>(cave);
            logger::info("[ConsoleHook] Trampoline cave: 0x{:016X} (resume 0x{:016X})",
                         originalAddr,
                         static_cast<std::uintptr_t>(targetAddr + kPrologueSize));
        }

        logger::info("[ConsoleHook] Original VPrint resolved to: 0x{:016X}", originalAddr);
        if (originalAddr == 0) {
            logger::error("[ConsoleHook] Failed to resolve original VPrint; "
                          "hook NOT armed (game will keep running)");
            return;
        }

        s_originalVPrint = reinterpret_cast<VPrint_t*>(originalAddr);
        logger::info("[ConsoleHook] Install() complete; hook armed");
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
