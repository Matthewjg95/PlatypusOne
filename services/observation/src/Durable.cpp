#include "platypus/observation/Durable.hpp"

#include <fstream>
#include <system_error>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

namespace platypus::observation {

bool syncPath(const std::filesystem::path& path) {
#ifdef _WIN32
    (void)path;
    return true;
#else
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    const bool ok = ::fsync(fd) == 0;
    ::close(fd);
    return ok;
#endif
}

bool writeFileDurably(const std::filesystem::path& file, const void* data, std::size_t size,
                      std::string* error) {
    const auto temp = std::filesystem::path(file.string() + ".tmp");
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            if (error) *error = "cannot write " + temp.string();
            return false;
        }
        out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        out.flush();
        if (!out) {
            if (error) *error = "write failed for " + temp.string();
            return false;
        }
    }
    if (!syncPath(temp)) {
        if (error) *error = "cannot sync " + temp.string();
        return false;
    }
    std::error_code ec;
    std::filesystem::rename(temp, file, ec);
    if (ec) {
        if (error) *error = "cannot move " + temp.string() + " into place: " + ec.message();
        return false;
    }
    const auto dir = file.has_parent_path() ? file.parent_path() : std::filesystem::path(".");
    if (!syncPath(dir)) {
        if (error) *error = "cannot sync directory " + dir.string();
        return false;
    }
    return true;
}

}  // namespace platypus::observation
