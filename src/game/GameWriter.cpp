#include "GameWriter.h"
#include "InventoryWriter.h"

#include <nlohmann/json.hpp>

namespace GameWriter
{
    // ─── Helpers ──────────────────────────────────────────────────────────

    static std::uint32_t ParseFormId(const nlohmann::json& val)
    {
        if (val.is_number_unsigned())
            return val.get<std::uint32_t>();
        if (val.is_string()) {
            try {
                // base 0 auto-detects "0x" prefix
                return static_cast<std::uint32_t>(
                    std::stoul(val.get<std::string>(), nullptr, 0));
            } catch (...) {}
        }
        return 0;
    }

    // ─── ExecuteCommand ───────────────────────────────────────────────────

    std::string ExecuteCommand(const nlohmann::json& msg)
    {
        const std::string id     = msg.value("id", "");
        const std::string action = msg.value("action", "");

        nlohmann::json result;
        result["type"] = "commandResult";
        result["id"]   = id;

        if (!msg.contains("formId")) {
            result["success"] = false;
            result["error"]   = "Missing required field: 'formId'";
            return result.dump();
        }

        const std::uint32_t formId = ParseFormId(msg["formId"]);
        if (formId == 0) {
            result["success"] = false;
            result["error"]   = "Invalid 'formId' value";
            return result.dump();
        }

        nlohmann::json opResult;

        if (action == "equip") {
            const std::string hand = msg.value("hand", "right");
            opResult = InventoryWriter::Equip(formId, hand);

        } else if (action == "unequip") {
            const std::string hand = msg.value("hand", "");
            opResult = InventoryWriter::Unequip(formId, hand);

        } else if (action == "use") {
            opResult = InventoryWriter::Use(formId);

        } else if (action == "drop") {
            const int count = msg.value("count", 1);
            opResult = InventoryWriter::Drop(formId, count);

        } else if (action == "favorite") {
            const bool add = msg.value("favorite", true);
            opResult = InventoryWriter::Favorite(formId, add);

        } else {
            opResult = {
                { "success", false },
                { "error",   "Unknown action: '" + action + "'" }
            };
        }

        result["success"] = opResult.value("success", false);
        if (opResult.contains("error"))
            result["error"] = opResult["error"];

        return result.dump();
    }
}

