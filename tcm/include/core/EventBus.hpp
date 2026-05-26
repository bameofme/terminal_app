#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace tcm {

// ---------------------------------------------------------------------------
// Event types
// ---------------------------------------------------------------------------
struct SessionAddedEvent        { std::string sessionId; };
struct SessionRemovedEvent      { std::string sessionId; };
struct SessionSwitchedEvent     { std::string fromId; std::string toId; };
struct DataReceivedEvent        { std::string sessionId; std::vector<uint8_t> data; bool isInput = false; };
struct KeyPressedEvent          { int key; bool isEscape = false; };
struct MacroTriggeredEvent      { std::string recipeName; std::string sessionId; };
struct MacroDoneEvent           { std::string sessionId; bool success = true; };
struct MacroFailedEvent         { std::string sessionId; std::string reason; };
struct SessionConnectedEvent    { std::string sessionId; };
struct SessionDisconnectedEvent { std::string sessionId; };

// ---------------------------------------------------------------------------
// EventBus — thread-safe publish/subscribe (header-only, template)
// ---------------------------------------------------------------------------
class EventBus {
public:
    using Token = int;

    template<typename EventType>
    Token subscribe(std::function<void(const EventType&)> handler) {
        Token tok = m_nextToken.fetch_add(1, std::memory_order_relaxed);
        auto wrapped = [h = std::move(handler)](const void* p) {
            h(*static_cast<const EventType*>(p));
        };
        std::unique_lock lock(m_mutex);
        m_handlers[std::type_index(typeid(EventType))].push_back(
            {tok, std::move(wrapped)});
        m_tokenToType.emplace(tok, std::type_index(typeid(EventType)));
        return tok;
    }

    template<typename EventType>
    void publish(const EventType& event) {
        std::shared_lock lock(m_mutex);
        auto it = m_handlers.find(std::type_index(typeid(EventType)));
        if (it == m_handlers.end()) return;
        for (const auto& entry : it->second) {
            try {
                entry.handler(&event);
            } catch (...) {
                // swallow exceptions from handlers
            }
        }
    }

    void unsubscribe(Token token) {
        std::unique_lock lock(m_mutex);
        auto typeIt = m_tokenToType.find(token);
        if (typeIt == m_tokenToType.end()) return;
        std::type_index typeIdx = typeIt->second;
        m_tokenToType.erase(typeIt);
        auto& vec = m_handlers[typeIdx];
        vec.erase(
            std::remove_if(vec.begin(), vec.end(),
                [token](const HandlerEntry& e) { return e.token == token; }),
            vec.end());
    }

private:
    struct HandlerEntry {
        Token token;
        std::function<void(const void*)> handler;
    };

    std::unordered_map<std::type_index, std::vector<HandlerEntry>> m_handlers;
    mutable std::shared_mutex m_mutex;
    std::atomic<Token> m_nextToken{1};
    std::unordered_map<Token, std::type_index> m_tokenToType;
};

} // namespace tcm
