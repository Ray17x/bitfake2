#include <cstdio>
#include "Utilites/consoleout.hpp"
using namespace ConsoleOut;
#include <filesystem>
namespace fs = std::filesystem;
#include "Utilites/filechecks.hpp"
namespace fc = FileChecks;
#include "Utilites/operations.hpp"
namespace op = Operations;
#include "Utilites/globals.hpp"
namespace gb = globals;
#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>
#include <ebur128.h>
#include <sndfile.h> 
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <future>
#include <thread>
#include <atomic>


namespace Operations
{
    // Core operations

    /*
        This file will define functions for core operations of the program, such as conversion, replaygain calculation,
        mass tagging for a directory, and other nitty gritty functions that will be a hassle to program. Will try to keep things
        as clean and organized as possible, but no promises >:D
    */

    // void ConvertToFileType(const fs::path& inputPath, const fs::path& outputPath, AudioFormat format);
    // void MassTagDirectory(const fs::path& dirPath, const std::string& tag, const std::string& value);
    // void ApplyReplayGain(const fs::path& path, float trackGain, float albumGain);
    // void CalculateReplayGain(const fs::path& path); 

    // We can now begin implementing these fuckass features:

    void ConvertToFileType(const fs::path& inputPath, const fs::path& outputPath, AudioFormat format, VBRQualities quality)
    {
        if (!fc::IsSpecficAudioFormat(inputPath, op::AudioFormat::FLAC) && !fc::IsSpecficAudioFormat(inputPath, op::AudioFormat::WAV))
        {
            warn("Input file is not a lossless file. Conversion may result in quality loss. :(");
        }


        /* Redundant check here, alr implemnted after strcmp passes for a -cvrt / --convert flag */

        // // Output path here will be the global var -po if specified.
        // if (!fs::exists(outputPath) || !fs::is_directory(outputPath))
        // {
        //     err("Output path does not exist or is not a regular file!");
        //     return; // exit function early due to invalid output path
        // }


        (void)quality;

        fs::path outputFile = outputPath;
        if (fs::is_directory(outputPath)) {
            outputFile = outputPath / (inputPath.stem().string() + OutputExtensionForFormat(format));
        }

        int outputFormat = 0;
        switch (format)
        {
            case AudioFormat::WAV:
                outputFormat = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
                break;
            case AudioFormat::FLAC:
                outputFormat = SF_FORMAT_FLAC | SF_FORMAT_PCM_16;
                break;
            case AudioFormat::OGG:
                outputFormat = SF_FORMAT_OGG | SF_FORMAT_VORBIS;
                break;
            default:
                err("Library-only conversion currently supports WAV, FLAC, and OGG output.");
                warn("For other formats, add a libavcodec-based transcoder path.");
                return;
        }

        SF_INFO inInfo{};
        SNDFILE* inFile = sf_open(inputPath.string().c_str(), SFM_READ, &inInfo);
        if (!inFile)
        {
            err("Failed to open input audio file for conversion.");
            return;
        }

        SF_INFO outInfo{};
        outInfo.samplerate = inInfo.samplerate;
        outInfo.channels = inInfo.channels;
        outInfo.format = outputFormat;

        if (!sf_format_check(&outInfo))
        {
            err("Output format is not supported by the current libsndfile build.");
            sf_close(inFile);
            return;
        }

        SNDFILE* outFile = sf_open(outputFile.string().c_str(), SFM_WRITE, &outInfo);
        if (!outFile)
        {
            err("Failed to create output audio file for conversion.");
            sf_close(inFile);
            return;
        }

        constexpr sf_count_t frameBlockSize = 4096;
        std::vector<float> pcmBuffer(static_cast<std::size_t>(frameBlockSize * inInfo.channels));

        sf_count_t framesRead = 0;
        while ((framesRead = sf_readf_float(inFile, pcmBuffer.data(), frameBlockSize)) > 0)
        {
            sf_count_t framesWritten = sf_writef_float(outFile, pcmBuffer.data(), framesRead);
            if (framesWritten != framesRead)
            {
                err("Audio conversion write failed before all frames were written.");
                sf_close(inFile);
                sf_close(outFile);
                return;
            }
        }

        sf_close(inFile);
        sf_close(outFile);

        if (InputHasAttachedCover(inputPath) && !FormatSupportsAttachedCover(format))
        {
            warn("Input appears to have cover art, but the library-only converter currently writes audio stream only.");
        }

        if (InputHasAttachedCover(inputPath) && FormatSupportsAttachedCover(format))
        {
            warn("Cover art passthrough is not implemented in the library-only converter yet.");
        }

        TagLib::FileRef inTagRef(inputPath.string().c_str());
        TagLib::FileRef outTagRef(outputFile.string().c_str());
        if (!inTagRef.isNull() && inTagRef.tag() && !outTagRef.isNull() && outTagRef.tag())
        {
            outTagRef.tag()->setTitle(inTagRef.tag()->title());
            outTagRef.tag()->setArtist(inTagRef.tag()->artist());
            outTagRef.tag()->setAlbum(inTagRef.tag()->album());
            outTagRef.tag()->setComment(inTagRef.tag()->comment());
            outTagRef.tag()->setGenre(inTagRef.tag()->genre());
            outTagRef.tag()->setYear(inTagRef.tag()->year());
            outTagRef.tag()->setTrack(inTagRef.tag()->track());
            outTagRef.file()->save();
        }

        if (format != AudioFormat::WAV && format != AudioFormat::FLAC && format != AudioFormat::OGG)
        {
            return;
        }

        yay("Conversion completed successfully!");
        plog("Output file:");
        yay(outputFile.c_str());
    }

