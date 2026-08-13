#include "logger.h"
#include "src/Network.h"
#include "src/Utils.h"
#include "src/game/EventBus.h"
#include "src/server/WsServer.h"

#include <DbgHelp.h>
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>

static constexpr std::uint16_t DEFAULT_PORT = 8765;

namespace asio = boost::asio;
using     tcp  = asio::ip::tcp;

static asio::io_context                          g_ioc;
static std::vector<std::unique_ptr<WsServer>>    g_servers;
static std::thread                               g_ioThread;
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
static LONG WINAPI SkyrimWebSocketCrashHandler(EXCEPTION_POINTERS* ep)
{
    if (auto* log = spdlog::default_logger_raw()) {
        log->critical("=== CRASH DETECTED ===");
        log->critical("Exception code:    0x{:08X}", ep->ExceptionRecord->ExceptionCode);
        log->critical("Exception address: 0x{:016X}",
                      reinterpret_cast<std::uintptr_t>(ep->ExceptionRecord->ExceptionAddress));
        log->flush();
    }

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

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);

    // ── Logging setup ────────────────────────────────────────────────────
    // Read LogLevel from [Debug] before anything else so every subsequent
    // log call is already routed to the right sink.
    std::string iniPath = GetIniPath();

    char levelBuf[32] = {};
    ::GetPrivateProfileStringA("Debug", "LogLevel", "off",
                               levelBuf, sizeof(levelBuf), iniPath.c_str());
    const auto logLevel = ParseLogLevel(levelBuf);

    SetupLog(logLevel);

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
    }

    // ── Server startup ───────────────────────────────────────────────────
    // Start the WS server once, as soon as data files are loaded (i.e. main
    // menu is visible). This lets clients connect before any save is loaded.
    // Field resolvers are responsible for returning null for fields that
    // require an actual in-game session (see FieldRegistry::IsInGame).
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* msg) {
        if (msg->type == SKSE::MessagingInterface::kDataLoaded && g_servers.empty()) {
            // Wire up SKSE event sinks for the event-driven optimisation
            // layer.  Must run on the game thread — kDataLoaded is delivered
            // there.
            EventBus::Install();

            std::string iniPath = GetIniPath();

            char addressBuf[256] = {};
            ::GetPrivateProfileStringA(
                "Server", "ListenAddress", "",
                addressBuf, sizeof(addressBuf), iniPath.c_str());

            UINT port = ::GetPrivateProfileIntA("Server", "Port", DEFAULT_PORT, iniPath.c_str());
            if (port == 0 || port > 65535)
                port = DEFAULT_PORT;

            // Empty or absent ListenAddress => bind to loopback plus every
            // automatically detected IPv4 address of the local interfaces,
            // so devices on the local subnets can connect by default.
            std::vector<std::string> addresses;
            if (addressBuf[0] != '\0') {
                addresses.emplace_back(addressBuf);
            } else {
                addresses.emplace_back("127.0.0.1");
                auto locals = EnumerateLocalIPv4Addresses();
                addresses.insert(addresses.end(), locals.begin(), locals.end());
            }

            logger::debug("WS server starting on port {}", port);

            for (const auto& addrStr : addresses) {
                boost::system::error_code ec;
                auto addr = asio::ip::make_address(addrStr, ec);
                if (ec) {
                    logger::error("WS server: invalid listen address '{}': {}",
                                  addrStr, ec.message());
                    continue;
                }

                tcp::endpoint endpoint(addr, static_cast<std::uint16_t>(port));
                auto server = std::make_unique<WsServer>(g_ioc, endpoint);
                if (server->ok())
                    g_servers.push_back(std::move(server));
                // On failure WsServer already logged the reason.
            }

            if (g_servers.empty()) {
                logger::error("WS server: failed to bind on any address");
                SKSE::GetTaskInterface()->AddTask([] {
                    PrintConsole("[WS] Server failed to bind on any address");
                });
                return;
            }

            // Keep the io_context alive even if there are momentarily no
            // pending operations — avoids the run() thread exiting early.
            g_workGuard = std::make_unique<asio::executor_work_guard<
                asio::io_context::executor_type>>(g_ioc.get_executor());

            g_ioThread  = std::thread([] { g_ioc.run(); });
            g_ioThread.detach();
        }
    });

    return true;
}
