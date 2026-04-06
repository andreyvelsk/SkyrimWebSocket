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

        auto* avo        = player->AsActorValueOwner();
        float health     = avo->GetActorValue(RE::ActorValue::kHealth);
        float maxHealth  = avo->GetPermanentActorValue(RE::ActorValue::kHealth);
        float magicka    = avo->GetActorValue(RE::ActorValue::kMagicka);
        float maxMagicka = avo->GetPermanentActorValue(RE::ActorValue::kMagicka);
        float stamina    = avo->GetActorValue(RE::ActorValue::kStamina);
        float maxStamina = avo->GetPermanentActorValue(RE::ActorValue::kStamina);

        if (!std::isfinite(health))     health     = 0.f;
        if (!std::isfinite(maxHealth))  maxHealth  = 0.f;
        if (!std::isfinite(magicka))    magicka    = 0.f;
        if (!std::isfinite(maxMagicka)) maxMagicka = 0.f;
        if (!std::isfinite(stamina))    stamina    = 0.f;
        if (!std::isfinite(maxStamina)) maxStamina = 0.f;

        return std::format(
            R"({{"health":{:.1f},"maxHealth":{:.1f},"magicka":{:.1f},"maxMagicka":{:.1f},"stamina":{:.1f},"maxStamina":{:.1f}}})",
            health, maxHealth, magicka, maxMagicka, stamina, maxStamina);
    }
}
