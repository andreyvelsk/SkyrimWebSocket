#include "GameWriter.h"
#include "../Utils.h"

namespace GameWriter
{
    void ApplyClientCommand(const std::string& json)
    {
        PrintConsole("[WS] Received: " + json);
    }
}
