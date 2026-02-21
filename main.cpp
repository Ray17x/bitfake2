#include <stdio.h>
#include <cstdlib>
#include "Utilites/consoleout.hpp"
using namespace ConsoleOut;
#include <filesystem>
#include <cstring>
namespace fs = std::filesystem;
#include "Utilites/filechecks.hpp"
namespace fc = FileChecks;

fs::path inputFile;
fs::path outputFile;


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
                    printf("Not done yet. \n");
                    return EXIT_SUCCESS;
                }

                if (strcmp(argv[i], "--input") == 0 || strcmp(argv[i], "-i") == 0) 
                {
                    if (i + 1 < argc) {
                        plog("Input file specified in flag: ");
                        yay(argv[i + 1]);
                        inputFile = argv[i + 1];
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
                        outputFile = argv[i + 1];
                        i++; // Skip the next argument since it's the output file
                    } else {
                        err("Output flag provided but no output file specified!");
                        return EXIT_FAILURE;
                    }
                }

                // This first pass is only for grabbing input and output files, so we can apply it to other commands later!
                break;
        }
    }

    // File checks before handing it off to the 2nd pass

    if (fc::IsValidAudioFile(inputFile) && fc::IsValidAudioFile(outputFile))
    {
        yay("Inputted files are valid audio files based on extension.");
        plog("Deeper checks will be performed in senstive operations.");
    } else if (!fc::IsValidAudioFile(inputFile))
    {
        if (!fc::ParentExists(inputFile))
        {
            err("Input file does not exist and parent directory does not exist either!");
            return EXIT_FAILURE;
        }
        else
        {
            err("Input file does not exist! Please check the path and try again.");
            return EXIT_FAILURE;
        }
    } else if (!fc::IsValidAudioFile(outputFile))
    {
        if (!fc::ParentExists(outputFile))
        {
            err("Output file does not exist and parent directory does not exist either!");
            return EXIT_FAILURE;
        }
        else
        {
            err("Output file does not exist! Please check the path and try again.");
            return EXIT_FAILURE;
        }
    }

    // 2nd Passin


    if (fc::IsTrueAudio(inputFile))
    {
        yay("Inputted files are valid audio files based on file signatures! This is a good sign :D");
    }

    return 0;
}