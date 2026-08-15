#include "MessageRouter.h"
#include "SubscriptionState.h"
#include "WsSession.h"
#include "../game/FieldRegistry.h"
#include "../game/GameReader.h"
#include "../game/GameCommands.h"
#include "../Utils.h"

#include <chrono>
#include <cctype>
#include <nlohmann/json.hpp>
#include <optional>

namespace asio   = boost::asio;
namespace logger = SKSE::log;

namespace MessageRouter
{
    static bool ParseFields(const nlohmann::json&              fieldsJson,
                            std::shared_ptr<WsSession>         session,
                            std::unordered_map<std::string, std::string>& out)
    {
        for (auto& [alias, keyVal] : fieldsJson.items()) {
            if (!keyVal.is_string()) {
                nlohmann::json err;
                err["type"]    = "error";
                err["message"] = "Field value for '" + alias + "' must be a string";
                session->send(err.dump());
                return false;
            }
            std::string registryKey = keyVal.get<std::string>();
            if (!FieldRegistry::Resolve(registryKey) && !FieldRegistry::ResolveJson(registryKey)) {
                nlohmann::json err;
                err["type"]    = "error";
                err["message"] = "Unknown field key: '" + registryKey + "'";
                session->send(err.dump());
                return false;
            }
            out[alias] = registryKey;
        }
        return true;
    }

    // Parse a hex formId string (e.g. "0x00012EB7" or "12EB7") to a FormID.
    // Returns std::nullopt on parse failure (unlike a 0 sentinel, this lets
    // callers distinguish the legitimate "0" input from a malformed one).
    static std::optional<RE::FormID> ParseFormId(const std::string& str)
    {
        try {
            const auto start = str.find_first_not_of(" \t\n\r\f\v");
            if (start == std::string::npos)
                return std::nullopt;

            const auto end = str.find_last_not_of(" \t\n\r\f\v");
            const auto trimmed = str.substr(start, end - start + 1);

            std::size_t pos = 0;
            auto        val = std::stoul(trimmed, &pos, 16);
            if (pos == 0 || pos != trimmed.size())
                return std::nullopt;
            return static_cast<RE::FormID>(val);
        } catch (...) {
            return std::nullopt;
        }
    }

    // Build the JSON response for a command result.
    static std::string BuildCommandResultJson(const std::string& id, const GameCommands::CommandResult& result)
    {
        nlohmann::json resp;
        resp["type"]    = "commandResult";
        resp["id"]      = id;
        resp["success"] = result.success;
        if (!result.success)
            resp["error"] = result.error;
        if (!result.data.is_null())
            resp["data"] = result.data;
        return resp.dump();
    }

