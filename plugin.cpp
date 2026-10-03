#include "logger.h"
#include "src/game/EventBus.h"
#include "src/server/WsServer.h"

#include <DbgHelp.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <exception>
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>

static constexpr std::uint16_t DEFAULT_PORT    = 8765;
static constexpr const char*   DEFAULT_ADDRESS = "127.0.0.1";

namespace asio = boost::asio;
using     tcp  = asio::ip::tcp;

static asio::io_context                                       g_ioc;
static std::unique_ptr<WsServer>                              g_server;
static std::thread                                            g_ioThread;
// Keeps g_ioc.run() alive even when there are no async operations scheduled.
static std::unique_ptr<asio::executor_work_guard<
    asio::io_context::executor_type>>                         g_workGuard;

// Address marker used to locate this DLL's HMODULE at runtime.
static const char kModuleLocator = 0;

// Path for the minidump written by the crash handler (set at plugin load).
static std::wstring                    g_dumpPath;
// Previous unhandled-exception filter, chained from our handler.
static LPTOP_LEVEL_EXCEPTION_FILTER    g_prevCrashFilter = nullptr;

// Parse the LogLevel string read from the [Debug] INI section.
// Accepted values (case-insensitive): "trace", "debug", "info".
// Anything else (including the default empty/"off") returns level::off.
static spdlog::level::level_enum ParseLogLevel(const char* str)
{
    if (_stricmp(str, "trace") == 0) return spdlog::level::trace;
    if (_stricmp(str, "debug") == 0) return spdlog::level::debug;
    if (_stricmp(str, "info")  == 0) return spdlog::level::info;
    return spdlog::level::off;
}

// Unhandled-exception filter: flushes the log and writes a minidump next to
// the log file, then chains to any previously registered filter.
static void LogExceptionDetails(EXCEPTION_POINTERS* ep, const char* header)
{
    if (auto* log = spdlog::default_logger_raw()) {
        log->critical("{}", header);
        log->critical("Exception code:    0x{:08X}", ep->ExceptionRecord->ExceptionCode);
        log->critical("Exception address: 0x{:016X}",
                      reinterpret_cast<std::uintptr_t>(ep->ExceptionRecord->ExceptionAddress));
        if (ep->ExceptionRecord->ExceptionCode == 0xC0000005 && ep->ExceptionRecord->NumberParameters >= 2) {
            log->critical("Access violation: {} at 0x{:016X}",
                          ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "read" :
                          ep->ExceptionRecord->ExceptionInformation[0] == 1 ? "write" : "execute",
                          static_cast<std::uint64_t>(ep->ExceptionRecord->ExceptionInformation[1]));
        }

        // Module + RVA for each frame, so the crash site can be matched to
        // SkyrimSE.exe / SkyrimWebSocket.dll without a debugger.
        auto describe = [](std::uint64_t addr) {
            HMODULE mod = nullptr;
            char    name[MAX_PATH] = "<unknown>";
            if (::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCSTR>(addr), &mod) && mod) {
                char full[MAX_PATH] = {};
                if (::GetModuleFileNameA(mod, full, MAX_PATH)) {
                    const char* slash = std::strrchr(full, '\\');
                    strcpy_s(name, slash ? slash + 1 : full);
                }
                return std::format("{}+0x{:X}", name, addr - reinterpret_cast<std::uint64_t>(mod));
            }
            return std::format("{}@0x{:X}", name, addr);
        };

        CONTEXT      ctx   = *ep->ContextRecord;
        STACKFRAME64 frame = {};
        frame.AddrPC.Offset    = ctx.Rip;
        frame.AddrPC.Mode      = AddrModeFlat;
        frame.AddrFrame.Offset = ctx.Rbp;
        frame.AddrFrame.Mode   = AddrModeFlat;
        frame.AddrStack.Offset = ctx.Rsp;
        frame.AddrStack.Mode   = AddrModeFlat;
        const HANDLE process = ::GetCurrentProcess();
        const HANDLE thread  = ::GetCurrentThread();
        ::SymInitialize(process, nullptr, TRUE);
        for (int i = 0; i < 40; ++i) {
            if (!::StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &ctx, nullptr,
                               ::SymFunctionTableAccess64, ::SymGetModuleBase64, nullptr) ||
                frame.AddrPC.Offset == 0) {
                break;
            }
            log->critical("  frame {:2}: {}", i, describe(frame.AddrPC.Offset));
        }
        log->flush();
    }
}