    void ApplyReplayGain(const fs::path& path, ReplayGainByTrack trackGainInfo, ReplayGainByAlbum albumGainInfo)
    {
        bool trackInfoEmpty = (trackGainInfo.trackGain == 0.0f && trackGainInfo.trackPeak == 0.0f);
        bool albumInfoEmpty = (albumGainInfo.albumGain == 0.0f && albumGainInfo.albumPeak == 0.0f);

        TagLib::FileRef f(path.c_str());
        if (f.isNull() || !f.audioProperties() || !f.file()) {
            err("Failed to read audio file for replaygain application.");
            return;
        }
        TagLib::PropertyMap properties = f.file()->properties();

        AudioMetadata existingMetadata = GetMetaData(path);
        
        if (trackInfoEmpty && albumInfoEmpty) {
            return;
        }
        if (trackInfoEmpty) {
            const std::string trackTitle = existingMetadata.title.empty() ? "<unknown title>" : existingMetadata.title;
            const std::string trackArtist = existingMetadata.artist.empty() ? "<unknown artist>" : existingMetadata.artist;
            printf("--- %s - %s\nAlbum Gain / Peak: %.2f dB / %.6f\n\n",
                   trackTitle.c_str(),
                   trackArtist.c_str(),
                   albumGainInfo.albumGain,
                   albumGainInfo.albumPeak);
            StageMetaDataChanges(properties, "REPLAYGAIN_ALBUM_GAIN", std::to_string(albumGainInfo.albumGain) + " dB");
            StageMetaDataChanges(properties, "REPLAYGAIN_ALBUM_PEAK", std::to_string(albumGainInfo.albumPeak));
            CommitMetaDataChanges(path, properties);
            return;
        }
        if (albumInfoEmpty) {
            const std::string trackTitle = existingMetadata.title.empty() ? "<unknown title>" : existingMetadata.title;
            const std::string trackArtist = existingMetadata.artist.empty() ? "<unknown artist>" : existingMetadata.artist;
            printf("--- %s - %s\nTrack Gain / Peak: %.2f dB / %.6f\n\n",
                   trackTitle.c_str(),
                   trackArtist.c_str(),
                   trackGainInfo.trackGain,
                   trackGainInfo.trackPeak);
            StageMetaDataChanges(properties, "REPLAYGAIN_TRACK_GAIN", std::to_string(trackGainInfo.trackGain) + " dB");
            StageMetaDataChanges(properties, "REPLAYGAIN_TRACK_PEAK", std::to_string(trackGainInfo.trackPeak));
            CommitMetaDataChanges(path, properties);
            return;
        }

        // Fall back to applying both if both are provided
         const std::string trackTitle = existingMetadata.title.empty() ? "<unknown title>" : existingMetadata.title;
         const std::string trackArtist = existingMetadata.artist.empty() ? "<unknown artist>" : existingMetadata.artist;
         printf("--- %s - %s\nTrack Gain / Peak: %.2f dB / %.6f\nAlbum Gain / Peak: %.2f dB / %.6f\n\n",
             trackTitle.c_str(),
             trackArtist.c_str(),
             trackGainInfo.trackGain,
             trackGainInfo.trackPeak,
             albumGainInfo.albumGain,
             albumGainInfo.albumPeak);
        StageMetaDataChanges(properties, "REPLAYGAIN_TRACK_GAIN", std::to_string(trackGainInfo.trackGain) + " dB");
        StageMetaDataChanges(properties, "REPLAYGAIN_TRACK_PEAK", std::to_string(trackGainInfo.trackPeak));
        StageMetaDataChanges(properties, "REPLAYGAIN_ALBUM_GAIN", std::to_string(albumGainInfo.albumGain) + " dB");
        StageMetaDataChanges(properties, "REPLAYGAIN_ALBUM_PEAK", std::to_string(albumGainInfo.albumPeak));
        CommitMetaDataChanges(path, properties);
    }
    
