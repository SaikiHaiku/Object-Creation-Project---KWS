#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace ocp {

struct PluginInfo {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
};

class Plugin {
public:
    virtual ~Plugin() = default;
    virtual PluginInfo get_info() const = 0;
    virtual bool initialize() { return true; }
    virtual void shutdown() {}
    virtual void on_frame(float dt) { (void)dt; }
};

using PluginFactory = std::function<std::unique_ptr<Plugin>()>;

class PluginManager {
public:
    void register_factory(const std::string& id, PluginFactory factory) {
        factories_[id] = std::move(factory);
    }

    Plugin* load(const std::string& id) {
        auto it = factories_.find(id);
        if (it == factories_.end()) return nullptr;
        auto plugin = it->second();
        if (!plugin->initialize()) return nullptr;
        auto* ptr = plugin.get();
        plugins_.push_back(std::move(plugin));
        return ptr;
    }

    void unload_all() {
        for (auto& p : plugins_) p->shutdown();
        plugins_.clear();
    }

    void on_frame(float dt) {
        for (auto& p : plugins_) p->on_frame(dt);
    }

    size_t count() const { return plugins_.size(); }

private:
    std::unordered_map<std::string, PluginFactory> factories_;
    std::vector<std::unique_ptr<Plugin>> plugins_;
};

} // namespace ocp
