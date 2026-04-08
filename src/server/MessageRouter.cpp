#include "MessageRouter.h"
#include "SubscriptionState.h"
#include "WsSession.h"
#include "../game/FieldRegistry.h"
#include "../game/GameReader.h"
#include "../Utils.h"

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
            if (!FieldRegistry::Resolve(registryKey) && !GameReader::IsKnownInventoryKey(registryKey)) {
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
            SubscriptionState state;

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
            session->CancelSubscription();

        } else if (type == "describe") {
            nlohmann::json desc = nlohmann::json::parse(FieldRegistry::BuildDescribeJson());
            auto& fields = desc["fields"];
            for (const auto& key : GameReader::GetInventoryKeys()) {
                fields[key] = { { "valueType", "json" }, { "valueCategory", "inventory" }, { "description", "Inventory data" } };
            }
            session->send(desc.dump());

        } else if (type == "query") {
            if (!msg.contains("fields") || !msg["fields"].is_object()) {
                session->send(R"({"type":"error","message":"Missing or invalid 'fields' in query"})");
                return;
            }

            SubscriptionState oneShot;
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

        } else {
            session->send(R"({"type":"error","message":"Unknown message type"})");
        }
    }
}