    // Replaygain is calculated from EBU R128 integrated loudness,
    // with a ReplayGain-style -18 dB reference target.
    ReplayGainByTrack CalculateReplayGainTrack(const fs::path& path)
    {
        ReplayGainByTrack result{0.0f, 0.0f};

        

        TagLib::FileRef f(path.c_str());
        if (f.isNull() || !f.audioProperties()) {
            err("Failed to read audio file for replaygain calculation. :(");
            return result;
        }

        constexpr double targetLufs = -18.0;

        // Open audio file with libsndfile
        SF_INFO sfinfo{};
        SNDFILE* sndfile = sf_open(path.string().c_str(), SFM_READ, &sfinfo);
        if (!sndfile) {
            err("Failed to open audio file with libsndfile for replaygain calculation. :(");
            return result;
        }
        
        ebur128_state* st = ebur128_init(sfinfo.channels, sfinfo.samplerate, EBUR128_MODE_I | EBUR128_MODE_TRUE_PEAK);
        if (!st) {
            err("Failed to initialize ebur128 state for replaygain calculation. :(");
            sf_close(sndfile);
            return result;
        }

        const int BUFFERSIZE = 4096;
        std::vector<float> buffer(BUFFERSIZE * sfinfo.channels);
        sf_count_t readcount;
        while ((readcount = sf_readf_float(sndfile, buffer.data(), BUFFERSIZE)) > 0) {
            if (ebur128_add_frames_float(st, buffer.data(), readcount) != EBUR128_SUCCESS) {
                err("Failed to feed PCM frames to ebur128. :(");
                ebur128_destroy(&st);
                sf_close(sndfile);
                return result;
            }
        }

        if (readcount < 0) {
            err("Error while reading PCM frames from audio file. :(");
            ebur128_destroy(&st);
            sf_close(sndfile);
            return result;
        }

        double integratedLufs = 0.0;
        if (ebur128_loudness_global(st, &integratedLufs) != EBUR128_SUCCESS) {
            err("Failed to compute integrated loudness with ebur128. :(");
            ebur128_destroy(&st);
            sf_close(sndfile);
            return result;
        }

        double maxTruePeak = 0.0;
        for (unsigned int ch = 0; ch < static_cast<unsigned int>(sfinfo.channels); ++ch) {
            double channelPeak = 0.0;
            if (ebur128_true_peak(st, ch, &channelPeak) == EBUR128_SUCCESS) {
                maxTruePeak = std::max(maxTruePeak, channelPeak);
            }
        }

        result.trackGain = static_cast<float>(targetLufs - integratedLufs);
        result.trackPeak = static_cast<float>(maxTruePeak);

        ebur128_destroy(&st);
        sf_close(sndfile);
        return result;



    }

