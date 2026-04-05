#include "logger.h"

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SKSE::Init(skse);
    SetupLog();

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message *msg) {
        if (msg->type == SKSE::MessagingInterface::kPostLoadGame) {
            RE::ConsoleLog::GetSingleton()->Print("Hello world - debug");
        }
    });

    return true;
}
