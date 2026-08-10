#include "platypus/filesystem/ProjectStore.hpp"

#include <fstream>

namespace platypus::filesystem {

namespace fs = std::filesystem;

ProjectStore::ProjectStore(std::shared_ptr<hal::IStorage> storage)
    : storage_(std::move(storage)) {}

std::vector<ProjectInfo> ProjectStore::list() const {
    std::vector<ProjectInfo> out;
    const auto root = storage_->dataRoot() / "projects";
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (!entry.is_directory()) continue;
        ProjectInfo info;
        info.name = entry.path().filename().string();
        info.path = entry.path();
        // Owner app recorded at creation time in a marker file.
        std::ifstream owner(entry.path() / ".owner");
        std::getline(owner, info.ownerApp);
        out.push_back(std::move(info));
    }
    return out;
}

hal::Result<ProjectInfo> ProjectStore::create(const std::string& name,
                                              const std::string& ownerApp) {
    if (name.empty() || name.find_first_of("/\\") != std::string::npos)
        return hal::Error::InvalidArgument;

    const auto path = storage_->dataRoot() / "projects" / name;
    std::error_code ec;
    if (fs::exists(path, ec)) return hal::Error::Busy;
    if (!fs::create_directories(path, ec)) return hal::Error::IoFailure;

    std::ofstream owner(path / ".owner");
    owner << ownerApp << '\n';

    return ProjectInfo{name, ownerApp, path};
}

hal::Status ProjectStore::remove(const std::string& name) {
    const auto path = storage_->dataRoot() / "projects" / name;
    std::error_code ec;
    if (!fs::exists(path, ec)) return hal::Error::InvalidArgument;
    fs::remove_all(path, ec);
    return ec ? hal::Status(hal::Error::IoFailure) : hal::Status();
}

}  // namespace platypus::filesystem
