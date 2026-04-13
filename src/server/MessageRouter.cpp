#include "MessageRouter.h"
#include "SubscriptionState.h"
#include "WsSession.h"
#include "../game/FieldRegistry.h"
#include "../game/GameReader.h"
#include "../game/GameWriter.h"
#include "../Utils.h"

#include <chrono>
#include <nlohmann/json.hpp>

namespace asio = boost::asio;

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
    static RE::FormID ParseFormId(const std::string& str)
    {
        try {
            return static_cast<RE::FormID>(std::stoul(str, nullptr, 16));
        } catch (...) {
            return 0;
        }
    }

    // Build the JSON response for a command result.
    static std::string BuildCommandResultJson(const std::string& id, const GameWriter::CommandResult& result)
    {
        nlohmann::json resp;
        resp["type"]    = "commandResult";
        resp["id"]      = id;
        resp["success"] = result.success;
        if (!result.success)
            resp["error"] = result.error;
        if (!result.debugLog.empty())
            resp["debugLog"] = result.debugLog;
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
        if (!msg.contains("formId") || !msg["formId"].is_string()) {
            session->send(R"({"type":"error","message":"Missing 'formId' field"})");
            return;
        }

        const std::string cmdId      = msg["id"].get<std::string>();
        const std::string command    = msg["command"].get<std::string>();
        const std::string formIdStr  = msg["formId"].get<std::string>();
        const std::string hand       = msg.value("hand", "right");
        const int         count      = msg.value("count", 1);

        const RE::FormID formId = ParseFormId(formIdStr);
        if (formId == 0) {
            nlohmann::json err;
            err["type"]    = "commandResult";
            err["id"]      = cmdId;
            err["success"] = false;
            err["error"]   = "Invalid formId: '" + formIdStr + "'";
            session->send(err.dump());
            return;
        }

        SKSE::GetTaskInterface()->AddTask([session, cmdId, command, formId, hand, count]() {
            GameWriter::CommandResult result;

            if (command == "equip")
                result = GameWriter::EquipItem(formId, hand);
            else if (command == "unequip")
                result = GameWriter::UnequipItem(formId, hand);
            else if (command == "use")
                result = GameWriter::UseItem(formId);
            else if (command == "drop")
                result = GameWriter::DropItem(formId, count);
            else if (command == "favorite")
                result = GameWriter::FavoriteItem(formId);
            else
                result = {false, "Unknown command: '" + command + "'"};

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
