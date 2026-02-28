#ifndef OPERATIONS_HPP
#define OPERATIONS_HPP

#include "consoleout.hpp"
using namespace ConsoleOut;
#include "filechecks.hpp"
namespace fc = FileChecks;
#include <array>
#include <string_view>
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;


namespace Operations
{

    /*
        This namespace will contain the core operations of the program, such as the actual bit reduction process,
        file processing, and any other main functionalities. The idea is to keep the main function clean and delegate
        the heavy lifting to this namespace. This will also help with organization and maintainability as the project grows.
    */
    enum class AudioFormat
    {
        MP3, OGG, M4A, WAV, FLAC, AAC, WMA,
        OPUS, AIFF, AU, RA, GA3, AMR, AWB,
        DSS, DVF, M4B, M4P, MMF, MPC, MSV,
        NMF, OGA, RAW, RF64, SLN, TTA, VOC,
        VOX, WV, WEBM, SVX8, CDA
    };

    enum class VBRQualities
    {
        V0, V1, V2, V3, V4, V5, V6, V7, V8, V9, // MP3
        Q0, Q3, Q6, Q9, Q10, // OGG VORBIS
        L0, L1, L2, L3, L4, L5, L6, L7, L8 // FLAC encoding lvls (not rlly quality based but w/e)
        // more to add l8r
    };

    struct AudioMetadata
    {
        std::string title;
        std::string artist;
        std::string album;
        std::string genre;
        std::string year;
        int trackNumber = 0;
    };

    struct AudioMetadataResult
    {
        fs::path path;
        AudioMetadata metadata;
    };

    struct ReplayGainInfo
    {
        std::string title;
        std::string artist;
        std::string album;
        int trackNumber;
        float trackGain;
        float trackPeak;
        float albumGain;
        float albumPeak;
    };

    struct ReplayGainResult
    {
        fs::path path;
        ReplayGainInfo info;
    };

    struct MusicBrainzInfo
    {
        std::string mbid; // MusicBrainz Identifier
        std::string releaseGroup;
        std::string releaseDate;
    };
    
    struct SpectralAnalysisResult
    {
        std::string title;
        std::string artist;
        std::string album;
        bool likelyLossy;
        float frequencyCutoff;      // Where high-freq content drops sharply
        float noiseFlorElevation;   // Elevated noise floor vs expected
        float bandingScore;         // Presence of quantization banding (0-1)
        std::string diagnosis;      // Human readable summary
    };

    // Function Declarations:

    // Non-user functions
    AudioFormat StringToAudioFormat(const std::string& str);
    VBRQualities StringToVBRQuality(const std::string& str);
    void StageMetaDataChanges(TagLib::PropertyMap& drawer, std::string key, const std::string& value);
    bool CommitMetaDataChanges(const std::filesystem::path& path, const TagLib::PropertyMap& drawer);
    
    // Helper functions
    AudioMetadata GetMetaData(const fs::path& path);
    std::vector<AudioMetadataResult> GetMetaDataList(const fs::path& path);
    ReplayGainInfo GetReplayGain(const fs::path& path);
    std::vector<ReplayGainResult> GetReplayGainList(const fs::path& path);
    SpectralAnalysisResult SpectralAnalysis(const fs::path& path);
    std::vector<SpectralAnalysisResult> SpectralAnalysisList(const fs::path& path);
    
    // Implemented later because it requires network access and more complex logic!!
    MusicBrainzInfo GetMusicBrainzInfo(const fs::path& path);

    // Core operations
    void ConvertToFileType(const fs::path& inputPath, const fs::path& outputPath, AudioFormat format);
    void MassTagDirectory(const fs::path& dirPath, const std::string& tag, const std::string& value);
    void ApplyReplayGain(const fs::path& path, float trackGain, float albumGain);
    void CalculateReplayGain(const fs::path& path); 

}


#endif // OPERATIONS_HPP