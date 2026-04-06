#pragma once

#include <string>

inline void PrintConsole(const std::string& msg)
{
    if (auto* log = RE::ConsoleLog::GetSingleton())
        log->Print(msg.c_str());
}
