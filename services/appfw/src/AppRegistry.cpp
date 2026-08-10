#include "platypus/appfw/AppRegistry.hpp"

#include <algorithm>

namespace platypus::appfw {

bool AppRegistry::add(AppManifest manifest, AppFactory factory) {
    if (!factory) return false;
    const bool duplicate = std::any_of(entries_.begin(), entries_.end(),
        [&](const Entry& e) { return e.manifest.id == manifest.id; });
    if (duplicate) return false;

    manifests_.push_back(manifest);
    entries_.push_back({std::move(manifest), factory});
    return true;
}

const std::vector<AppManifest>& AppRegistry::manifests() const noexcept {
    return manifests_;
}

std::unique_ptr<IApp> AppRegistry::create(std::string_view id) const {
    const auto it = std::find_if(entries_.begin(), entries_.end(),
        [&](const Entry& e) { return e.manifest.id == id; });
    return it != entries_.end() ? it->factory() : nullptr;
}

}  // namespace platypus::appfw
