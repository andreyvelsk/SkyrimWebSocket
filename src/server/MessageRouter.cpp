#include "MessageRouter.h"
#include "SubscriptionState.h"
#include "WsSession.h"
#include "../game/FieldRegistry.h"
#include "../game/GameReader.h"
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

        } else {
            session->send(R"({"type":"error","message":"Unknown message type"})");
        }
    }
}
