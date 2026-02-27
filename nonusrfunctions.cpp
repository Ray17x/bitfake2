#include <cstdlib>
#include <unordered_map>
#include "Utilites/operations.hpp"
namespace op = Operations;
#include "Utilites/globals.hpp"
namespace gb = globals;
#include "Utilites/consoleout.hpp"
#include <algorithm>
using namespace ConsoleOut;

namespace Operations
{
    // Non-user functions will be declared here
    // aka functions that can't be accessed by the users
    // during run time.

    AudioFormat StringToAudioFormat(const std::string& str)
    {
        std::string key = str;
        std::transform(key.begin(), key.end(), key.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // Function is now prepared to handle case-insensitive input and implementing
        // a dictionary for mapping string to enum values.

        static const std::unordered_map<std::string, AudioFormat> formatMap = {
            {"mp3", AudioFormat::MP3}, // fuck
            {"ogg", AudioFormat::OGG},
            {"m4a", AudioFormat::M4A},
            {"wav", AudioFormat::WAV},
            {"flac", AudioFormat::FLAC},
            {"aac", AudioFormat::AAC},
            {"wma", AudioFormat::WMA},
            {"opus", AudioFormat::OPUS},
            {"aiff", AudioFormat::AIFF},
            {"au", AudioFormat::AU},
            {"ra", AudioFormat::RA},
            {"ga3", AudioFormat::GA3},
            {"amr", AudioFormat::AMR},
            {"awb", AudioFormat::AWB},
            {"dss", AudioFormat::DSS},
            {"dvf", AudioFormat::DVF},
            {"m4b", AudioFormat::M4B},
            {"m4p", AudioFormat::M4P},
            {"mmf", AudioFormat::MMF},
            {"mpc", AudioFormat::MPC},
            {"msv", AudioFormat::MSV},
            {"nmf", AudioFormat::NMF},
            {"oga", AudioFormat::OGA},
            {"raw", AudioFormat::RAW},
            {"rf64", AudioFormat::RF64},
            {"sln", AudioFormat::SLN},
            {"tta", AudioFormat::TTA},
            {"voc", AudioFormat::VOC},
            {"vox", AudioFormat::VOX},
            {"wv",  AudioFormat::WV },
            {"webm",AudioFormat::WEBM },
            {"svx8",AudioFormat::SVX8 },
            {"cda",  AudioFormat::CDA }
        };
        
        auto it = formatMap.find(key);
        if (it != formatMap.end()) {
            return it->second;
        }
        err("Invalid audio format string");
        exit(EXIT_FAILURE);
    }
}
