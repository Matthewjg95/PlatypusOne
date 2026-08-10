// PlatypusOS services — project storage.
//
// Every app persists user work as a "project": a directory under
// <dataRoot>/projects/<name> owned by that app. ProjectStore centralizes
// naming, enumeration, and quota checks so apps never invent their own
// on-disk layouts.
#pragma once

#include <platypus/hal/IStorage.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace platypus::filesystem {

struct ProjectInfo {
    std::string name;
    std::string ownerApp;
    std::filesystem::path path;
};

class ProjectStore {
public:
    explicit ProjectStore(std::shared_ptr<hal::IStorage> storage);

    [[nodiscard]] std::vector<ProjectInfo> list() const;
    hal::Result<ProjectInfo> create(const std::string& name, const std::string& ownerApp);
    hal::Status remove(const std::string& name);

private:
    std::shared_ptr<hal::IStorage> storage_;
};

}  // namespace platypus::filesystem