    // Dispatch a "command" message to the game thread.
    static void DispatchCommand(std::shared_ptr<WsSession> session, const nlohmann::json& msg)
    {
        if (!msg.contains("id") || !msg["id"].is_string()) {
            session->send(R"({"type":"error","message":"Missing or invalid 'id' in command"})");
            return;
        }
        if (!msg.contains("command") || !msg["command"].is_string()) {
            session->send(R"({"type":"error","message":"Missing 'command' field"})");
            return;
        }

        const std::string cmdId   = msg["id"].get<std::string>();
        const std::string command = msg["command"].get<std::string>();

        // Player-placed map marker commands have their own argument shape
        // (coordinates, no formId). Dispatch them before the generic
        // formId-required path.
        if (command == "player_marker_set" || command == "player_marker_clear") {
            if (command == "player_marker_set") {
                const auto getNum = [&](const char* key, float& out) -> bool {
                    if (!msg.contains(key) || !msg[key].is_number())
                        return false;
                    out = msg[key].get<float>();
                    return true;
                };
                float x = 0, y = 0, z = 0;
                if (!getNum("x", x) || !getNum("y", y)) {
                    nlohmann::json err;
                    err["type"]    = "commandResult";
                    err["id"]      = cmdId;
                    err["success"] = false;
                    err["error"]   = "player_marker_set requires numeric 'x' and 'y' (and optional numeric 'z')";
                    session->send(err.dump());
                    return;
                }
                // 'z' is optional — defaults to 0 when omitted.
                if (msg.contains("z")) {
                    if (!msg["z"].is_number()) {
                        nlohmann::json err;
                        err["type"]    = "commandResult";
                        err["id"]      = cmdId;
                        err["success"] = false;
                        err["error"]   = "player_marker_set 'z' must be numeric when present";
                        session->send(err.dump());
                        return;
                    }
                    z = msg["z"].get<float>();
                }

                SKSE::GetTaskInterface()->AddTask([session, cmdId, x, y, z]() {
                    auto        result = GameCommands::SetPlayerMarker(x, y, z);
                    std::string json   = BuildCommandResultJson(cmdId, result);
                    asio::post(session->ioc(), [session, json] { session->send(json); });
                });
            } else {  // player_marker_clear
                SKSE::GetTaskInterface()->AddTask([session, cmdId]() {
                    auto        result = GameCommands::ClearPlayerMarker();
                    std::string json   = BuildCommandResultJson(cmdId, result);
                    asio::post(session->ioc(), [session, json] { session->send(json); });
                });
            }
            return;
        }

        // Hotkey commands have different argument shapes; dispatch them
        // separately so we don't force a formId on clear/trigger.
        if (command == "hotkey_set" || command == "hotkey_clear" ||
            command == "hotkey_trigger")
        {
            const bool needsFormId = (command == "hotkey_set");

            if (!msg.contains("slot") || !msg["slot"].is_number_integer()) {
                nlohmann::json err;
                err["type"]    = "commandResult";
                err["id"]      = cmdId;
                err["success"] = false;
                err["error"]   = "Missing or invalid 'slot' (expected integer 1..8)";
                session->send(err.dump());
                return;
            }
            const int slot = msg["slot"].get<int>();

            RE::FormID formId = 0;
            if (needsFormId) {
                if (!msg.contains("formId") || !msg["formId"].is_string()) {
                    nlohmann::json err;
                    err["type"]    = "commandResult";
                    err["id"]      = cmdId;
                    err["success"] = false;
                    err["error"]   = "Missing 'formId' field";
                    session->send(err.dump());
                    return;
                }
                const std::string formIdStr = msg["formId"].get<std::string>();
                const auto        parsed    = ParseFormId(formIdStr);
                if (!parsed) {
                    nlohmann::json err;
                    err["type"]    = "commandResult";
                    err["id"]      = cmdId;
                    err["success"] = false;
                    err["error"]   = "Invalid formId: '" + formIdStr + "'";
                    session->send(err.dump());
                    return;
                }
                formId = *parsed;
            }

            SKSE::GetTaskInterface()->AddTask([session, cmdId, command, slot, formId]() {
                GameCommands::CommandResult result;
                const auto slotU8 = static_cast<std::uint8_t>(slot);
                if (command == "hotkey_set")
                    result = GameCommands::SetHotkey(slotU8, formId);
                else if (command == "hotkey_clear")
                    result = GameCommands::ClearHotkey(slotU8);
                else
                    result = GameCommands::TriggerHotkey(slotU8);

                std::string json = BuildCommandResultJson(cmdId, result);
                asio::post(session->ioc(), [session, json] { session->send(json); });
            });
            return;
        }

        // Quest commands use boolean arguments,
        // so parse them before the generic formId-required item/spell/map-marker command path.
        if (command == "quest_set_active") {
            if (!msg.contains("formId") || !msg["formId"].is_string()) {
                nlohmann::json err;
                err["type"]    = "commandResult";
                err["id"]      = cmdId;
                err["success"] = false;
                err["error"]   = "quest_set_active requires string 'formId'";
                session->send(err.dump());
                return;
            }
            if (!msg.contains("active") || !msg["active"].is_boolean()) {
                nlohmann::json err;
                err["type"]    = "commandResult";
                err["id"]      = cmdId;
                err["success"] = false;
                err["error"]   = "quest_set_active requires boolean 'active'";
                session->send(err.dump());
                return;
            }

            const std::string formIdStr = msg["formId"].get<std::string>();
            const auto parsed = ParseFormId(formIdStr);
            if (!parsed) {
                nlohmann::json err;
                err["type"]    = "commandResult";
                err["id"]      = cmdId;
                err["success"] = false;
                err["error"]   = "Invalid formId: '" + formIdStr + "'";
                session->send(err.dump());
                return;
            }

            const RE::FormID formId = *parsed;
            const bool active = msg["active"].get<bool>();
            SKSE::GetTaskInterface()->AddTask([session, cmdId, formId, active]() {
                auto result = GameCommands::SetQuestActive(formId, active);
                std::string json = BuildCommandResultJson(cmdId, result);
                asio::post(session->ioc(), [session, json] { session->send(json); });
            });
            return;
        }

        if (command == "texture_preview") {
            if (!msg.contains("path") || !msg["path"].is_string()) {
                nlohmann::json err;
                err["type"]    = "commandResult";
                err["id"]      = cmdId;
                err["success"] = false;
                err["error"]   = "texture_preview requires string 'path'";
                session->send(err.dump());
                return;
            }
            const std::string path = msg["path"].get<std::string>();
            // Decode + PNG encode + base64 are heavy: run them on the
            // io_context thread, not the game thread, to avoid freezing the game.
            asio::post(session->ioc(), [session, cmdId, path]() {
                auto result = GameCommands::GetTexturePreview(path);
                session->send(BuildCommandResultJson(cmdId, result));
            });
            return;
        }

        if (command == "file_download") {
            if (!msg.contains("path") || !msg["path"].is_string()) {
                nlohmann::json err;
                err["type"]    = "commandResult";
                err["id"]      = cmdId;
                err["success"] = false;
                err["error"]   = "file_download requires string 'path'";
                session->send(err.dump());
                return;
            }
            const std::string path = msg["path"].get<std::string>();
            asio::post(session->ioc(), [session, cmdId, path]() {
                auto result = GameCommands::GetFileDownload(path);
                session->send(BuildCommandResultJson(cmdId, result));
            });
            return;
        }

        if (command == "item_preview") {
            if (!msg.contains("formId") || !msg["formId"].is_string()) {
                nlohmann::json err;
                err["type"]    = "commandResult";
                err["id"]      = cmdId;
                err["success"] = false;
                err["error"]   = "item_preview requires string 'formId'";
                session->send(err.dump());
                return;
            }
            const std::string formIdStr = msg["formId"].get<std::string>();
            const auto        parsed    = ParseFormId(formIdStr);
            if (!parsed) {
                nlohmann::json err;
                err["type"]    = "commandResult";
                err["id"]      = cmdId;
                err["success"] = false;
                err["error"]   = "Invalid formId: '" + formIdStr + "'";
                session->send(err.dump());
                return;
            }
            const RE::FormID formId = *parsed;
            // 1) Resolve the DDS path on the game thread (fast, no file I/O).
            SKSE::GetTaskInterface()->AddTask([session, cmdId, formId]() {
                auto pathResult = GameCommands::ResolvePreviewPath(formId);
                // 2) Convert + send on the io_context thread (heavy).
                asio::post(session->ioc(), [session, cmdId, pathResult = std::move(pathResult)]() {
                    if (!pathResult.success)
                        session->send(BuildCommandResultJson(cmdId, { false, pathResult.error }));
                    else
                        session->send(BuildCommandResultJson(cmdId, GameCommands::GetTexturePreview(pathResult.path)));
                });
            });
            return;
        }

        if (!msg.contains("formId") || !msg["formId"].is_string()) {
            session->send(BuildCommandResultJson(cmdId, {false, "Missing 'formId' field"}));
            return;
        }

        const std::string formIdStr  = msg["formId"].get<std::string>();
        const std::string hand       = msg.value("hand", "right");
        const int         count      = msg.value("count", 1);

        logger::debug("command '{}' id='{}' formId={} hand={} count={}",
                      command, cmdId, formIdStr, hand, count);

        const auto parsed = ParseFormId(formIdStr);
        if (!parsed) {
            nlohmann::json err;
            err["type"]    = "commandResult";
            err["id"]      = cmdId;
            err["success"] = false;
            err["error"]   = "Invalid formId: '" + formIdStr + "'";
            session->send(err.dump());
            return;
        }
        const RE::FormID formId = *parsed;

        SKSE::GetTaskInterface()->AddTask([session, cmdId, command, formId, hand, count]() {
            GameCommands::CommandResult result;

            if (command == "equip")
                result = GameCommands::EquipItem(formId, hand);
            else if (command == "unequip")
                result = GameCommands::UnequipItem(formId, hand);
            else if (command == "use")
                result = GameCommands::UseItem(formId);
            else if (command == "read_book")
                result = GameCommands::ReadBook(formId);
            else if (command == "drop")
                result = GameCommands::DropItem(formId, count);
            else if (command == "favorite")
                result = GameCommands::FavoriteItem(formId);
            else if (command == "equip_spell")
                result = GameCommands::EquipSpell(formId, hand);
            else if (command == "unequip_spell")
                result = GameCommands::UnequipSpell(formId, hand);
            else if (command == "favorite_spell")
                result = GameCommands::FavoriteSpell(formId);
            else if (command == "equip_shout")
                result = GameCommands::EquipShout(formId);
            else if (command == "unequip_shout")
                result = GameCommands::UnequipShout(formId);
            else if (command == "equip_power")
                result = GameCommands::EquipPower(formId);
            else if (command == "favorite_shout")
                result = GameCommands::FavoriteShout(formId);
            else if (command == "fast_travel")
                result = GameCommands::FastTravelToMarker(formId);
            else
                result = {false, "Unknown command: '" + command + "'"};

            if (result.success)
                logger::debug("command '{}' id='{}' succeeded", command, cmdId);
            else
                logger::warn("command '{}' id='{}' failed: {}", command, cmdId, result.error);

            std::string json = BuildCommandResultJson(cmdId, result);
            asio::post(session->ioc(), [session, json] { session->send(json); });
        });
    }

