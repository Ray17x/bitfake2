#include <cstdlib>
#include <cmath>
#include <cfloat>
#include "Utilites/consoleout.hpp"
using namespace ConsoleOut;
#include <filesystem>
namespace fs = std::filesystem;
#include <algorithm>
#include "Utilites/filechecks.hpp"
namespace fc = FileChecks;
#include "Utilites/operations.hpp"
namespace op = Operations;
#include "Utilites/globals.hpp"
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/xiphcomment.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <regex>
#include <fftw3.h>
#include <cctype>
#include <sndfile.h>

/*
    helperfunctions.cpp
    This file will contain the implementation of any helper functions that are used across the project.
    The idea is to keep the main function clean and delegate any non-core operations to this file.
    This can include things like metadata extraction, replay gain calculations, MusicBrainz lookups, and more.

    For functions that might request online resources, its best we define them in a different file.
*/

namespace Operations
{
    // Helper Functions

    AudioMetadata GetMetaData(const fs::path& path)
    {
        AudioMetadata tmpdata;
        if (!fs::exists(path) || !fs::is_regular_file(path))
        {
            warn("Metadata read failed: file does not exist or is not a regular file.");
            return tmpdata;
        }

        TagLib::FileRef fileRef(path.string().c_str());
        if (fileRef.isNull() || fileRef.tag() == nullptr)
        {
            warn("Metadata read failed: unsupported format or unreadable tags.");
            return tmpdata;
        }

        TagLib::Tag* tag = fileRef.tag();
        if (tag->isEmpty())
        {
            warn("Metadata read: tag is empty.");
            return tmpdata;
        }

        tmpdata.title = tag->title().to8Bit(true);
        tmpdata.artist = tag->artist().to8Bit(true);
        tmpdata.album = tag->album().to8Bit(true);
        tmpdata.genre = tag->genre().to8Bit(true);
        tmpdata.trackNumber = tag->track();
        if (tag->year() > 0)
        {
            tmpdata.date = std::to_string(tag->year());
        }

        // Try to extract additional metadata from Xiph comments (for Opus, Vorbis, FLAC)
        if (auto* xc = dynamic_cast<TagLib::Ogg::XiphComment*>(fileRef.file()->tag()))
        {
            auto fields = xc->fieldListMap();
            
            // Override with Xiph comment values if available and not empty
            if (fields.find("TITLE") != fields.end() && !fields["TITLE"].isEmpty() && tmpdata.title.empty())
            {
                tmpdata.title = fields["TITLE"][0].to8Bit(true);
            }
            if (fields.find("ARTIST") != fields.end() && !fields["ARTIST"].isEmpty() && tmpdata.artist.empty())
            {
                tmpdata.artist = fields["ARTIST"][0].to8Bit(true);
            }
            if (fields.find("ALBUM") != fields.end() && !fields["ALBUM"].isEmpty() && tmpdata.album.empty())
            {
                tmpdata.album = fields["ALBUM"][0].to8Bit(true);
            }
            if (fields.find("GENRE") != fields.end() && !fields["GENRE"].isEmpty() && tmpdata.genre.empty())
            {
                tmpdata.genre = fields["GENRE"][0].to8Bit(true);
            }
            if (fields.find("DATE") != fields.end() && !fields["DATE"].isEmpty() && tmpdata.date.empty())
            {
                std::string dateStr = fields["DATE"][0].to8Bit(true);
                // Extract year from DATE field (format: YYYY or YYYY-MM-DD)
                if (dateStr.length() >= 4)
                {
                    tmpdata.date = dateStr.substr(0, 4);
                }
            }
            if (fields.find("TRACKNUMBER") != fields.end() && !fields["TRACKNUMBER"].isEmpty() && tmpdata.trackNumber == 0)
            {
                try {
                    std::string trackStr = fields["TRACKNUMBER"][0].to8Bit(true);
                    tmpdata.trackNumber = std::stoi(trackStr);
                } catch (...) {}
            }
        }



        return tmpdata;
    } // GetMetaData

