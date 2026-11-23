// Durable writes: data that is reported as saved survives a power cut.
//
// A write() only reaches the page cache; on the UNO Q a power loss a few
// seconds later left a "saved" capture as empty files (bench 2026-11-22,
// scan-0121, and the session.json rewritten beside it). syncPath() pushes a
// file's data - or a directory's entries - to storage before the caller
// reports success.
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace platypus::observation {

/// fsync() a file or a directory (its entries: new names, renames). Returns
/// false if it cannot be opened or synced. A no-op that returns true on
/// platforms without fsync (the Windows host build); durability matters on
/// the device.
bool syncPath(const std::filesystem::path& path);

/// Writes `size` bytes to `file` so that a reader after a power cut sees
/// either the previous content or the complete new content, never a torn or
/// empty file: write `file.tmp`, sync it, rename over `file`, sync the
/// directory. On failure returns false and, when `error` is set, says why.
bool writeFileDurably(const std::filesystem::path& file, const void* data, std::size_t size,
                      std::string* error = nullptr);

}  // namespace platypus::observation
