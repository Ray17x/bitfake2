#ifndef FILECHECKS_HPP
#define FILECHECKS_HPP

#include <cstdio>
#include <filesystem>
#include "consoleout.hpp"
namespace fs = std::filesystem;
#include "operations.hpp"
namespace op = Operations;

namespace FileChecks {
// Checks if the parent directory of the given path exists. If the file itself exists,
// it returns true without checking the parent directory.
inline bool ParentExists(const fs::path &path) {
    if (fs::exists(path)) {
        ConsoleOut::plog("Path exists, no need to check parent dir.");
        return true;
    }

    // Since the file doesn't exist, check if the parent dir exists
    fs::path parent = path.parent_path();
    if (fs::exists(parent) && fs::is_directory(parent)) {
        ConsoleOut::plog("Parent directory exists.");
        return true;
    }

    ConsoleOut::warn("Parent directory does not exist.");
    return false;
}

bool IsValidAudioFile(const fs::path &path); // Checks via extensions
bool IsTrueAudio(
    const fs::path &path); // Checks via file signatures for senstive operations (As described in filechecks.cpp)
bool IsSpecificAudioFormat(const fs::path &path, op::AudioFormat format);
inline bool IsSpecficAudioFormat(const fs::path &path, op::AudioFormat format) {
    return IsSpecificAudioFormat(path, format);
}

} // namespace FileChecks

#endif // FILECHECKS_HPP