    void CalculateReplayGainAlbum(const fs::path& path)
    {
        if (!fs::exists(path) || !fs::is_directory(path)) {
            err("Album replaygain calculation failed: input path does not exist or is not a directory.");
            warn("Album replaygain calculation requires a directory input containing audio files from the same album.");
            return;
        }

        // 1) Group valid audio files by album tag.
        std::unordered_map<std::string, std::vector<fs::path>> albumMap;
        for (const auto& entry : fs::directory_iterator(path)) {
            if (fc::IsValidAudioFile(entry.path())) {
                TagLib::FileRef f(entry.path().c_str());
                if (!f.isNull() && f.tag()) {
                    std::string album = f.tag()->album().to8Bit(true);
                    albumMap[album].push_back(entry.path());
                }
            }
        }

        if (albumMap.empty()) {
            warn("No valid tagged audio files found for album replaygain calculation.");
            return;
        }

        if (albumMap.size() > 1) {
            warn("Multiple albums detected in directory. Applying album replaygain per album group by album tag.");
        }

        // 2) Calculate track replaygain in parallel, then derive album replaygain per group.
        std::vector<fs::path> allTracks;
        for (const auto& [album, paths] : albumMap) {
            allTracks.insert(allTracks.end(), paths.begin(), paths.end());
        }

        if (allTracks.empty()) {
            warn("No audio tracks found after album grouping.");
            return;
        }

        const unsigned int hardwareThreads = std::thread::hardware_concurrency();
        const std::size_t desiredWorkers = std::max<std::size_t>(1, static_cast<std::size_t>(hardwareThreads / 2));
        const std::size_t workerCount = std::min<std::size_t>(desiredWorkers, allTracks.size());
        std::vector<ReplayGainByTrack> trackResults(allTracks.size(), ReplayGainByTrack{0.0f, 0.0f});
        std::atomic<std::size_t> nextIndex{0};
        std::vector<std::future<void>> workers;
        workers.reserve(workerCount);

        for (std::size_t worker = 0; worker < workerCount; ++worker) {
            workers.push_back(std::async(std::launch::async, [&]() {
                while (true) {
                    const std::size_t index = nextIndex.fetch_add(1);
                    if (index >= allTracks.size()) {
                        break;
                    }
                    trackResults[index] = CalculateReplayGainTrack(allTracks[index]);
                }
            }));
        }

        for (auto& worker : workers) {
            worker.get();
        }

        std::unordered_map<std::string, ReplayGainByTrack> trackReplayGainByPath;
        trackReplayGainByPath.reserve(allTracks.size());
        for (std::size_t i = 0; i < allTracks.size(); ++i) {
            trackReplayGainByPath[allTracks[i].string()] = trackResults[i];
        }

        std::unordered_map<std::string, ReplayGainByAlbum> albumReplayGain;

        for (const auto& [album, paths] : albumMap) {
            double totalGain = 0.0;
            double maxPeak = 0.0;
            int trackCount = 0;

            for (const auto& trackPath : paths) {
                ReplayGainByTrack trackGainInfo = trackReplayGainByPath[trackPath.string()];
                totalGain += trackGainInfo.trackGain;
                maxPeak = std::max(maxPeak, static_cast<double>(trackGainInfo.trackPeak));
                trackCount++;
            }

            if (trackCount > 0) {
                albumReplayGain[album] = ReplayGainByAlbum{
                    static_cast<float>(totalGain / trackCount),
                    static_cast<float>(maxPeak)
                };

                const std::string albumName = album.empty() ? "<unknown album>" : album;
                plog(("Album replaygain calculated: " + albumName +
                      " | tracks=" + std::to_string(trackCount) +
                      " | gain=" + std::to_string(albumReplayGain[album].albumGain) + " dB" +
                      " | peak=" + std::to_string(albumReplayGain[album].albumPeak)).c_str());
            }
        }

        // 3) Apply previously calculated album replaygain values in a separate pass.
        for (const auto& [album, paths] : albumMap) {
            auto it = albumReplayGain.find(album);
            if (it == albumReplayGain.end()) {
                continue;
            }

            for (const auto& trackPath : paths) {
                ApplyReplayGain(trackPath, {}, it->second); // only apply album gain info
            }
        }
}

    }