#include <cstdlib>
#include <unordered_map>
#include "Utilities/operations.hpp"
namespace op = Operations;
#include "Utilities/globals.hpp"
namespace gb = globals;
#include "Utilities/consoleout.hpp"
#include <algorithm>
using namespace ConsoleOut;
#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>
namespace tl = TagLib;

namespace Operations {
const std::map<AudioFormat, std::string> ConversionLibMap = {
    {AudioFormat::MP3, "libmp3lame"},    {AudioFormat::OGG, "libvorbis"}, {AudioFormat::FLAC, "flac"},
    {AudioFormat::AAC, "libfdk-aac"},    {AudioFormat::OPUS, "libopus"},  {AudioFormat::WAV, "pcm_s16le"},
    {AudioFormat::GENERAL, "libavcodec"}};

// Non-user functions will be declared here
// aka functions that can't be accessed by the users
// during run time.

AudioFormat StringToAudioFormat(const std::string &str) {
    std::string key = str;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Function is now prepared to handle case-insensitive input and implementing
    // a dictionary for mapping string to enum values.

    static const std::unordered_map<std::string, AudioFormat> formatMap = {
        {"mp3", AudioFormat::MP3}, // fuck (static int 0)
        {"ogg", AudioFormat::OGG},   {"m4a", AudioFormat::M4A},   {"wav", AudioFormat::WAV},
        {"flac", AudioFormat::FLAC}, {"aac", AudioFormat::AAC},   {"wma", AudioFormat::WMA},
        {"opus", AudioFormat::OPUS}, {"aiff", AudioFormat::AIFF}, {"au", AudioFormat::AU},
        {"ra", AudioFormat::RA},     {"ga3", AudioFormat::GA3},   {"amr", AudioFormat::AMR},
        {"awb", AudioFormat::AWB},   {"dss", AudioFormat::DSS},   {"dvf", AudioFormat::DVF},
        {"m4b", AudioFormat::M4B},   {"m4p", AudioFormat::M4P},   {"mmf", AudioFormat::MMF},
        {"mpc", AudioFormat::MPC},   {"msv", AudioFormat::MSV},   {"nmf", AudioFormat::NMF},
        {"oga", AudioFormat::OGA},   {"raw", AudioFormat::RAW},   {"rf64", AudioFormat::RF64},
        {"sln", AudioFormat::SLN},   {"tta", AudioFormat::TTA},   {"voc", AudioFormat::VOC},
        {"vox", AudioFormat::VOX},   {"wv", AudioFormat::WV},     {"webm", AudioFormat::WEBM},
        {"svx8", AudioFormat::SVX8}, {"cda", AudioFormat::CDA}};

    auto it = formatMap.find(key);
    if (it != formatMap.end()) {
        return it->second;
    }
    err("Invalid audio format string");
    exit(EXIT_FAILURE);
}

VBRQualities StringToVBRQuality(const std::string &str) {
    std::string key = str;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    static const std::unordered_map<std::string, VBRQualities> vbrMap = {
        {"v0", VBRQualities::V0}, // MP3 (0 - 8 static int)
        {"v1", VBRQualities::V1}, {"v2", VBRQualities::V2}, {"v3", VBRQualities::V3}, {"v4", VBRQualities::V4},
        {"v5", VBRQualities::V5}, {"v6", VBRQualities::V6}, {"v7", VBRQualities::V7}, {"v8", VBRQualities::V8},
        {"v9", VBRQualities::V9}, {"q0", VBRQualities::Q0}, // OGG VORBIS (9 - 13 static int)
        {"q3", VBRQualities::Q3}, {"q6", VBRQualities::Q6}, {"q9", VBRQualities::Q9}, {"q10", VBRQualities::Q10},
        {"l0", VBRQualities::L0}, // FLAC encoding levels (14 - 22 static int)
        {"l1", VBRQualities::L1}, {"l2", VBRQualities::L2}, {"l3", VBRQualities::L3}, {"l4", VBRQualities::L4},
        {"l5", VBRQualities::L5}, {"l6", VBRQualities::L6}, {"l7", VBRQualities::L7}, {"l8", VBRQualities::L8}};

    auto it = vbrMap.find(key);
    if (it != vbrMap.end()) {
        return it->second;
    }
    err("Invalid VBR quality string");
    warn("Are you converting into something thats not mp3/ogg/flac? If so, VBR quality is not applicable yet :(");
    plog("Here's a list of valid VBR quality strings for supported formats:");
    plog("MP3: V0, V1, V2, V3, V4, V5, V6, V7, V8, V9");
    plog("OGG VORBIS: Q0, Q3, Q6, Q9, Q10");
    plog("FLAC encoding levels: L0, L1, L2, L3, L4, L5, L6, L7, L8");
    printf("hint: L8 is the slowest but highest compression level for FLAC, V0 is the highest quality for MP3\n");
    printf("hint: and for OGG VORBIS, Q10 is the highest quality :D\n");
    exit(EXIT_FAILURE);
}

void StageMetaDataChanges(tl::PropertyMap &drawer, std::string key, const std::string &value) {
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    drawer[key] = tl::String(value);
}

bool CommitMetaDataChanges(const std::filesystem::path &path, const tl::PropertyMap &drawer) {
    tl::FileRef f(path.c_str());
    if (f.isNull() || !f.tag()) {
        err("Failed to open file for metadata commit");
        return false;
    }

    tl::PropertyMap existingProps = f.file()->properties();

    for (auto const &[key, val] : drawer) {
        existingProps[key] = val;
    }

    f.file()->setProperties(existingProps);
    return f.file()->save();
}

} // namespace Operations
