#include "GameReader.h"

#include <cmath>
#include <format>

namespace GameReader
{
    std::string BuildPlayerStateJson()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {};

        auto* avo     = player->AsActorValueOwner();
        float health  = avo->GetActorValue(RE::ActorValue::kHealth);
        float magicka = avo->GetActorValue(RE::ActorValue::kMagicka);
        float stamina = avo->GetActorValue(RE::ActorValue::kStamina);

        if (!std::isfinite(health))  health  = 0.f;
        if (!std::isfinite(magicka)) magicka = 0.f;
        if (!std::isfinite(stamina)) stamina = 0.f;

        return std::format(R"({{"health":{:.1f},"magicka":{:.1f},"stamina":{:.1f}}})",
            health, magicka, stamina);
    }
}
