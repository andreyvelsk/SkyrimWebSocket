#include "logger.h"
#include "src/server/WsServer.h"

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
    SetupLog();

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* msg) {
        if (msg->type == SKSE::MessagingInterface::kPostLoadGame && !g_server) {
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

    return true;
}