    void Dispatch(std::shared_ptr<WsSession> session, const std::string& raw)
    {
        nlohmann::json msg;
        try {
            msg = nlohmann::json::parse(raw);
        } catch (...) {
            session->send(R"({"type":"error","message":"Invalid JSON"})");
            return;
        }

        const std::string type = msg.value("type", "");

        // Skip noisy heartbeat messages from debug log.
        if (type != "heartbeat")
            logger::debug("WS message: type='{}'", type);

        if (type == "subscribe") {
            if (!msg.contains("id") || !msg["id"].is_string()) {
                session->send(R"({"type":"error","message":"Missing or invalid 'id' in subscribe"})");
                return;
            }

            SubscriptionState state;
            state.id = msg["id"].get<std::string>();

            if (msg.contains("settings") && msg["settings"].is_object()) {
                auto& s        = msg["settings"];
                state.frequencyMs  = s.value("frequency", 500);
                state.sendOnChange = s.value("sendOnChange", false);
            }

            if (msg.contains("fields") && msg["fields"].is_object()) {
                if (!ParseFields(msg["fields"], session, state.fields))
                    return;
            }

            logger::debug("subscribe id='{}' freq={}ms sendOnChange={} fields={}",
                          state.id, state.frequencyMs, state.sendOnChange, state.fields.size());
            session->SetSubscription(std::move(state));

        } else if (type == "unsubscribe") {
            if (msg.contains("id") && msg["id"].is_string())
                session->CancelSubscription(msg["id"].get<std::string>());
            else
                session->CancelAllSubscriptions();

        } else if (type == "heartbeat") {
            const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
            nlohmann::json resp;
            resp["type"] = "heartbeat";
            resp["ts"]   = nowMs;
            session->send(resp.dump());

        } else if (type == "query") {
            if (!msg.contains("id") || !msg["id"].is_string()) {
                session->send(R"({"type":"error","message":"Missing or invalid 'id' in query"})");
                return;
            }
            if (!msg.contains("fields") || !msg["fields"].is_object()) {
                session->send(R"({"type":"error","message":"Missing or invalid 'fields' in query"})");
                return;
            }

            SubscriptionState oneShot;
            oneShot.id           = msg["id"].get<std::string>();
            oneShot.sendOnChange = false;
            if (!ParseFields(msg["fields"], session, oneShot.fields))
                return;

            SKSE::GetTaskInterface()->AddTask([session, oneShot]() mutable {
                std::string json = GameReader::BuildSubscriptionJson(oneShot);
                asio::post(session->ioc(), [session, json] {
                    if (!json.empty())
                        session->send(json);
                });
            });

        } else if (type == "unsubscribe_all") {
            session->CancelAllSubscriptions();

        } else if (type == "command") {
            DispatchCommand(session, msg);

        } else {
            session->send(R"({"type":"error","message":"Unknown message type"})");
        }
    }
}
