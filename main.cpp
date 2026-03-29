#include <cstdlib>
#include "Utilities/consoleout.hpp"
using namespace ConsoleOut;
#include <filesystem>
#include <cstring>
#include <future>
#include <thread>
#include <atomic>
namespace fs = std::filesystem;
#include "Utilities/filechecks.hpp"
namespace fc = FileChecks;
#include "Utilities/operations.hpp"
namespace op = Operations;
#include "Utilities/globals.hpp"
namespace gb = globals;
#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>
#include <taglib/tag.h>
namespace tl = TagLib;

int main(int argc, char *argv[]) {

    /*
    Main function will be responsible for parsing command line arguments and invoking the appropriate functions in the
    Operations namespace. The idea is to keep this function clean and simple, and delegate the heavy lifting to the
    Operations namespace. This will also help with organization and maintainability as the project grows.
    */

    if (argc < 2) {
        // Checks if no arguments were provided and prints a helpful message instead of just doing nothing or crashing.
        err("No arguments provided! I can't do anything :(");
        plog("psst... use --help for more info!");
        return EXIT_FAILURE;
    }

    // First Pass grabs input file(s) and output file (if specified)
    for (int i = 1; i < argc; i++) {
        switch (argv[i][0]) {
        case '-':
            if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
                printf("Usage: ./bitfake2 [options]\n\n");
                printf("Options:\n");
                printf("  -h,    --help                            Show this help message\n");
                printf("  -i,    --input <path>                    Input audio file or directory path\n");
                printf("  -o,    --output <file>                   Output file path (must be .txt)\n");
                printf("  -po,   --pathout <directory>             Output directory path for conversion functions\n");
                printf("  -t,    --tag <tag:val>                   Tag to apply to input file (e.g. -t title:NewTitle)\n");
                printf("  -f,    --format <fmt[:q]>                Conversion type (e.g. mp3:V0, flac:L8, opus:160)\n");
                printf("  -gmd,  --getmetadata                     Get metadata of input file\n");
                printf("  -grg,  --getreplaygain                   Get ReplayGain information of input file\n");
                printf("  -sa,   --spectralanalysis                Perform spectral analysis on input file\n");
                printf("  -atrg, --applytrackreplaygain            Calculate track replaygain and apply to file(s)\n");
                printf("  -arg,  --applyalbumreplaygain            Calculate album replaygain and apply to file(s)\n");
                printf("  -oia,  --organizeintoalbums              Organize audio files into album subdirectories\n");
                printf("  -oiaa, --organizeintoartistalbum         Organize audio files into artist/album subdirectories\n");
                printf("  -cvrt, --convertto <fmt[:q]>             Convert input file(s) to specified format\n");
                printf("  -sg,   --spectrogram <output.png>        Generate spectrogram image from input audio file\n");
                printf("  -v,    --version                         Show program version\n");
                return EXIT_SUCCESS;
            }

            if (strcmp(argv[i], "--input") == 0 || strcmp(argv[i], "-i") == 0) {
                if (i + 1 < argc) {
                    // plog("Input file specified in flag: ");
                    // yay(argv[i + 1]);
                    gb::inputFile = argv[i + 1];
                    i++; // Skip the next argument since it's the input file
                } else {
                    err("Input flag provided but no input file specified!");
                    return EXIT_FAILURE;
                }
            }

            if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) {
                if (i + 1 < argc) {
                    // plog("Output path specified in flag: ");
                    // yay(argv[i + 1]);
                    gb::outputFile = argv[i + 1];
                    i++; // Skip the next argument since it's the output file
                } else {
                    err("Output flag provided but no output file specified!");
                    return EXIT_FAILURE;
                }
            }

            if (strcmp(argv[i], "--format") == 0 || strcmp(argv[i], "-f") == 0) {
                if (i + 1 < argc) {
                    std::string formatStr = argv[i + 1];
                    size_t colonPos = formatStr.find(':');
                    if (colonPos != std::string::npos) {
                        std::string qualityStr = formatStr.substr(colonPos + 1);
                        formatStr = formatStr.substr(0, colonPos);
                        gb::outputFormat = op::StringToAudioFormat(formatStr);
                        if (gb::outputFormat == op::AudioFormat::OPUS) {
                            try {
                                int bitrateKbps = std::stoi(qualityStr);
                                if (bitrateKbps < 0 || bitrateKbps > 512) {
                                    err("Opus bitrate must be between 0 and 512 kbps (inclusive). (Though I dont get "
                                        "why you would want 0..)");
                                    return EXIT_FAILURE;
                                }
                                gb::opusBitrateKbps = bitrateKbps;
                            } catch (...) {
                                err("Invalid Opus bitrate. Use an integer between 0 and 512 (e.g. opus:160).");
                                return EXIT_FAILURE;
                            }
                        } else {
                            gb::VBRQuality = op::StringToVBRQuality(qualityStr);
                        }
                    } else {
                        gb::outputFormat = op::StringToAudioFormat(formatStr);
                        if (gb::outputFormat == op::AudioFormat::OPUS) {
                            gb::opusBitrateKbps = 192;
                        }
                    }
                    // printf("format=%d quality=%d\n", static_cast<int>(gb::outputFormat),
                    // static_cast<int>(gb::VBRQuality));
                    i++; // Skip the next argument since it's the format
                } else {
                    err("Format flag provided but no format specified!");
                    warn("To use: -f/--format <format>[:<quality>] (e.g. -f mp3:V0, -f ogg:Q6, -f flac:L8, -f "
                         "opus:160)");
                    return EXIT_FAILURE;
                }
            }

            if (strcmp(argv[i], "--tag") == 0 || strcmp(argv[i], "-t") == 0) {
                if (i + 1 < argc) {
                    std::string tagStr = argv[i + 1];
                    size_t colonPos = tagStr.find(':');
                    if (colonPos != std::string::npos) {
                        std::string taggedValue = tagStr.substr(colonPos + 1);
                        tagStr = tagStr.substr(0, colonPos);
                        gb::tag = tagStr;
                        gb::val = taggedValue;
                        plog("Tag specified in flag: ");
                        yay((gb::tag + ":" + gb::val).c_str());
                    } else {
                        err("Tag flag provided but no tag value specified :^(");
                        warn("pssst!! Use me like this: -t/--tag <tag:val> (e.g. -t title:NewTitle, -t "
                             "REPLAY_GAIN_TRACK_GAIN:-12.35)");
                        warn("Hey! This is a testing feature, you CAN technically use it, but I recommened use the "
                             "Picard GUI as its much easier. :^)");
                        return EXIT_FAILURE;
                    }
                    i++; // Skip the next argument since it's the tag value
                }
            }

            if (strcmp(argv[i], "--pathout") == 0 || strcmp(argv[i], "-po") == 0) {
                if (i + 1 < argc) {
                    plog("Conversion output directory specified in flag: ");
                    yay(argv[i + 1]);
                    gb::conversionOutputDirectory = argv[i + 1];
                    i++; // Skip the next argument since it's the output directory
                }
            }

            if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
                printf("Bitfake Version %s\n", gb::version.c_str());
                printf("By Ray17x on Github <ray@atl.tools> (alias: koyomi / kero on various platforms)\n");
                printf("This software is provided as-is, without any express or implied warranty.\n In no event will the authors be "
                       "held liable for any damages arising from the use of this software.\n");
                return EXIT_SUCCESS;
            }

            // This first pass is only for grabbing input and output files, so we can apply it to other commands later!
            break;
        }
    }

    // static_cast<int>(gb::outputFormat), static_cast<int>(gb::VBRQuality)
    int fmt = static_cast<int>(gb::outputFormat);
    int q = static_cast<int>(gb::VBRQuality);

    // To the brave souls reading this code:
    // if (fmt == 0 && (q < 0 || q > 9)) { /* MP3: V0..V9 */ }
    // if (fmt == 1 && (q < 10 || q > 14)) { /* OGG: Q0..Q10 */ }
    // if (fmt == 4 && (q < 15 || q > 23)) { /* FLAC: L0..L8 */ }

    /*
        Invoking a check here to make sure the specified VBR quality is valid for the specified format. This is
       important because it will prevent us from passing invalid quality settings to the conversion functions later on,
       which could cause them to fail or produce unexpected results. By doing this check early on, we can provide
       immediate feedback to the user and prevent them from going through the entire conversion process only to find out
       that their quality setting was invalid. It's all about providing a better user experience and preventing
       frustration.
    */

    if (fmt == 0 && (q < 0 || q > 9)) {
        err("MP3 format requires a VBR quality between V0 and V9 (inclusive)! (Do not use Qx or Lx for MP3! :( )");
        printf("format=%d quality=%d\n", static_cast<int>(gb::outputFormat), static_cast<int>(gb::VBRQuality));
        return EXIT_FAILURE;
    }

    if (fmt == 1 && (q < 0 || q > 10)) {
        err("OGG VORBIS format requires a VBR quality between Q0 and Q10 (inclusive)! (Do not use Vx or Lx for OGG! :( "
            ")");
        printf("format=%d quality=%d\n", static_cast<int>(gb::outputFormat), static_cast<int>(gb::VBRQuality));
        return EXIT_FAILURE;
    }

    if (fmt == 4 && (q < 0 || q > 8)) {
        err("FLAC format requires an encoding level between L0 and L8 (inclusive)! (Do not use Vx or Qx for FLAC! :( "
            ")");
        printf("format=%d quality=%d\n", static_cast<int>(gb::outputFormat), static_cast<int>(gb::VBRQuality));
        return EXIT_FAILURE;
    }

    // File checks before handing it off to the 2nd pass
    // negate pathout checks since it can be used for other functions that don't require an output file
    // allow the program to continue forward without a specified po

    if (gb::outputFile.empty()) {
        plog("No output file specified. Output will be sent to terminal.");
        gb::outputToTerminal = true;
    } else if (gb::outputFile.extension() != ".txt") {
        warn("Output file must have .txt extension! Output will be sent to terminal instead.");
        gb::outputToTerminal = true;
    } else if (!fc::ParentExists(gb::outputFile)) {
        err("Output file parent directory does not exist!");
        return EXIT_FAILURE;
    } else {
        plog("Output file will be created/written to: ");
        yay(gb::outputFile.c_str());
        gb::outputToTerminal = false;
    }

    if (!fs::exists(gb::inputFile)) {
        err("Input file does not exist or is not a valid audio file!");
        err("Try again :(");
        return EXIT_FAILURE;
    }

    /*
        2nd Pass: Loop through arguments again and execute commands in Operations namespace. This separation allows us
       to have all the file checks and preparations done beforehand, so we can just focus on executing the commands in
       this pass without worrying about file validity or argument parsing.
    */

    for (int j = 1; j < argc; j++) {
        if (strcmp(argv[j], "-gmd") == 0 || strcmp(argv[j], "--getmetadata") == 0) {
            std::vector<op::AudioMetadataResult> results = op::GetMetaDataList(gb::inputFile);
            if (results.empty()) {
                err("No metadata found for input path.");
                return EXIT_FAILURE;
            }

            plog("Metadata for input file(s): ");

            if (gb::outputToTerminal) {
                for (const auto &result : results) {
                    printf("----------------------\n");
                    printf("Track #%d: %s\n", result.metadata.trackNumber, result.metadata.title.c_str());
                    printf("Artist: %s\n", result.metadata.artist.c_str());
                    printf("Album: %s\n", result.metadata.album.c_str());
                    printf("Genre: %s\n", result.metadata.genre.c_str());
                    printf("Year: %s\n", result.metadata.date.c_str());
                }
            } else {
                FILE *outFile = fopen(gb::outputFile.string().c_str(), "w");
                if (!outFile) {
                    err("Failed to open output file for writing.");
                    return EXIT_FAILURE;
                }
                for (const auto &result : results) {
                    fprintf(outFile, "----------------------\n");
                    fprintf(outFile, "Track #%d: %s\n", result.metadata.trackNumber, result.metadata.title.c_str());
                    fprintf(outFile, "Artist: %s\n", result.metadata.artist.c_str());
                    fprintf(outFile, "Album: %s\n", result.metadata.album.c_str());
                    fprintf(outFile, "Genre: %s\n", result.metadata.genre.c_str());
                    fprintf(outFile, "Year: %s\n", result.metadata.date.c_str());
                }
                fclose(outFile);
                yay("Metadata written to output file successfully.");
            }
        }

        if (strcmp(argv[j], "-grg") == 0 || strcmp(argv[j], "--getreplaygain") == 0) {
            std::vector<op::ReplayGainResult> results = op::GetReplayGainList(gb::inputFile);
            if (results.empty()) {
                err("No ReplayGain information found for input path.");
                return EXIT_FAILURE;
            }

            plog("ReplayGain information for input file(s): ");

            if (gb::outputToTerminal) {
                for (const auto &result : results) {
                    printf("----------------------\n");
                    printf("Track #%d: %s\n", result.info.trackNumber, result.info.title.c_str());
                    printf("Track Gain: %f\n", result.info.trackGain);
                    printf("Track Peak: %f\n", result.info.trackPeak);
                    printf("Album Gain: %f\n", result.info.albumGain);
                    printf("Album Peak: %f\n", result.info.albumPeak);
                }
            } else {
                FILE *outFile = fopen(gb::outputFile.string().c_str(), "w");
                if (!outFile) {
                    err("Failed to open output file for writing.");
                    return EXIT_FAILURE;
                }
                for (const auto &result : results) {
                    fprintf(outFile, "----------------------\n");
                    fprintf(outFile, "Track #%d: %s\n", result.info.trackNumber, result.info.title.c_str());
                    fprintf(outFile, "Track Gain: %f\n", result.info.trackGain);
                    fprintf(outFile, "Track Peak: %f\n", result.info.trackPeak);
                    fprintf(outFile, "Album Gain: %f\n", result.info.albumGain);
                    fprintf(outFile, "Album Peak: %f\n", result.info.albumPeak);
                }
                fclose(outFile);
                yay("ReplayGain information written to output file successfully.");
            }
        }

        if (strcmp(argv[j], "-sa") == 0 || strcmp(argv[j], "--spectralanalysis") == 0) {
            std::vector<op::SpectralAnalysisResult> results = op::SpectralAnalysisList(gb::inputFile);
            if (results.empty()) {
                err("No spectral analysis results found for input path.");
                return EXIT_FAILURE;
            }

            plog("Spectral Analysis Result for input file(s): ");

            if (gb::outputToTerminal) {
                for (const auto &result : results) {
                    printf("----------------------\n");
                    printf("Title: %s\n", result.title.c_str());
                    printf("Artist: %s\n", result.artist.c_str());
                    printf("Album: %s\n", result.album.c_str());
                    printf("Diagnosis: %s\n", result.diagnosis.c_str());
                    printf("Likely Lossy: %s\n", result.likelyLossy ? "Yes" : "No");
                    printf("Frequency Cutoff: %.2f Hz\n", result.frequencyCutoff);
                    printf("Noise Floor Elevation: %.2f dB\n", result.noiseFlorElevation);
                    printf("Banding Score: %.2f\n", result.bandingScore);
                }
            } else {
                FILE *outFile = fopen(gb::outputFile.string().c_str(), "w");
                if (!outFile) {
                    err("Failed to open output file for writing.");
                    return EXIT_FAILURE;
                }
                for (const auto &result : results) {
                    fprintf(outFile, "----------------------\n");
                    fprintf(outFile, "Title: %s\n", result.title.c_str());
                    fprintf(outFile, "Artist: %s\n", result.artist.c_str());
                    fprintf(outFile, "Album: %s\n", result.album.c_str());
                    fprintf(outFile, "Diagnosis: %s\n", result.diagnosis.c_str());
                    fprintf(outFile, "Likely Lossy: %s\n", result.likelyLossy ? "Yes" : "No");
                    fprintf(outFile, "Frequency Cutoff: %.2f Hz\n", result.frequencyCutoff);
                    fprintf(outFile, "Noise Floor Elevation: %.2f dB\n", result.noiseFlorElevation);
                    fprintf(outFile, "Banding Score: %.2f\n", result.bandingScore);
                }
                fclose(outFile);
                yay("Spectral analysis results written to output file successfully.");
            }
        }

        if (strcmp(argv[j], "-cvrt") == 0 || strcmp(argv[j], "--convert") == 0) {
            // converting stuff now..
            if (!fs::exists(gb::conversionOutputDirectory)) {
                err("Oh no! Did you provide an output **directory** via -po? :(");
                return EXIT_FAILURE;
            }

            if (fs::is_directory(gb::inputFile)) {
                yay("trying to convert directory >:P...");
                fs::directory_iterator dirIter(gb::inputFile);

                for (const auto &entry : dirIter) {
                    if (!entry.is_regular_file()) {

                        warn((std::string("Skipping non-regular file: ") + entry.path().string()).c_str());
                        continue;
                    }
                    if (!fc::IsValidAudioFile(entry.path())) {
                        warn((std::string("Skipping non-audio file: ") + entry.path().string()).c_str());
                        continue;
                    }

                    try {
                        op::ConvertToFileType(entry.path(), gb::conversionOutputDirectory, gb::outputFormat,
                                              gb::VBRQuality);
                    } catch (const std::exception &e) {
                        err((std::string("Failed to convert file: ") + entry.path().string() + " Error: " + e.what())
                                .c_str());
                    }
                }
            } else {
                yay("Trying to convert single file >:P...");
                try {
                    op::ConvertToFileType(gb::inputFile, gb::conversionOutputDirectory, gb::outputFormat,
                                          gb::VBRQuality);
                } catch (const std::exception &e) {
                    err((std::string("Failed to convert file: ") + gb::inputFile.string() + " Error: " + e.what())
                            .c_str());
                }
            }
        }

        if (strcmp(argv[j], "-t") == 0 || strcmp(argv[j], "--tag") == 0) {

            if (fs::is_directory(gb::inputFile)) {
                op::MassTagDirectory(gb::inputFile, gb::tag, gb::val);
                yay("Done! :D");
                return EXIT_SUCCESS;
            }

            tl::FileRef f(gb::inputFile.c_str());
            if (f.isNull() || !f.tag()) {
                err("Failed to open file for metadata commit");
                return EXIT_FAILURE;
            }

            tl::PropertyMap existingProps = f.file()->properties();
            op::StageMetaDataChanges(existingProps, gb::tag, gb::val);
            op::CommitMetaDataChanges(gb::inputFile, existingProps);
            yay("Tag applied successfully!");
        }

        if (strcmp(argv[j], "-atrg") == 0 || strcmp(argv[j], "--applytrackreplaygain") == 0) {
            if (fs::is_directory(gb::inputFile)) {
                std::vector<fs::path> tracks;
                for (const auto &entry : fs::directory_iterator(gb::inputFile)) {
                    if (!entry.is_regular_file()) {
                        continue;
                    }
                    if (!fc::IsValidAudioFile(entry.path())) {
                        continue;
                    }
                    tracks.push_back(entry.path());
                }

                if (tracks.empty()) {
                    warn("No valid audio files found in directory for track replaygain application.");
                } else {
                    const unsigned int hardwareThreads = std::thread::hardware_concurrency();
                    const std::size_t desiredWorkers =
                        std::max<std::size_t>(1, static_cast<std::size_t>(hardwareThreads / 2));
                    const std::size_t workerCount = std::min<std::size_t>(desiredWorkers, tracks.size());
                    std::vector<op::ReplayGainByTrack> trackResults(tracks.size(), op::ReplayGainByTrack{0.0f, 0.0f});
                    std::atomic<std::size_t> nextIndex{0};
                    std::vector<std::future<void>> workers;
                    workers.reserve(workerCount);

                    for (std::size_t worker = 0; worker < workerCount; ++worker) {
                        workers.push_back(std::async(std::launch::async, [&]() {
                            while (true) {
                                const std::size_t index = nextIndex.fetch_add(1);
                                if (index >= tracks.size()) {
                                    break;
                                }
                                trackResults[index] = op::CalculateReplayGainTrack(tracks[index]);
                            }
                        }));
                    }

                    for (auto &worker : workers) {
                        worker.get();
                    }

                    op::ReplayGainByAlbum albumGainInfo; // leave empty since we only want to apply track gain info
                    for (std::size_t i = 0; i < tracks.size(); ++i) {
                        op::ApplyReplayGain(tracks[i], trackResults[i], albumGainInfo);
                    }

                    yay(("Track ReplayGain applied successfully to " + std::to_string(tracks.size()) + " file(s)!")
                            .c_str());
                }
            } else {
                op::ReplayGainByTrack trackGainInfo = op::CalculateReplayGainTrack(gb::inputFile);
                op::ReplayGainByAlbum albumGainInfo; // leave empty since we only want to apply track gain info
                op::ApplyReplayGain(gb::inputFile, trackGainInfo, albumGainInfo);
                yay("ReplayGain applied successfully!");
            }
        }

        if (strcmp(argv[j], "-aag") == 0 || strcmp(argv[j], "--applyalbumgain") == 0) {
            op::CalculateReplayGainAlbum(gb::inputFile);
            yay("Album ReplayGain applied successfully!");
        }

        if (strcmp(argv[j], "-oia") == 0 || strcmp(argv[j], "--organizeintoalbums") == 0) {
            if (!fs::is_directory(gb::inputFile)) {
                err("Organize into albums requires a directory as input! Use -i <directory>");
                return EXIT_FAILURE;
            }
            op::OrganizeIntoAlbums(gb::inputFile, gb::conversionOutputDirectory);
        }

        if (strcmp(argv[j], "-oiaa") == 0 || strcmp(argv[j], "--organizeintoartistalbum") == 0) {
            if (!fs::is_directory(gb::inputFile)) {
                err("Organize into artist/album requires a directory as input! Use -i <directory>");
                return EXIT_FAILURE;
            }
            op::OrganizeIntoArtistAlbum(gb::inputFile, gb::conversionOutputDirectory);
        }

        if (strcmp(argv[j], "-sg") == 0 || strcmp(argv[j], "--generatespectrogram") == 0) {
            if (fs::is_directory(gb::inputFile)) {
                err("Generate spectrogram requires a single audio file as input! Use -i <file>");
                return EXIT_FAILURE;
            }
            if (gb::conversionOutputDirectory.empty()) {
                err("Generate spectrogram requires an output directory specified with -po/--pathout to save the generated image!");
                return EXIT_FAILURE;
            }
            fs::path outputImagePath = gb::conversionOutputDirectory / (gb::inputFile.stem().string() + "_spectrogram.png");
            op::GenerateSpectrogram(gb::inputFile, outputImagePath);
            std::error_code outputError;
            const bool outputExists = fs::exists(outputImagePath, outputError);
            const bool outputIsRegular = outputExists && fs::is_regular_file(outputImagePath, outputError);
            const auto outputSize = outputIsRegular ? fs::file_size(outputImagePath, outputError) : 0;

            if (!outputError && outputIsRegular && outputSize > 0) {
                yay(("Spectrogram generated successfully at: " + outputImagePath.string()).c_str());
            } else {
                err("Spectrogram generation completed but output image was not written correctly.");
                return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}
// hi there... 