// Vectored handler: sees the exception before any game/Steam/Wine handler
// can swallow it, so the log gets the crash site even when the top-level
// filter never runs. Logs only fatal-looking codes, at most 3 times.
static LONG WINAPI SkyrimWebSocketVectoredHandler(EXCEPTION_POINTERS* ep)
{
    const auto code = ep->ExceptionRecord->ExceptionCode;
    if (code != 0xC0000005 && code != 0xC000001D && code != 0xC00000FD &&
        code != 0xC0000409 && code != 0xC0000374) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    static std::atomic<int> s_count{ 0 };
    if (s_count.fetch_add(1) < 3) {
        LogExceptionDetails(ep, "=== FIRST-CHANCE EXCEPTION (may be fatal) ===");
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static LONG WINAPI SkyrimWebSocketCrashHandler(EXCEPTION_POINTERS* ep)
{
    LogExceptionDetails(ep, "=== CRASH DETECTED ===");

    if (!g_dumpPath.empty()) {
        HANDLE hFile = ::CreateFileW(g_dumpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION info{};
            info.ThreadId          = ::GetCurrentThreadId();
            info.ExceptionPointers = ep;
            info.ClientPointers    = FALSE;
            ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(),
                                hFile, MiniDumpNormal, &info, nullptr, nullptr);
            ::CloseHandle(hFile);
        }
    }

    if (g_prevCrashFilter)
        return g_prevCrashFilter(ep);
    return EXCEPTION_CONTINUE_SEARCH;
}

static std::string GetIniPath()
{
    HMODULE hModule = nullptr;
    if (!::GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            &kModuleLocator,
            &hModule)) {
        return {};
    }

    // Use a growing buffer to handle paths longer than MAX_PATH.
    std::vector<char> buf(MAX_PATH);
    for (;;) {
        const DWORD len = ::GetModuleFileNameA(hModule, buf.data(),
                                               static_cast<DWORD>(buf.size()));
        if (len == 0)
            return {};
        if (len < buf.size() - 1)
            break;
        // Buffer was too small; double it and try again.
        if (buf.size() >= 32 * 1024)
            return {};  // sanity guard
        buf.resize(buf.size() * 2);
    }

    std::string path(buf.data());
    auto lastSlash = path.find_last_of("\\/");
    if (lastSlash != std::string::npos)
        path = path.substr(0, lastSlash);

    return path + "\\SkyrimWebSocket.ini";
}

// ── Diagnostics (temporary) ──────────────────────────────────────────────
// Writes step-by-step breadcrumbs and any structured exception to
// Data/SKSE/Plugins/SkyrimWebSocket_diag.txt using plain Win32 calls only,
// so it works even if spdlog / CommonLib init is what fails.
static void Diag(const char* msg)
{
    HMODULE hModule = nullptr;
    char    path[1024] = {};
    if (::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            &kModuleLocator, &hModule) &&
        ::GetModuleFileNameA(hModule, path, sizeof(path) - 64)) {
        char* slash = std::strrchr(path, '\\');
        if (!slash) slash = std::strrchr(path, '/');
        if (slash) *(slash + 1) = '\0';
    }
    strcat_s(path, "SkyrimWebSocket_diag.txt");
    HANDLE h = ::CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    ::WriteFile(h, msg, static_cast<DWORD>(std::strlen(msg)), &written, nullptr);
    ::WriteFile(h, "\r\n", 2, &written, nullptr);
    ::CloseHandle(h);
}

static int DiagFilter(EXCEPTION_POINTERS* ep)
{
    char        buf[1024];
    const auto  addr = reinterpret_cast<std::uintptr_t>(ep->ExceptionRecord->ExceptionAddress);
    HMODULE     mod  = nullptr;
    char        modName[512] = "<unknown module>";
    std::uintptr_t rva = 0;
    if (::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(addr), &mod) && mod) {
        ::GetModuleFileNameA(mod, modName, sizeof(modName));
        rva = addr - reinterpret_cast<std::uintptr_t>(mod);
    }
    sprintf_s(buf, "EXCEPTION code=0x%08lX addr=0x%016llX module=%s rva=0x%llX",
              ep->ExceptionRecord->ExceptionCode,
              static_cast<unsigned long long>(addr), modName,
              static_cast<unsigned long long>(rva));
    Diag(buf);
    if (ep->ExceptionRecord->ExceptionCode == 0xE06D7363 && ep->ExceptionRecord->NumberParameters >= 3) {
        // MSVC C++ exception: try to print std::exception::what()
        auto* obj = reinterpret_cast<std::exception*>(ep->ExceptionRecord->ExceptionInformation[1]);
        __try {
            sprintf_s(buf, "C++ exception what(): %s", obj ? obj->what() : "<null>");
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            sprintf_s(buf, "C++ exception (not std::exception)");
        }
        Diag(buf);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

static bool LoadImpl(const SKSE::LoadInterface* skse);

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    Diag("---- SKSEPlugin_Load entered ----");
    __try {
        return LoadImpl(skse);
    } __except (DiagFilter(GetExceptionInformation())) {
        Diag("load aborted by exception (see line above)");
        return false;
    }
}

