#include <cstdlib>
#include "Utilites/filechecks.hpp"
#include "Utilites/consoleout.hpp"
using namespace ConsoleOut;
#include <filesystem>
#include <cstring>
namespace fs = std::filesystem;

/*
    filecheks.cpp
    This file contains the implementation of the functions declared in filechecks.hpp.
    These functions are used to perform various checks on files, such as validating audio files,
    checking for the existence of parent directories, and more. The goal is to centralize all
    file-related checks in one place for better organization and maintainability.
*/


namespace FileChecks {

// Defined (Valid) Audio Extensions list:
constexpr const char* validAudioExtensions[] = {
    "mp3", "ogg", "m4a", "wav", "flac", "aac", "wma", 
    "opus", "aiff", "au", "ra", "3ga", "amr", "awb", 
    "dss", "dvf", "m4b", "m4p", "mmf", "mpc", "msv", 
    "nmf", "oga", "raw", "rf64", "sln", "tta", "voc", 
    "vox", "wv", "webm", "8svx", "cda"
};

bool IsValidAudioFile(const fs::path& path)
{
    if (!fs::exists(path) || !fs::is_regular_file(path)) {
        warn("File does not exist or is not a regular file.");
        return false;
    }
    const std::string extension = path.extension().string();
    for (const char* validExt : validAudioExtensions) {
        if (extension.substr(1) == validExt) {
            return true;
        }
    }
    return false;
}

// bool IsTrueAudio(const fs::path& path);
bool IsTrueAudio(const fs::path& path)
{

    if (!fs::exists(path) || !fs::is_regular_file(path)) {
        warn("File does not exist or is not a regular file.");
        return false;
    }

    FILE* file = fopen(path.string().c_str(), "rb");
    if (!file) {
        err("Failed to open file for reading.");
        return false;
    }

    unsigned char buffer[12];
    size_t bytesRead = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);

    if (bytesRead < 12) {
        warn("File is too small to be a valid audio file.");
        return false;
    }

    // Check for common audio signatures
    if (memcmp(buffer, "RIFF", 4) == 0 && memcmp(buffer + 8, "WAVE", 4) == 0) {
        return true; // WAV
    }
    if (memcmp(buffer, "OggS", 4) == 0) {
        return true; // OGG (also used for OPUS)
    }
    if (memcmp(buffer, "ID3", 3) == 0 || (buffer[0] == 0xFF && (buffer[1] & 0xE0) == 0xE0)) {
        return true; // MP3
    }
    if (memcmp(buffer, "fLaC", 4) == 0) {
        return true; // FLAC
    }
    
    if (IsValidAudioFile(path)) {
        warn("File has a valid audio extension but failed signature check. It may be corrupted or mislabeled.");
    } else {
        warn("File does not have a valid audio extension and failed signature check.");
    }

    return false;
}

bool IsSpecficAudioFormat(const fs::path& path, op::AudioFormat format)
{
    if (!IsTrueAudio(path)) {
        return false; // Not a valid audio file
    }

    FILE* file = fopen(path.string().c_str(), "rb");
    if (!file) {
        err("Failed to open file for reading.");
        return false;
    }

    unsigned char buffer[12];
    size_t bytesRead = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);

    if (bytesRead < 12) {
        warn("File is too small to be a valid audio file.");
        return false;
    }

    switch (format) {
        case op::AudioFormat::MP3:
            return (memcmp(buffer, "ID3", 3) == 0 || (buffer[0] == 0xFF && (buffer[1] & 0xE0) == 0xE0));
        case op::AudioFormat::WAV:
            return (memcmp(buffer, "RIFF", 4) == 0 && memcmp(buffer + 8, "WAVE", 4) == 0);
        case op::AudioFormat::FLAC:
            return (memcmp(buffer, "fLaC", 4) == 0);
        case op::AudioFormat::OGG:
            return (memcmp(buffer, "OggS", 4) == 0);
        // Add more cases for other formats as needed
        default:
            warn("Unsupported audio format specified for checking.");
            return false;
    }

} // namespace FileChecks