    std::vector<AudioMetadataResult> GetMetaDataList(const fs::path& path)
    {
        std::vector<AudioMetadataResult> results;

        if (!fs::exists(path) || !fc::IsValidAudioFile(path))
        {
            warn("Metadata list failed: input path does not exist or is not a valid audio file.");
            return results;
        }

        if (fs::is_directory(path))
        {
            for (const auto& entry : fs::directory_iterator(path))
            {
                if (fc::IsValidAudioFile(entry.path()))
                {
                    results.push_back({entry.path(), GetMetaData(entry.path())});
                }
            }

            if (results.empty())
            {
                warn("Metadata list: no valid audio files found in directory.");
            }
            else
            {
                std::sort(results.begin(), results.end(),
                    [](const AudioMetadataResult& a, const AudioMetadataResult& b)
                    {
                        return a.metadata.trackNumber < b.metadata.trackNumber;
                    });
            }
            return results;
        }

        if (!fc::IsValidAudioFile(path))
        {
            warn("Metadata list failed: input file is not a valid audio file.");
            return results;
        }

        results.push_back({path, GetMetaData(path)});
        return results;
    } // GetMetaDataList


    ReplayGainInfo GetReplayGain(const fs::path& path)
    {
        // No calculations here; read ReplayGain tags directly with TagLib.

        ReplayGainInfo result;

        if (!fs::exists(path) || !fs::is_regular_file(path))
        {
            warn("ReplayGain read failed: file does not exist or is not a regular file.");
            return result;
        }

        TagLib::FileRef f(path.string().c_str());
        if (f.isNull() || f.tag() == nullptr)
        {
            warn("ReplayGain read failed: unsupported format or unreadable tags.");
            return result;
        }

        TagLib::PropertyMap properties = f.file()->properties();

        // getting title/artist/album for better logging context
        result.title = f.tag()->title().to8Bit(true);
        result.artist = f.tag()->artist().to8Bit(true);
        result.album = f.tag()->album().to8Bit(true);
        result.trackNumber = f.tag()->track();

        if (properties.find("REPLAYGAIN_TRACK_GAIN") != properties.end() && properties.find("REPLAYGAIN_TRACK_PEAK") != properties.end())
        {
            try {
                std::string trackGainStr = properties["REPLAYGAIN_TRACK_GAIN"].toString().to8Bit(true);
                std::string trackPeakStr = properties["REPLAYGAIN_TRACK_PEAK"].toString().to8Bit(true);
                result.trackGain = std::stof(trackGainStr);
                result.trackPeak = std::stof(trackPeakStr);
            } catch (...) {
                warn("ReplayGain read: failed to parse track gain/peak values.");
            }
        }

        // Album gain/peak are optional, so we won't log warnings if they're missing. Just try to read if they exist.
        if (properties.find("REPLAYGAIN_ALBUM_GAIN") != properties.end() && properties.find("REPLAYGAIN_ALBUM_PEAK") != properties.end())
        {
            try {
                std::string albumGainStr = properties["REPLAYGAIN_ALBUM_GAIN"].toString().to8Bit(true);
                std::string albumPeakStr = properties["REPLAYGAIN_ALBUM_PEAK"].toString().to8Bit(true);
                result.albumGain = std::stof(albumGainStr);
                result.albumPeak = std::stof(albumPeakStr);
            } catch (...) {
                warn("ReplayGain read: failed to parse album gain/peak values.");
            }
        }

        return result;

    } // GetReplayGain


    std::vector<ReplayGainResult> GetReplayGainList(const fs::path& path)
    {
        std::vector<ReplayGainResult> results;

        if (!fs::exists(path))
        {
            warn("ReplayGain list failed: input path does not exist.");
            return results;
        }

        if (fs::is_directory(path))
        {
            for (const auto& entry : fs::directory_iterator(path))
            {
                if (fc::IsValidAudioFile(entry.path()))
                {
                    results.push_back({entry.path(), GetReplayGain(entry.path())});
                }
            }

            if (results.empty())
            {
                warn("ReplayGain list: no valid audio files found in directory.");
            }
            else
            {
                std::sort(results.begin(), results.end(),
                    [](const ReplayGainResult& a, const ReplayGainResult& b)
                    {
                        return a.info.trackNumber < b.info.trackNumber;
                    });
            }
            return results;
        }

        if (!fc::IsValidAudioFile(path))
        {
            warn("ReplayGain list failed: input file is not a valid audio file.");
            return results;
        }

        results.push_back({path, GetReplayGain(path)});
        return results;
    } // GetReplayGainList

