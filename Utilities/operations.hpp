#ifndef OPERATIONS_HPP
#define OPERATIONS_HPP

#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
namespace fs = std::filesystem;

namespace Operations {

/*
    This namespace will contain the core operations of the program, such as the actual bit reduction process,
    file processing, and any other main functionalities. The idea is to keep the main function clean and delegate
    the heavy lifting to this namespace. This will also help with organization and maintainability as the project grows.
*/
enum class AudioFormat {
    MP3,
    OGG,
    M4A,
    WAV,
    FLAC,
    AAC,
    WMA,
    OPUS,
    AIFF,
    AU,
    RA,
    GA3,
    AMR,
    AWB,
    DSS,
    DVF,
    M4B,
    M4P,
    MMF,
    MPC,
    MSV,
    NMF,
    OGA,
    RAW,
    RF64,
    SLN,
    TTA,
    VOC,
    VOX,
    WV,
    WEBM,
    SVX8,
    CDA,
    GENERAL
    // General used for things we don't need a specfic format for
};

enum class VBRQualities : int {
    V0 = 0,
    V1 = 1,
    V2 = 2,
    V3 = 3,
    V4 = 4,
    V5 = 5,
    V6 = 6,
    V7 = 7,
    V8 = 8,
    V9 = 9, // MP3
    Q0 = 0,
    Q3 = 3,
    Q6 = 6,
    Q9 = 9,
    Q10 = 10, // OGG VORBIS
    L0 = 0,
    L1 = 1,
    L2 = 2,
    L3 = 3,
    L4 = 4,
    L5 = 5,
    L6 = 6,
    L7 = 7,
    L8 = 8 // FLAC
};

extern const std::map<AudioFormat, std::string> ConversionLibMap;

struct AudioMetadata {
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::string date;
    int trackNumber = 0;
};

struct AudioMetadataResult {
    fs::path path;
    AudioMetadata metadata;
};

struct ReplayGainInfo {
    std::string title;
    std::string artist;
    std::string album;
    int trackNumber;
    float trackGain;
    float trackPeak;
    float albumGain;
    float albumPeak;
};

struct ReplayGainResult {
    fs::path path;
    ReplayGainInfo info;
};

struct MusicBrainzInfo {
    std::string mbid; // MusicBrainz Identifier
    std::string releaseGroup;
    std::string releaseDate;
};

struct SpectralAnalysisResult {
    std::string title;
    std::string artist;
    std::string album;
    bool likelyLossy;
    float frequencyCutoff;    // Where high-freq content drops sharply
    float noiseFlorElevation; // Elevated noise floor vs expected
    float bandingScore;       // Presence of quantization banding (0-1)
    std::string diagnosis;    // Human readable summary
};

struct ReplayGainByTrack {
    float trackGain;
    float trackPeak;
};

struct ReplayGainByAlbum {
    float albumGain;
    float albumPeak;
};

struct AttachedCoverArt {
    std::vector<unsigned char> imageData;
    std::string mimeType;
    std::string description;
    int pictureType = 3;
};

// Function Declarations:

// Non-user functions
AudioFormat StringToAudioFormat(const std::string &str);
VBRQualities StringToVBRQuality(const std::string &str);
void StageMetaDataChanges(TagLib::PropertyMap &drawer, std::string key, const std::string &value);
bool CommitMetaDataChanges(const std::filesystem::path &path, const TagLib::PropertyMap &drawer);

// Helper functions
AudioMetadata GetMetaData(const fs::path &path);
std::vector<AudioMetadataResult> GetMetaDataList(const fs::path &path);
ReplayGainInfo GetReplayGain(const fs::path &path);
std::vector<ReplayGainResult> GetReplayGainList(const fs::path &path);
SpectralAnalysisResult SpectralAnalysis(const fs::path &path);
std::vector<SpectralAnalysisResult> SpectralAnalysisList(const fs::path &path);
bool InputHasAttachedCover(const fs::path &inputPath);
bool GetAttachedCover(const fs::path &inputPath, AttachedCoverArt &coverArt);
bool WriteAttachedCover(const fs::path &outputPath, AudioFormat outputFormat, const AttachedCoverArt &coverArt);
bool CopyAttachedCover(const fs::path &inputPath, const fs::path &outputPath, AudioFormat outputFormat);
bool FormatSupportsAttachedCover(AudioFormat format);
std::string OutputExtensionForFormat(AudioFormat format);

// Implemented later because it requires network access and more complex logic!!
MusicBrainzInfo GetMusicBrainzInfo(const fs::path &path);

// Core operations
void ConvertToFileType(const fs::path &inputPath, const fs::path &outputPath, AudioFormat format, VBRQualities quality);
void MassTagDirectory(const fs::path &dirPath, const std::string &tag, const std::string &value);
void ApplyReplayGain(const fs::path &path, ReplayGainByTrack trackGainInfo, ReplayGainByAlbum albumGainInfo);
ReplayGainByTrack CalculateReplayGainTrack(const fs::path &path);
void CalculateReplayGainAlbum(const fs::path &path);
} // namespace Operations

#endif // OPERATIONS_HPP