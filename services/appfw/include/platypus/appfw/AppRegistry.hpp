// PlatypusOS app framework — application registry.
//
// Owns the set of installed apps. Today apps register statically from the
// launcher's composition root; the same interface will back dynamic plugin
// loading (dlopen'd .so exporting platypus_create_app) later.
#pragma once

#include "IApp.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace platypus::appfw {

class AppRegistry {
public:
    /// Registers an app factory. Duplicate ids are rejected.
    bool add(AppManifest manifest, AppFactory factory);

    [[nodiscard]] const std::vector<AppManifest>& manifests() const noexcept;

    /// Instantiates a fresh app instance; nullptr if id unknown.
    [[nodiscard]] std::unique_ptr<IApp> create(std::string_view id) const;

private:
    struct Entry {
        AppManifest manifest;
        AppFactory factory;
    };
    std::vector<Entry> entries_;
    std::vector<AppManifest> manifests_;
};

}  // namespace platypus::appfw