    SpectralAnalysisResult SpectralAnalysis(const fs::path& path)
    {
        SpectralAnalysisResult tmpresult;
        std::vector<float> samples;
        const int BUFFER_SIZE = 4096;
        const int FFT_SIZE = 4096;

        float buffer[BUFFER_SIZE];
        sf_count_t bytesRead;

        if (!fs::exists(path) || !fs::is_regular_file(path))
        {
            err("Spectral analysis failed: file does not exist/is not a regular file.");
            tmpresult.diagnosis = "Error: File not found or invalid.";
            return tmpresult;
        }

        // libsndfile to do PCM stuff
        SF_INFO sfinfo{};
        SNDFILE* sndfile = sf_open(path.string().c_str(), SFM_READ, &sfinfo);
        if (!sndfile)
        {
            err("Spectral analysis failed: unable to open audio file for reading.");
            tmpresult.diagnosis = "Error: Unable to read audio file.";
            return tmpresult;
        }

        while ((bytesRead = sf_read_float(sndfile, buffer, BUFFER_SIZE)) > 0)
        {
            samples.insert(samples.end(), buffer, buffer + bytesRead);
        }

        sf_close(sndfile);

        if (samples.empty())
        {
            err("Spectral analysis failed: decoder produced no audio samples.");
            tmpresult.diagnosis = "Error: Could not decode audio samples.";
            return tmpresult;
        }

        int numChunks = static_cast<int>(samples.size() / FFT_SIZE);
        if (numChunks == 0)
        {
            err("Spectral analysis failed: insufficient audio samples.");
            tmpresult.diagnosis = "Error: Not enough samples for analysis.";
            return tmpresult;
        }

        // Setup FFT
        fftw_plan plan;
        fftw_complex *in, *out;
        in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
        out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
        plan = fftw_plan_dft_1d(FFT_SIZE, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
        std::vector<float> MeanMagnitude(FFT_SIZE / 2, 0.0f);

        int actualChunks = 0;
        for (int chunk = 0; chunk < numChunks && chunk < 500; chunk++)
        {
            for (int i = 0; i < FFT_SIZE; i++)
            {
                in[i][0] = samples[chunk * FFT_SIZE + i];
                in[i][1] = 0.0;
            }

            fftw_execute(plan);

            for (int i = 0; i < FFT_SIZE / 2; i++)
            {
                float magnitude = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);
                MeanMagnitude[i] += magnitude;
            }
            actualChunks++;
        }

        if (actualChunks > 0) {
            for (int i = 0; i < FFT_SIZE / 2; i++) {
                MeanMagnitude[i] /= actualChunks;
            }
        }

        TagLib::FileRef fileRef(path.string().c_str());
        float sampleRate = fileRef.audioProperties() ? fileRef.audioProperties()->sampleRate() : 44100.0f;
        int bitrate = fileRef.audioProperties() ? fileRef.audioProperties()->bitrate() : 0;  // Get bitrate metadata

        std::vector<float> bandEnergies(10, 0.0f);
        int bandSize = (FFT_SIZE / 2) / 10;
        
        for (int b = 0; b < 10; b++)
        {
            for (int i = b * bandSize; i < (b + 1) * bandSize && i < FFT_SIZE / 2; i++)
            {
                bandEnergies[b] += MeanMagnitude[i];
            }
            bandEnergies[b] /= bandSize;
        }
        
        // stats
        float bandMean = 0.0f;
        for (float e : bandEnergies) bandMean += e;
        bandMean /= 10.0f;
        
        float bandVariance = 0.0f;
        for (float e : bandEnergies)
        {
            float diff = e - bandMean;
            bandVariance += diff * diff;
        }
        bandVariance /= 10.0f;
        float bandStdDev = sqrt(bandVariance);
        float coefficientOfVariation = bandMean > 0 ? (bandStdDev / bandMean) : 0.0f;

        tmpresult.frequencyCutoff = sampleRate / 2.0f;  // Nyquist
        
        // Multi-metric detection: spectral analysis + bitrate heuristic
        // High CV > 1.95 = lossy (catches high-bitrate MP3)
        // Low-Medium CV with 32-400 kbps = likely MP3 (catches V0, V1, V9, etc.)
        // Low CV with variable/0 bitrate = likely lossless (FLAC, WAV, etc.)
        bool spectralLossy = coefficientOfVariation > 1.95f;
        bool bitrateLossy = (bitrate > 32 && bitrate < 400);  // Catch all MP3 VBR variants
        tmpresult.likelyLossy = spectralLossy || bitrateLossy;
        
        tmpresult.noiseFlorElevation = coefficientOfVariation * 100.0f;
        tmpresult.bandingScore = coefficientOfVariation;

        if (tmpresult.likelyLossy)
        {
            tmpresult.diagnosis = "Likely lossy: Uneven spectrum distribution detected.";
        }
        else
        {
            tmpresult.diagnosis = "Likely lossless: Even spectrum distribution detected.";
        }

        fftw_destroy_plan(plan);
        fftw_free(in);
        fftw_free(out);

        tmpresult.title = fileRef.tag() ? fileRef.tag()->title().to8Bit(true) : "";
        tmpresult.artist = fileRef.tag() ? fileRef.tag()->artist().to8Bit(true) : "";
        tmpresult.album = fileRef.tag() ? fileRef.tag()->album().to8Bit(true) : "";

        return tmpresult;
    } // SpectralAnalysis

