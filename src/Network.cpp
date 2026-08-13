#include "Network.h"

// Winsock2 must be included before iphlpapi.h (which relies on the Winsock
// socket types). Boost.Asio may already have pulled winsock2.h in, but the
// include guard keeps this safe either way.
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include <cstdint>
#include <vector>

std::vector<std::string> EnumerateLocalIPv4Addresses()
{
    std::vector<std::string> result;

    const ULONG flags =
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;

    ULONG bufLen = 0;
    ULONG err = ::GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &bufLen);
    if (err != ERROR_BUFFER_OVERFLOW || bufLen == 0)
        return result;

    std::vector<BYTE> buf(bufLen);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    err = ::GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &bufLen);
    if (err != NO_ERROR)
        return result;

    for (auto* adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
        for (auto* addr = adapter->FirstUnicastAddress; addr != nullptr; addr = addr->Next) {
            const auto* sock = addr->Address.lpSockaddr;
            if (sock == nullptr || sock->sa_family != AF_INET)
                continue;

            const auto* in = reinterpret_cast<const sockaddr_in*>(sock);
            char ip[INET_ADDRSTRLEN] = {};
            if (::inet_ntop(AF_INET, &in->sin_addr, ip, sizeof(ip)) == nullptr)
                continue;

            std::string s(ip);
            // Skip the unspecified address and the whole loopback range.
            if (s == "0.0.0.0" || s.rfind("127.", 0) == 0)
                continue;

            result.push_back(std::move(s));
        }
    }

    return result;
}
