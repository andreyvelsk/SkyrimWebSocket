#pragma once

#include <boost/asio.hpp>

namespace BroadcastService
{
    void Start(boost::asio::io_context& ioc);
}