    std::vector<SpectralAnalysisResult> SpectralAnalysisList(const fs::path& path)
    {
        std::vector<SpectralAnalysisResult> results;

        if (!fs::exists(path))
        {
            warn("Spectral analysis list failed: input path does not exist.");
            return results;
        }

        if (fs::is_directory(path))
        {
            for (const auto& entry : fs::directory_iterator(path))
            {
                if (fc::IsValidAudioFile(entry.path()))
                {
                    results.push_back(SpectralAnalysis(entry.path()));
                }
            }

            if (results.empty())
            {
                warn("Spectral analysis list: no valid audio files found in directory.");
            }
            else
            {
                std::sort(results.begin(), results.end(),
                    [](const SpectralAnalysisResult& a, const SpectralAnalysisResult& b)
                    {
                        return a.title < b.title; // Sort by title for simplicity
                    });
            }
            return results;
        }

        if (!fc::IsValidAudioFile(path))
        {
            warn("Spectral analysis list failed: input file is not a valid audio file.");
            return results;
        }

        results.push_back(SpectralAnalysis(path));
        return results;
    } // SpectralAnalysisList

    bool InputHasAttachedCover(const fs::path& inputPath)
    {
        if (!fs::exists(inputPath) || !fs::is_regular_file(inputPath)) {
            return false;
        }

        const std::string ext = inputPath.extension().string();

        if (ext == ".mp3")
        {
            TagLib::MPEG::File file(inputPath.string().c_str());
            TagLib::ID3v2::Tag* tag = file.ID3v2Tag(false);
            if (!tag)
            {
                return false;
            }

            const auto frames = tag->frameListMap()["APIC"];
            return !frames.isEmpty();
        }

        if (ext == ".flac")
        {
            TagLib::FLAC::File file(inputPath.string().c_str());
            return !file.pictureList().isEmpty();
        }

        return false;
    }

    bool FormatSupportsAttachedCover(AudioFormat format)
    {
        return format == AudioFormat::MP3 || format == AudioFormat::FLAC;
    }

    std::string OutputExtensionForFormat(AudioFormat format)
    {
        switch (format)
        {
            case AudioFormat::MP3: return ".mp3";
            case AudioFormat::OGG: return ".ogg";
            case AudioFormat::FLAC: return ".flac";
            case AudioFormat::WAV: return ".wav";
            case AudioFormat::OPUS: return ".opus";
            case AudioFormat::AAC: return ".aac";
            default: return ".audio";
        }
    }
} // namespace Operations

// constexpr AudioMetadata GetMetaData(const fs::path& path);


// constexpr ReplayGainInfo GetReplayGain(const fs::path& path);
// constexpr MusicBrainzInfo GetMusicBrainzInfo(const fs::path& path);