static bool LoadImpl(const SKSE::LoadInterface* skse)
{
    Diag("step 1: REL::Module::get()");
    {
        auto& m = REL::Module::get();
        auto  v = m.version();
        char  b[128];
        sprintf_s(b, "  runtime %u.%u.%u.%u isAE=%d isSE=%d", v[0], v[1], v[2], v[3],
                  (int)REL::Module::IsAE(), (int)REL::Module::IsSE());
        Diag(b);
    }
    Diag("step 2: REL::IDDB::get() (address library)");
    (void)REL::IDDB::get();
    Diag("step 3: PluginVersionData");
    {
        auto* d = SKSE::PluginVersionData::GetSingleton();
        Diag(d ? "  plugin version data OK" : "  plugin version data NULL");
    }
    Diag("step 4: SKSE::log::log_directory()");
    {
        auto dir = SKSE::log::log_directory();
        Diag(dir ? dir->string().c_str() : "  <no log directory>");
    }
    Diag("step 5: SKSE::Init");
    SKSE::Init(skse);
    Diag("step 6: SKSE::Init done");

    // ── Logging setup ────────────────────────────────────────────────────
    // Read LogLevel from [Debug] before anything else so every subsequent
    // log call is already routed to the right sink.
    std::string iniPath = GetIniPath();

    char levelBuf[32] = {};
    ::GetPrivateProfileStringA("Debug", "LogLevel", "off",
                               levelBuf, sizeof(levelBuf), iniPath.c_str());
    const auto logLevel = ParseLogLevel(levelBuf);

    Diag("step 7: SetupLog");
    SetupLog(logLevel);
    Diag("step 8: SetupLog done");

    if (logLevel != spdlog::level::off) {
        logger::info("SkyrimWebSocket starting (LogLevel={})", levelBuf);
        logger::info("INI path: {}", iniPath.empty() ? "(not found)" : iniPath);

        // Pre-compute the minidump path (same folder as the .log file).
        auto logsFolder = SKSE::log::log_directory();
        if (logsFolder) {
            auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
            g_dumpPath = (*logsFolder / std::format("{}.dmp", pluginName)).wstring();
            logger::debug("Minidump path: {}", (*logsFolder / std::format("{}.dmp", pluginName)).string());
        }

        g_prevCrashFilter = ::SetUnhandledExceptionFilter(SkyrimWebSocketCrashHandler);
        ::AddVectoredExceptionHandler(1, SkyrimWebSocketVectoredHandler);
    }

    // ── Server startup ───────────────────────────────────────────────────
    // Start the WS server once, as soon as data files are loaded (i.e. main
    // menu is visible). This lets clients connect before any save is loaded.
    // Field resolvers are responsible for returning null for fields that
    // require an actual in-game session (see FieldRegistry::IsInGame).
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* msg) {
        if (msg->type == SKSE::MessagingInterface::kDataLoaded && !g_server) {
            // Wire up SKSE event sinks for the event-driven optimisation
            // layer.  Must run on the game thread — kDataLoaded is delivered
            // there.
            EventBus::Install();

            std::string iniPath = GetIniPath();

            char addressBuf[64];
            ::GetPrivateProfileStringA(
                "Server", "ListenAddress", DEFAULT_ADDRESS,
                addressBuf, sizeof(addressBuf), iniPath.c_str());

            UINT port = ::GetPrivateProfileIntA("Server", "Port", DEFAULT_PORT, iniPath.c_str());
            if (port == 0 || port > 65535)
                port = DEFAULT_PORT;

            boost::system::error_code ec;
            auto addr = asio::ip::make_address(addressBuf, ec);
            if (ec)
                addr = asio::ip::make_address(DEFAULT_ADDRESS);

            logger::debug("WS server starting on {}:{}", addressBuf, port);

            tcp::endpoint endpoint(addr, static_cast<std::uint16_t>(port));
            g_server = std::make_unique<WsServer>(g_ioc, endpoint);

            // Keep the io_context alive even if there are momentarily no
            // pending operations — avoids the run() thread exiting early.
            g_workGuard = std::make_unique<asio::executor_work_guard<
                asio::io_context::executor_type>>(g_ioc.get_executor());

            g_ioThread  = std::thread([] { g_ioc.run(); });
            g_ioThread.detach();
        }
    });

    Diag("step 9: listener registered, load returning true");
    return true;
}
