#include <stdio.h>
#include <cstdlib>
#include "Utilites/consoleout.hpp"
using namespace ConsoleOut;
#include <filesystem>
#include <cstring>
namespace fs = std::filesystem;
#include "Utilites/filechecks.hpp"
namespace fc = FileChecks;
#include "Utilites/operations.hpp"
namespace op = Operations;
#include "Utilites/globals.hpp"
namespace gb = globals;


int main(int argc, char* argv[])
{
    if (argc < 2) {
        err("No arguments provided! I can't do anything :(");
        plog("psst... use --help for more info!");
        return EXIT_FAILURE;
    }

    // First Pass grabs input file(s) and output file (if specified)
    for (int i = 1; i < argc; i++) {
        switch (argv[i][0]) 
        {     
            case '-':
                if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) 
                {
                    printf("Usage: ./bitfake2 [options]\n");
                    printf("Options:\n");
                    printf("  -h, --help              Show this help message\n");
                    printf("  -i, --input <file>      Input audio file path\n");
                    printf("  -o, --output <file>     Output file path (must be .txt)\n");
                    printf("  -f, --format <format>   Output format (default: txt)\n");
                    printf("  -gmd, --getmetadata     Get metadata of input file\n");
                    printf("  -grg, --getreplaygain   Get ReplayGain information of input file\n");
                    return EXIT_SUCCESS;
                }

                if (strcmp(argv[i], "--input") == 0 || strcmp(argv[i], "-i") == 0) 
                {
                    if (i + 1 < argc) {
                        plog("Input file specified in flag: ");
                        yay(argv[i + 1]);
                        gb::inputFile = argv[i + 1];
                        i++; // Skip the next argument since it's the input file
                    } else {
                        err("Input flag provided but no input file specified!");
                        return EXIT_FAILURE;
                    }
                }

                if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) 
                {
                    if (i + 1 < argc) {
                        plog("Output path specified in flag: ");
                        yay(argv[i + 1]);
                        gb::outputFile = argv[i + 1];
                        i++; // Skip the next argument since it's the output file
                    } else {
                        err("Output flag provided but no output file specified!");
                        return EXIT_FAILURE;
                    }
                }

                if (strcmp(argv[i], "--format") == 0 || strcmp(argv[i], "-f") == 0) 
                {
                    if (i + 1 < argc) {
                        plog("Output format specified in flag: ");
                        yay(argv[i + 1]);
                        i++; // Skip the next argument since it's the format
                    } else {
                        err("Format flag provided but no format specified!");
                        return EXIT_FAILURE;
                    }
                }

                // This first pass is only for grabbing input and output files, so we can apply it to other commands later!
                break;
        }
    }

    // File checks before handing it off to the 2nd pass

    if (gb::outputFile.empty())
    {
        plog("No output file specified. Output will be sent to terminal.");
        gb::outputToTerminal = true;
    }
    else if (gb::outputFile.extension() != ".txt")
    {
        warn("Output file must have .txt extension! Output will be sent to terminal instead.");
        gb::outputToTerminal = true;
    }
    else if (!fc::ParentExists(gb::outputFile))
    {
        err("Output file parent directory does not exist!");
        return EXIT_FAILURE;
    }
    else
    {
        plog("Output file will be created/written to: ");
        yay(gb::outputFile.c_str());
        gb::outputToTerminal = false;
    }
    

    if (!fs::exists(gb::inputFile))
    {
        err("Input file does not exist or is not a valid audio file!");
        err("Try again :(");
        return EXIT_FAILURE;
    }


    // 2nd pass

    for (int j = 1; j < argc; j++)
    {
         if (strcmp(argv[j], "-gmd") == 0 || strcmp(argv[j], "--getmetadata") == 0)
         {
            std::vector<op::AudioMetadataResult> results = op::GetMetaDataList(gb::inputFile);
            if (results.empty())
            {
                err("No metadata found for input path.");
                return EXIT_FAILURE;
            }

            plog("Metadata for input file(s): ");

            if (gb::outputToTerminal) {
                for (const auto& result : results)
                {
                    printf("----------------------\n");
                    printf("Track #%d: %s\n", result.metadata.trackNumber, result.metadata.title.c_str());
                    printf("Artist: %s\n", result.metadata.artist.c_str());
                    printf("Album: %s\n", result.metadata.album.c_str());
                    printf("Genre: %s\n", result.metadata.genre.c_str());
                    printf("Year: %s\n", result.metadata.year.c_str());
                }
            } else {
                FILE* outFile = fopen(gb::outputFile.string().c_str(), "w");
                if (!outFile) {
                    err("Failed to open output file for writing.");
                    return EXIT_FAILURE;
                }
                for (const auto& result : results)
                {
                    fprintf(outFile, "----------------------\n");
                    fprintf(outFile, "Track #%d: %s\n", result.metadata.trackNumber, result.metadata.title.c_str());
                    fprintf(outFile, "Artist: %s\n", result.metadata.artist.c_str());
                    fprintf(outFile, "Album: %s\n", result.metadata.album.c_str());
                    fprintf(outFile, "Genre: %s\n", result.metadata.genre.c_str());
                    fprintf(outFile, "Year: %s\n", result.metadata.year.c_str());
                }
                fclose(outFile);
                yay("Metadata written to output file successfully.");
            }
        }

        if (strcmp(argv[j], "-grg") == 0 || strcmp(argv[j], "--getreplaygain") == 0)
        {
            std::vector<op::ReplayGainResult> results = op::GetReplayGainList(gb::inputFile);
            if (results.empty())
            {
                err("No ReplayGain information found for input path.");
                return EXIT_FAILURE;
            }

            plog("ReplayGain information for input file(s): ");

            if (gb::outputToTerminal) {
                for (const auto& result : results)
                {
                    printf("----------------------\n");
                    printf("Track #%d: %s\n", result.info.trackNumber, result.info.title.c_str());
                    printf("Track Gain: %f\n", result.info.trackGain);
                    printf("Track Peak: %f\n", result.info.trackPeak);
                    printf("Album Gain: %f\n", result.info.albumGain);
                    printf("Album Peak: %f\n", result.info.albumPeak);
                }
            } else {
                FILE* outFile = fopen(gb::outputFile.string().c_str(), "w");
                if (!outFile) {
                    err("Failed to open output file for writing.");
                    return EXIT_FAILURE;
                }
                for (const auto& result : results)
                {
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

        if (strcmp(argv[j], "-sa") == 0 || strcmp(argv[j], "--spectralanalysis") == 0)
        {
            std::vector<op::SpectralAnalysisResult> results = op::SpectralAnalysisList(gb::inputFile);
            if (results.empty())
            {
                err("No spectral analysis results found for input path.");
                return EXIT_FAILURE;
            }

            plog("Spectral Analysis Result for input file(s): ");

            if (gb::outputToTerminal) {
                for (const auto& result : results)
                {
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
                FILE* outFile = fopen(gb::outputFile.string().c_str(), "w");
                if (!outFile) {
                    err("Failed to open output file for writing.");
                    return EXIT_FAILURE;
                }
                for (const auto& result : results)
                {
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
    }

    return 0;
}