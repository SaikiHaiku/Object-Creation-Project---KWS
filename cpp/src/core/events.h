#pragma once
#include <functional>
#include <vector>
#include <unordered_map>
#include <any>
#include <typeindex>
#include <mutex>

namespace ocp {

class EventBus {
public:
    using HandlerId = uint64_t;

    template<typename EventT>
    HandlerId on(std::function<void(const EventT&)> handler) {
        auto key = std::type_index(typeid(EventT));
        auto id = next_id_++;
        handlers_[key].emplace_back(id, std::move(handler), sizeof(EventT));
        return id;
    }

    template<typename EventT>
    void emit(const EventT& event) {
        auto key = std::type_index(typeid(EventT));
        auto it = handlers_.find(key);
        if (it == handlers_.end()) return;
        for (auto& entry : it->second) {
            if (entry.id == 0) continue;
            auto* fn = std::any_cast<HandlerFn<EventT>>(&entry.fn);
            if (fn) (*fn)(event);
        }
    }

    void remove(HandlerId id) {
        for (auto& [key, list] : handlers_) {
            for (auto& entry : list) {
                if (entry.id == id) { entry.id = 0; return; }
            }
        }
    }

    void clear() { handlers_.clear(); }

private:
    template<typename EventT>
    using HandlerFn = std::function<void(const EventT&)>;

    struct HandlerEntry {
        HandlerId id = 0;
        std::any fn;
        size_t event_size = 0;
    };

    uint64_t next_id_ = 1;
    std::unordered_map<std::type_index, std::vector<HandlerEntry>> handlers_;
};

// === Common Events ===

struct NodeSelectedEvent { class SceneNode* node; };
struct NodeDeletedEvent { class SceneNode* node; };
struct SceneModifiedEvent {};
struct UndoTriggeredEvent { std::string description; };
struct ModeChangedEvent { int new_mode; };
struct ToolChangedEvent { int new_tool; };

} // namespace ocp
