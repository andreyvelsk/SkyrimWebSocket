#pragma once

#include <string>
#include <vector>

// Returns the IPv4 unicast addresses of the local network interfaces,
// excluding loopback (127.x.x.x) and the unspecified address (0.0.0.0).
// Used to bind the WebSocket server to every local subnet by default.
std::vector<std::string> EnumerateLocalIPv4Addresses();
