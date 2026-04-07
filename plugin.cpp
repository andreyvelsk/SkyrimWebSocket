#include "logger.h"
#include "src/server/WsServer.h"

#include <boost/asio.hpp>
#include <memory>
#include <thread>

static constexpr std::uint16_t DEFAULT_PORT    = 8765;
static constexpr const char*   DEFAULT_ADDRESS = "127.0.0.1";

namespace asio = boost::asio;
using     tcp  = asio::ip::tcp;

static asio::io_context          g_ioc;
static std::unique_ptr<WsServer> g_server;
static std::thread               g_ioThread;

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

    char dllPath[MAX_PATH];
    ::GetModuleFileNameA(hModule, dllPath, MAX_PATH);

    std::string path(dllPath);
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
            g_server    = std::make_unique<WsServer>(g_ioc, endpoint);
            g_ioThread  = std::thread([] { g_ioc.run(); });
            g_ioThread.detach();
        }
    });

    return true;
}
