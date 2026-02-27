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
#include <regex>
#include <fftw3.h>
#include <cctype>

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
            tmpdata.year = std::to_string(tag->year());
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
            if (fields.find("DATE") != fields.end() && !fields["DATE"].isEmpty() && tmpdata.year.empty())
            {
                std::string dateStr = fields["DATE"][0].to8Bit(true);
                // Extract year from DATE field (format: YYYY or YYYY-MM-DD)
                if (dateStr.length() >= 4)
                {
                    tmpdata.year = dateStr.substr(0, 4);
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

        if (!fs::exists(path))
        {
            warn("Metadata list failed: input path does not exist.");
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
        ReplayGainInfo tmpdata = {"", "", "", 0, 0.0f, 0.0f, 0.0f, 0.0f};


        if (!fs::exists(path) || !fs::is_regular_file(path))
        {
            warn("ReplayGain read failed: file does not exist or is not a regular file.");
            return tmpdata;
        }

        TagLib::FileRef fileRef(path.string().c_str());
        if (fileRef.isNull() || fileRef.tag() == nullptr)
        {
            warn("ReplayGain read failed: unsupported format or unreadable tags.");
            return tmpdata;
        }

        // Still reliable for getting basic stuff

        tmpdata.title = fileRef.tag()->title().to8Bit(true);
        tmpdata.artist = fileRef.tag()->artist().to8Bit(true);
        tmpdata.album = fileRef.tag()->album().to8Bit(true);
        tmpdata.trackNumber = fileRef.tag()->track();


        // // Get Xiph comment for ReplayGain (works for Opus, Vorbis, FLAC, etc.)
        // if (auto* xc = dynamic_cast<TagLib::Ogg::XiphComment*>(fileRef.file()->tag()))
        // {
        //     auto fields = xc->fieldListMap();
            
        //     // Extract ReplayGain values and remove the "dB" suffix
        //     if (fields.find("REPLAYGAIN_TRACK_GAIN") != fields.end() && !fields["REPLAYGAIN_TRACK_GAIN"].isEmpty())
        //     {
        //         std::string gainStr = fields["REPLAYGAIN_TRACK_GAIN"][0].to8Bit(true);
        //         // Remove " dB" suffix if present
        //         gainStr = std::regex_replace(gainStr, std::regex(" dB$"), "");
        //         try {
        //             tmpdata.trackGain = std::stof(gainStr);
        //         } catch (...) {}
        //     }
            
        //     if (fields.find("REPLAYGAIN_TRACK_PEAK") != fields.end() && !fields["REPLAYGAIN_TRACK_PEAK"].isEmpty())
        //     {
        //         std::string peakStr = fields["REPLAYGAIN_TRACK_PEAK"][0].to8Bit(true);
        //         try {
        //             tmpdata.trackPeak = std::stof(peakStr);
        //         } catch (...) {}
        //     }
            
        //     if (fields.find("REPLAYGAIN_ALBUM_GAIN") != fields.end() && !fields["REPLAYGAIN_ALBUM_GAIN"].isEmpty())
        //     {
        //         std::string gainStr = fields["REPLAYGAIN_ALBUM_GAIN"][0].to8Bit(true);
        //         gainStr = std::regex_replace(gainStr, std::regex(" dB$"), "");
        //         try {
        //             tmpdata.albumGain = std::stof(gainStr);
        //         } catch (...) {}
        //     }
            
        //     if (fields.find("REPLAYGAIN_ALBUM_PEAK") != fields.end() && !fields["REPLAYGAIN_ALBUM_PEAK"].isEmpty())
        //     {
        //         std::string peakStr = fields["REPLAYGAIN_ALBUM_PEAK"][0].to8Bit(true);
        //         try {
        //             tmpdata.albumPeak = std::stof(peakStr);
        //         } catch (...) {}
        //     }
        // }

        // It does not work. I am now sad. :(

        // Get ReplayGain tags via ffprobe from both container and stream scopes.
        std::string ffprbCmd =
            "ffprobe -v error -select_streams a:0 "
            "-show_entries "
            "format_tags=REPLAYGAIN_TRACK_GAIN,REPLAYGAIN_TRACK_PEAK,REPLAYGAIN_ALBUM_GAIN,REPLAYGAIN_ALBUM_PEAK:"
            "stream_tags=REPLAYGAIN_TRACK_GAIN,REPLAYGAIN_TRACK_PEAK,REPLAYGAIN_ALBUM_GAIN,REPLAYGAIN_ALBUM_PEAK "
            "-of default=noprint_wrappers=1 \"" + path.string() + "\"";
        FILE* pipe = popen(ffprbCmd.c_str(), "r");
        if (!pipe)
        {
            warn("ReplayGain read warning: failed to execute ffprobe for replay gain extraction.");
            return tmpdata;
        }

        auto trim = [](std::string& value)
        {
            const auto first = value.find_first_not_of(" \n\r\t");
            if (first == std::string::npos)
            {
                value.clear();
                return;
            }
            const auto last = value.find_last_not_of(" \n\r\t");
            value = value.substr(first, last - first + 1);
        };

        auto parseReplayGainValue = [&trim](std::string value, float& output) -> bool
        {
            trim(value);
            if (value.empty())
            {
                return false;
            }

            if (value.size() >= 2)
            {
                char c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(value[value.size() - 2])));
                char c2 = static_cast<char>(std::tolower(static_cast<unsigned char>(value[value.size() - 1])));
                if (c1 == 'd' && c2 == 'b')
                {
                    value.erase(value.size() - 2);
                    trim(value);
                }
            }

            try
            {
                output = std::stof(value);
                return true;
            }
            catch (...)
            {
                return false;
            }
        };

        char buffer[128];
        bool foundTrackGain = false;
        bool foundTrackPeak = false;
        bool foundAlbumGain = false;
        bool foundAlbumPeak = false;

        while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            std::string line(buffer);

            trim(line);
            if (line.empty())
            {
                continue;
            }

            const auto equalsPos = line.find('=');
            if (equalsPos == std::string::npos || equalsPos == 0)
            {
                continue;
            }

            std::string key = line.substr(0, equalsPos);
            std::string value = line.substr(equalsPos + 1);

            if (key.rfind("TAG:", 0) == 0)
            {
                key = key.substr(4);
            }

            std::transform(key.begin(), key.end(), key.begin(),
                [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

            if (key == "REPLAYGAIN_TRACK_GAIN" && parseReplayGainValue(value, tmpdata.trackGain))
            {
                foundTrackGain = true;
            }
            else if (key == "REPLAYGAIN_TRACK_PEAK" && parseReplayGainValue(value, tmpdata.trackPeak))
            {
                foundTrackPeak = true;
            }
            else if (key == "REPLAYGAIN_ALBUM_GAIN" && parseReplayGainValue(value, tmpdata.albumGain))
            {
                foundAlbumGain = true;
            }
            else if (key == "REPLAYGAIN_ALBUM_PEAK" && parseReplayGainValue(value, tmpdata.albumPeak))
            {
                foundAlbumPeak = true;
            }
        }
        pclose(pipe);

        if (!(foundTrackGain || foundTrackPeak || foundAlbumGain || foundAlbumPeak))
        {
            warn("ReplayGain read warning: ffprobe did not return expected replay gain tags.");
        }

        return tmpdata;
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
        size_t bytesRead;

        if (!fs::exists(path) || !fs::is_regular_file(path))
        {
            err("Spectral analysis failed: file does not exist/is not a regular file.");
            tmpresult.diagnosis = "Error: File not found or invalid.";
            return tmpresult;
        }

        std::string ffmpegCmd = "ffmpeg -loglevel quiet -i \"" + path.string() + "\" -f f32le -acodec pcm_f32le pipe:1";
        
        FILE* pipe = popen(ffmpegCmd.c_str(), "r");
        if (!pipe)
        {
            err("Spectral analysis failed: could not open ffmpeg pipe.");
            tmpresult.diagnosis = "Error: Failed to execute ffmpeg.";
            return tmpresult;
        }

        while ((bytesRead = fread(buffer, sizeof(float), BUFFER_SIZE, pipe)) > 0)
        {
            samples.insert(samples.end(), buffer, buffer + bytesRead);
        }
        pclose(pipe);

        if (samples.empty())
        {
            err("Spectral analysis failed: ffmpeg produced no audio data.");
            tmpresult.diagnosis = "Error: ffmpeg failed or file is invalid.";
            return tmpresult;
        }

        int numChunks = samples.size() / FFT_SIZE;
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
        float freqPerBin = sampleRate / FFT_SIZE;

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
} // namespace Operations

// constexpr AudioMetadata GetMetaData(const fs::path& path);


// constexpr ReplayGainInfo GetReplayGain(const fs::path& path);
// constexpr MusicBrainzInfo GetMusicBrainzInfo(const fs::path& path);