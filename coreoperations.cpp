#include <cstdio>
#include "Utilities/consoleout.hpp"
using namespace ConsoleOut;
#include <filesystem>
namespace fs = std::filesystem;
#include "Utilities/filechecks.hpp"
namespace fc = FileChecks;
#include "Utilities/operations.hpp"
namespace op = Operations;
#include "Utilities/globals.hpp"
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
#include <string>
#include <cmath>
#include <limits>
#include <array>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <fftw3.h>
#include <complex>
// #include <musicbrainz5/Query.h>
// #include <musicbrainz5/Metadata.h>
// #include <musicbrainz5/Artist.h>
// #include <musicbrainz5/Release.h>
// #include <musicbrainz5/Track.h>
// #include <musicbrainz5/Recording.h>
// #include <musicbrainz5/ReleaseGroup.h>
// #include <musicbrainz5/NameCredit.h>
// #include <musicbrainz5/ArtistCredit.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
}

namespace {
std::string AvErrorToString(int errorCode) {
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errorCode, errorBuffer, sizeof(errorBuffer));
    return std::string(errorBuffer);
}

struct Rgb {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

double Clamp01(double value) {
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

Rgb InterpolateColor(const Rgb &a, const Rgb &b, double t) {
    const double clamped = Clamp01(t);
    return {
        static_cast<std::uint8_t>(a.r + (b.r - a.r) * clamped),
        static_cast<std::uint8_t>(a.g + (b.g - a.g) * clamped),
        static_cast<std::uint8_t>(a.b + (b.b - a.b) * clamped),
    };
}

Rgb SpekLikeColor(double normalizedDb) {
    const double t = Clamp01(normalizedDb);
    constexpr std::array<double, 7> positions = {0.0, 0.12, 0.28, 0.46, 0.66, 0.84, 1.0};
    constexpr std::array<Rgb, 7> colors = {
        Rgb{0, 0, 0},
        Rgb{15, 8, 48},
        Rgb{72, 16, 130},
        Rgb{170, 16, 110},
        Rgb{255, 36, 18},
        Rgb{255, 168, 0},
        Rgb{255, 255, 210},
    };

    for (std::size_t i = 1; i < positions.size(); ++i) {
        if (t <= positions[i]) {
            const double local = (t - positions[i - 1]) / (positions[i] - positions[i - 1]);
            return InterpolateColor(colors[i - 1], colors[i], local);
        }
    }

    return colors.back();
}

void PutPixel(std::vector<std::uint8_t> &image, int width, int height, int x, int y, const Rgb &color) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(y * width + x) * 3;
    image[index] = color.r;
    image[index + 1] = color.g;
    image[index + 2] = color.b;
}

void FillRect(std::vector<std::uint8_t> &image, int width, int height, int x, int y, int rectWidth, int rectHeight,
              const Rgb &color) {
    for (int row = 0; row < rectHeight; ++row) {
        for (int col = 0; col < rectWidth; ++col) {
            PutPixel(image, width, height, x + col, y + row, color);
        }
    }
}

void DrawHorizontalLine(std::vector<std::uint8_t> &image, int width, int height, int x1, int x2, int y,
                        const Rgb &color) {
    if (x2 < x1) {
        std::swap(x1, x2);
    }
    for (int x = x1; x <= x2; ++x) {
        PutPixel(image, width, height, x, y, color);
    }
}

void DrawVerticalLine(std::vector<std::uint8_t> &image, int width, int height, int x, int y1, int y2,
                      const Rgb &color) {
    if (y2 < y1) {
        std::swap(y1, y2);
    }
    for (int y = y1; y <= y2; ++y) {
        PutPixel(image, width, height, x, y, color);
    }
}

void DrawSevenSegmentChar(std::vector<std::uint8_t> &image, int width, int height, int x, int y, int scale, char ch,
                          const Rgb &color) {
    const int glyphWidth = 3 * scale;
    const int glyphHeight = 5 * scale;
    const int stroke = std::max(1, scale / 2);

    auto drawSegment = [&](int seg) {
        switch (seg) {
        case 0:
            FillRect(image, width, height, x + stroke, y, glyphWidth - 2 * stroke, stroke, color);
            break;
        case 1:
            FillRect(image, width, height, x + glyphWidth - stroke, y + stroke, stroke,
                     glyphHeight / 2 - stroke, color);
            break;
        case 2:
            FillRect(image, width, height, x + glyphWidth - stroke, y + glyphHeight / 2, stroke,
                     glyphHeight / 2 - stroke, color);
            break;
        case 3:
            FillRect(image, width, height, x + stroke, y + glyphHeight - stroke, glyphWidth - 2 * stroke, stroke,
                     color);
            break;
        case 4:
            FillRect(image, width, height, x, y + glyphHeight / 2, stroke, glyphHeight / 2 - stroke, color);
            break;
        case 5:
            FillRect(image, width, height, x, y + stroke, stroke, glyphHeight / 2 - stroke, color);
            break;
        case 6:
            FillRect(image, width, height, x + stroke, y + glyphHeight / 2 - stroke / 2, glyphWidth - 2 * stroke,
                     stroke, color);
            break;
        default:
            break;
        }
    };

    if (ch == ':') {
        FillRect(image, width, height, x + scale, y + glyphHeight / 3, scale, scale, color);
        FillRect(image, width, height, x + scale, y + (2 * glyphHeight) / 3, scale, scale, color);
        return;
    }

    if (ch == '.') {
        FillRect(image, width, height, x + glyphWidth - stroke, y + glyphHeight - stroke, stroke, stroke, color);
        return;
    }

    if (ch == '-') {
        drawSegment(6);
        return;
    }

    if (ch == ' ') {
        return;
    }

    std::array<bool, 7> segments = {false, false, false, false, false, false, false};
    switch (ch) {
    case '0':
        segments = {true, true, true, true, true, true, false};
        break;
    case '1':
        segments = {false, true, true, false, false, false, false};
        break;
    case '2':
        segments = {true, true, false, true, true, false, true};
        break;
    case '3':
        segments = {true, true, true, true, false, false, true};
        break;
    case '4':
        segments = {false, true, true, false, false, true, true};
        break;
    case '5':
        segments = {true, false, true, true, false, true, true};
        break;
    case '6':
        segments = {true, false, true, true, true, true, true};
        break;
    case '7':
        segments = {true, true, true, false, false, false, false};
        break;
    case '8':
        segments = {true, true, true, true, true, true, true};
        break;
    case '9':
        segments = {true, true, true, true, false, true, true};
        break;
    default:
        return;
    }

    for (int seg = 0; seg < 7; ++seg) {
        if (segments[static_cast<std::size_t>(seg)]) {
            drawSegment(seg);
        }
    }
}

int NumberTextWidth(const std::string &text, int scale) {
    if (text.empty()) {
        return 0;
    }
    return static_cast<int>(text.size()) * (3 * scale + scale) - scale;
}

void DrawNumberText(std::vector<std::uint8_t> &image, int width, int height, int x, int y, int scale,
                    const std::string &text, const Rgb &color) {
    int cursorX = x;
    for (char ch : text) {
        DrawSevenSegmentChar(image, width, height, cursorX, y, scale, ch, color);
        cursorX += (3 * scale + scale);
    }
}

std::array<std::uint8_t, 7> Glyph5x7(char ch) {
    switch (ch) {
    case 'A':
        return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'B':
        return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    case 'C':
        return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    case 'D':
        return {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    case 'E':
        return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    case 'F':
        return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    case 'G':
        return {0x0E, 0x11, 0x10, 0x13, 0x11, 0x11, 0x0F};
    case 'H':
        return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'I':
        return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
    case 'J':
        return {0x1F, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C};
    case 'K':
        return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    case 'L':
        return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    case 'M':
        return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    case 'N':
        return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    case 'O':
        return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'P':
        return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    case 'Q':
        return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
    case 'R':
        return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    case 'S':
        return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    case 'T':
        return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case 'U':
        return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'V':
        return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
    case 'W':
        return {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
    case 'X':
        return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
    case 'Y':
        return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
    case 'Z':
        return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
    case '(': 
        return {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
    case ')':
        return {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};
    case '/':
        return {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00};
    case '.':
        return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04};
    case '-':
        return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    case ':':
        return {0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00};
    case ' ':
        return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    default:
        return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    }
}

void DrawGlyph5x7Char(std::vector<std::uint8_t> &image, int width, int height, int x, int y, int scale, char ch,
                      const Rgb &color) {
    const std::array<std::uint8_t, 7> rows = Glyph5x7(ch);
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (rows[static_cast<std::size_t>(row)] & (1U << (4 - col))) {
                FillRect(image, width, height, x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

int LabelTextWidth(const std::string &text, int scale) {
    int width = 0;
    bool first = true;

    for (char raw : text) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
        const bool sevenSegment = std::isdigit(static_cast<unsigned char>(ch)) || ch == ':' || ch == '.' || ch == '-';
        const int glyphWidth = sevenSegment ? (3 * scale) : ((ch == ' ') ? (3 * scale) : (5 * scale));

        if (!first) {
            width += scale;
        }
        width += glyphWidth;
        first = false;
    }

    return width;
}

void DrawLabelText(std::vector<std::uint8_t> &image, int width, int height, int x, int y, int scale,
                   const std::string &text, const Rgb &color) {
    int cursorX = x;
    for (char raw : text) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
        const bool sevenSegment = std::isdigit(static_cast<unsigned char>(ch)) || ch == ':' || ch == '.' || ch == '-';

        if (sevenSegment) {
            DrawSevenSegmentChar(image, width, height, cursorX, y, scale, ch, color);
            cursorX += 3 * scale;
        } else {
            DrawGlyph5x7Char(image, width, height, cursorX, y, scale, ch, color);
            cursorX += (ch == ' ') ? (3 * scale) : (5 * scale);
        }

        cursorX += scale;
    }
}

std::string FormatMinutesSeconds(double seconds) {
    int totalSeconds = static_cast<int>(std::round(seconds));
    if (totalSeconds < 0) {
        totalSeconds = 0;
    }
    int minutes = totalSeconds / 60;
    int remainder = totalSeconds % 60;

    std::ostringstream stream;
    stream << minutes << ':' << std::setw(2) << std::setfill('0') << remainder;
    return stream.str();
}

bool WriteRgbPng(const fs::path &outputPath, int width, int height, const std::vector<std::uint8_t> &rgbData,
                 std::string &errorMessage) {
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!codec) {
        errorMessage = "Unable to find PNG encoder in libavcodec.";
        return false;
    }

    AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        errorMessage = "Unable to allocate PNG codec context.";
        return false;
    }

    codecContext->width = width;
    codecContext->height = height;
    codecContext->pix_fmt = AV_PIX_FMT_RGB24;
    codecContext->time_base = AVRational{1, 1};

    int status = avcodec_open2(codecContext, codec, nullptr);
    if (status < 0) {
        errorMessage = "Unable to open PNG encoder: " + AvErrorToString(status);
        avcodec_free_context(&codecContext);
        return false;
    }

    AVFrame *frame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    if (!frame || !packet) {
        errorMessage = "Unable to allocate frame/packet for PNG encoding.";
        if (frame)
            av_frame_free(&frame);
        if (packet)
            av_packet_free(&packet);
        avcodec_free_context(&codecContext);
        return false;
    }

    frame->format = codecContext->pix_fmt;
    frame->width = codecContext->width;
    frame->height = codecContext->height;
    status = av_frame_get_buffer(frame, 32);
    if (status < 0) {
        errorMessage = "Unable to allocate frame buffer: " + AvErrorToString(status);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codecContext);
        return false;
    }

    status = av_frame_make_writable(frame);
    if (status < 0) {
        errorMessage = "Frame is not writable: " + AvErrorToString(status);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codecContext);
        return false;
    }

    for (int y = 0; y < height; ++y) {
        std::memcpy(frame->data[0] + static_cast<std::size_t>(y) * frame->linesize[0],
                    rgbData.data() + static_cast<std::size_t>(y * width) * 3,
                    static_cast<std::size_t>(width) * 3);
    }

    status = avcodec_send_frame(codecContext, frame);
    if (status < 0) {
        errorMessage = "Failed to send PNG frame for encoding: " + AvErrorToString(status);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codecContext);
        return false;
    }

    status = avcodec_receive_packet(codecContext, packet);
    if (status < 0) {
        errorMessage = "Failed to receive PNG packet: " + AvErrorToString(status);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codecContext);
        return false;
    }

    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile.is_open()) {
        errorMessage = "Unable to open output file for writing PNG.";
        av_packet_unref(packet);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codecContext);
        return false;
    }

    outputFile.write(reinterpret_cast<const char *>(packet->data), packet->size);
    outputFile.close();

    av_packet_unref(packet);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codecContext);
    return true;
}

const AVCodec *FindPreferredEncoder(Operations::AudioFormat format) {
    switch (format) {
    case Operations::AudioFormat::MP3:
        if (const AVCodec *codec = avcodec_find_encoder_by_name("libmp3lame")) {
            return codec;
        }
        return avcodec_find_encoder(AV_CODEC_ID_MP3);
    case Operations::AudioFormat::AAC:
    case Operations::AudioFormat::M4A:
        return avcodec_find_encoder(AV_CODEC_ID_AAC);
    case Operations::AudioFormat::OPUS:
        if (const AVCodec *codec = avcodec_find_encoder_by_name("libopus")) {
            return codec;
        }
        return avcodec_find_encoder(AV_CODEC_ID_OPUS);
    default:
        return nullptr;
    }
}

const char *ContainerForFormat(Operations::AudioFormat format) {
    switch (format) {
    case Operations::AudioFormat::MP3:
        return "mp3";
    case Operations::AudioFormat::AAC:
        return "adts";
    case Operations::AudioFormat::M4A:
        return "ipod";
    case Operations::AudioFormat::OPUS:
        return "ogg";
    default:
        return nullptr;
    }
}

int PickSampleRate(const AVCodec *encoder, int preferred) {
    if (!encoder || !encoder->supported_samplerates) {
        return preferred > 0 ? preferred : 44100;
    }

    int best = encoder->supported_samplerates[0];
    int bestDistance = std::abs(best - preferred);
    for (const int *p = encoder->supported_samplerates; *p; ++p) {
        int distance = std::abs(*p - preferred);
        if (distance < bestDistance) {
            best = *p;
            bestDistance = distance;
        }
    }
    return best;
}

AVSampleFormat PickSampleFormat(const AVCodec *encoder) {
    if (!encoder || !encoder->sample_fmts) {
        return AV_SAMPLE_FMT_FLTP;
    }
    return encoder->sample_fmts[0];
}

bool ConvertWithLibAv(const fs::path &inputPath, const fs::path &outputFile, Operations::AudioFormat format,
                      int opusBitrateKbps) {
    AVFormatContext *inputFormat = nullptr;
    AVFormatContext *outputFormat = nullptr;
    AVCodecContext *decoderContext = nullptr;
    AVCodecContext *encoderContext = nullptr;
    SwrContext *resampler = nullptr;
    AVAudioFifo *audioFifo = nullptr;
    AVPacket *packet = nullptr;
    AVFrame *decodedFrame = nullptr;
    int audioStreamIndex = -1;
    int status = 0;
    int64_t nextPts = 0;

    auto cleanup = [&]() {
        if (packet)
            av_packet_free(&packet);
        if (decodedFrame)
            av_frame_free(&decodedFrame);
        if (audioFifo)
            av_audio_fifo_free(audioFifo);
        if (resampler)
            swr_free(&resampler);
        if (decoderContext)
            avcodec_free_context(&decoderContext);
        if (encoderContext)
            avcodec_free_context(&encoderContext);
        if (outputFormat) {
            if (!(outputFormat->oformat->flags & AVFMT_NOFILE) && outputFormat->pb) {
                avio_closep(&outputFormat->pb);
            }
            avformat_free_context(outputFormat);
        }
        if (inputFormat)
            avformat_close_input(&inputFormat);
    };

    status = avformat_open_input(&inputFormat, inputPath.string().c_str(), nullptr, nullptr);
    if (status < 0) {
        err(("libav: failed to open input: " + AvErrorToString(status)).c_str());
        cleanup();
        return false;
    }

    status = avformat_find_stream_info(inputFormat, nullptr);
    if (status < 0) {
        err(("libav: failed to read stream info: " + AvErrorToString(status)).c_str());
        cleanup();
        return false;
    }

    status = av_find_best_stream(inputFormat, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (status < 0) {
        err("libav: no audio stream found in input file.");
        cleanup();
        return false;
    }
    audioStreamIndex = status;

    AVStream *inputStream = inputFormat->streams[audioStreamIndex];
    const AVCodec *decoder = avcodec_find_decoder(inputStream->codecpar->codec_id);
    if (!decoder) {
        err("libav: no decoder available for input codec.");
        cleanup();
        return false;
    }

    decoderContext = avcodec_alloc_context3(decoder);
    if (!decoderContext) {
        err("libav: failed to allocate decoder context.");
        cleanup();
        return false;
    }

    status = avcodec_parameters_to_context(decoderContext, inputStream->codecpar);
    if (status < 0) {
        err(("libav: failed to copy decoder parameters: " + AvErrorToString(status)).c_str());
        cleanup();
        return false;
    }

    status = avcodec_open2(decoderContext, decoder, nullptr);
    if (status < 0) {
        err(("libav: failed to open decoder: " + AvErrorToString(status)).c_str());
        cleanup();
        return false;
    }

    const AVCodec *encoder = FindPreferredEncoder(format);
    const char *container = ContainerForFormat(format);
    if (!encoder || !container) {
        err("libav: unsupported output format for libav conversion.");
        cleanup();
        return false;
    }

    status = avformat_alloc_output_context2(&outputFormat, nullptr, container, outputFile.string().c_str());
    if (status < 0 || !outputFormat) {
        err(("libav: failed to allocate output format: " + AvErrorToString(status)).c_str());
        cleanup();
        return false;
    }

    AVStream *outputStream = avformat_new_stream(outputFormat, nullptr);
    if (!outputStream) {
        err("libav: failed to create output stream.");
        cleanup();
        return false;
    }

    encoderContext = avcodec_alloc_context3(encoder);
    if (!encoderContext) {
        err("libav: failed to allocate encoder context.");
        cleanup();
        return false;
    }

    if (decoderContext->ch_layout.nb_channels <= 0) {
        av_channel_layout_default(&decoderContext->ch_layout, 2);
    }

    encoderContext->sample_rate =
        PickSampleRate(encoder, decoderContext->sample_rate > 0 ? decoderContext->sample_rate : 44100);
    status = av_channel_layout_copy(&encoderContext->ch_layout, &decoderContext->ch_layout);
    if (status < 0) {
        err(("libav: failed to copy channel layout: " + AvErrorToString(status)).c_str());
        cleanup();
        return false;
    }
    encoderContext->sample_fmt = PickSampleFormat(encoder);
    if (format == Operations::AudioFormat::OPUS) {
        if (opusBitrateKbps > 0) {
            encoderContext->bit_rate = opusBitrateKbps * 1000;
        } else {
            encoderContext->bit_rate = 0;
        }
        if (encoderContext->priv_data) {
            av_opt_set(encoderContext->priv_data, "vbr", "on", 0);
        }
    } else {
        encoderContext->bit_rate = 192000;
    }
    encoderContext->time_base = AVRational{1, encoderContext->sample_rate};

    if (outputFormat->oformat->flags & AVFMT_GLOBALHEADER) {
        encoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    status = avcodec_open2(encoderContext, encoder, nullptr);
    if (status < 0) {
        err(("libav: failed to open encoder: " + AvErrorToString(status)).c_str());
        cleanup();
        return false;
    }

    status = avcodec_parameters_from_context(outputStream->codecpar, encoderContext);
    if (status < 0) {
        err(("libav: failed to set output stream parameters: " + AvErrorToString(status)).c_str());
        cleanup();
        return false;
    }
    outputStream->time_base = encoderContext->time_base;

    if (!(outputFormat->oformat->flags & AVFMT_NOFILE)) {
        status = avio_open(&outputFormat->pb, outputFile.string().c_str(), AVIO_FLAG_WRITE);
        if (status < 0) {
            err(("libav: failed to open output file: " + AvErrorToString(status)).c_str());
            cleanup();
            return false;
        }
    }

    status = avformat_write_header(outputFormat, nullptr);
    if (status < 0) {
        err(("libav: failed to write output header: " + AvErrorToString(status)).c_str());
        cleanup();
        return false;
    }

    status = swr_alloc_set_opts2(&resampler, &encoderContext->ch_layout, encoderContext->sample_fmt,
                                 encoderContext->sample_rate, &decoderContext->ch_layout, decoderContext->sample_fmt,
                                 decoderContext->sample_rate, 0, nullptr);
    if (status < 0 || !resampler || swr_init(resampler) < 0) {
        err("libav: failed to initialize audio resampler.");
        cleanup();
        return false;
    }

    packet = av_packet_alloc();
    decodedFrame = av_frame_alloc();
    if (!packet || !decodedFrame) {
        err("libav: failed to allocate frame/packet structures.");
        cleanup();
        return false;
    }

    audioFifo = av_audio_fifo_alloc(encoderContext->sample_fmt, encoderContext->ch_layout.nb_channels, 1);
    if (!audioFifo) {
        err("libav: failed to allocate audio FIFO.");
        cleanup();
        return false;
    }

    auto encodeConvertedFrame = [&](AVFrame *frame) -> bool {
        int sendStatus = avcodec_send_frame(encoderContext, frame);
        if (sendStatus < 0) {
            err(("libav: failed sending frame to encoder: " + AvErrorToString(sendStatus)).c_str());
            return false;
        }

        while (true) {
            AVPacket *outPacket = av_packet_alloc();
            if (!outPacket) {
                err("libav: failed to allocate output packet.");
                return false;
            }

            int recvStatus = avcodec_receive_packet(encoderContext, outPacket);
            if (recvStatus == AVERROR(EAGAIN) || recvStatus == AVERROR_EOF) {
                av_packet_free(&outPacket);
                break;
            }
            if (recvStatus < 0) {
                err(("libav: failed receiving packet from encoder: " + AvErrorToString(recvStatus)).c_str());
                av_packet_free(&outPacket);
                return false;
            }

            av_packet_rescale_ts(outPacket, encoderContext->time_base, outputStream->time_base);
            outPacket->stream_index = outputStream->index;

            int writeStatus = av_interleaved_write_frame(outputFormat, outPacket);
            av_packet_free(&outPacket);
            if (writeStatus < 0) {
                err(("libav: failed writing encoded packet: " + AvErrorToString(writeStatus)).c_str());
                return false;
            }
        }
        return true;
    };

    auto encodeFromFifo = [&](int requestedSamples, bool padWithSilence) -> bool {
        if (requestedSamples <= 0) {
            return true;
        }

        AVFrame *frame = av_frame_alloc();
        if (!frame) {
            err("libav: failed to allocate encoder input frame.");
            return false;
        }

        frame->format = encoderContext->sample_fmt;
        frame->sample_rate = encoderContext->sample_rate;
        frame->nb_samples = requestedSamples;
        if (av_channel_layout_copy(&frame->ch_layout, &encoderContext->ch_layout) < 0) {
            err("libav: failed to copy encoder channel layout to frame.");
            av_frame_free(&frame);
            return false;
        }

        int frameStatus = av_frame_get_buffer(frame, 0);
        if (frameStatus < 0) {
            err(("libav: failed to allocate encoder frame buffer: " + AvErrorToString(frameStatus)).c_str());
            av_frame_free(&frame);
            return false;
        }

        int availableSamples = av_audio_fifo_size(audioFifo);
        int toRead = std::min(requestedSamples, availableSamples);
        if (toRead > 0) {
            int readSamples = av_audio_fifo_read(audioFifo, reinterpret_cast<void **>(frame->data), toRead);
            if (readSamples < toRead) {
                err("libav: failed reading expected samples from FIFO.");
                av_frame_free(&frame);
                return false;
            }
        }

        if (toRead < requestedSamples) {
            if (!padWithSilence) {
                av_frame_free(&frame);
                return true;
            }
            av_samples_set_silence(frame->data, toRead, requestedSamples - toRead,
                                   encoderContext->ch_layout.nb_channels, encoderContext->sample_fmt);
        }

        frame->pts = nextPts;
        nextPts += requestedSamples;

        bool encoded = encodeConvertedFrame(frame);
        av_frame_free(&frame);
        return encoded;
    };

    auto pushConvertedToFifo = [&](const AVFrame *inFrame) -> bool {
        const int inputSamples = (inFrame != nullptr) ? inFrame->nb_samples : 0;
        int maxOutSamples = swr_get_out_samples(resampler, inputSamples);

        if (maxOutSamples < 0) {
            err("libav: failed to compute SWR output size.");
            return false;
        }
        
        if (maxOutSamples == 0) {
            return true;
        }

        uint8_t **convertedData = nullptr;
        int convertedLineSize = 0;
        // oh my fuck what is this formatting
        int allocStatus = av_samples_alloc_array_and_samples(&convertedData, &convertedLineSize,
                                                             encoderContext->ch_layout.nb_channels, maxOutSamples,
                                                             encoderContext->sample_fmt, 0);
        if (allocStatus < 0) {
            err(("libav: failed to allocate conversion buffers: " + AvErrorToString(allocStatus)).c_str());
            return false;
        }

        int convertedSamples = swr_convert(
            resampler, convertedData, maxOutSamples,
            inFrame != nullptr ? const_cast<const uint8_t **>(inFrame->extended_data) : nullptr, inputSamples);

        if (convertedSamples < 0) {
            av_freep(&convertedData[0]);
            av_freep(&convertedData);
            err("libav: sample format conversion failed.");
            return false;
        }

        if (av_audio_fifo_space(audioFifo) < convertedSamples) {
            if (av_audio_fifo_realloc(audioFifo, av_audio_fifo_size(audioFifo) + convertedSamples) < 0) {
                av_freep(&convertedData[0]);
                av_freep(&convertedData);
                err("libav: failed to grow audio FIFO.");
                return false;
            }
        }

        int wrote = av_audio_fifo_write(audioFifo, reinterpret_cast<void **>(convertedData), convertedSamples);
        av_freep(&convertedData[0]);
        av_freep(&convertedData);
        if (wrote < convertedSamples) {
            err("libav: failed to write all converted samples to FIFO.");
            return false;
        }

        return true;
    };

    while (av_read_frame(inputFormat, packet) >= 0) {
        if (packet->stream_index != audioStreamIndex) {
            av_packet_unref(packet);
            continue;
        }

        status = avcodec_send_packet(decoderContext, packet);
        av_packet_unref(packet);
        if (status < 0) {
            err(("libav: failed sending packet to decoder: " + AvErrorToString(status)).c_str());
            cleanup();
            return false;
        }

        while (true) {
            status = avcodec_receive_frame(decoderContext, decodedFrame);
            if (status == AVERROR(EAGAIN) || status == AVERROR_EOF) {
                break;
            }
            if (status < 0) {
                err(("libav: failed receiving frame from decoder: " + AvErrorToString(status)).c_str());
                cleanup();
                return false;
            }

            if (!pushConvertedToFifo(decodedFrame)) {
                cleanup();
                return false;
            }

            av_frame_unref(decodedFrame);

            const int encoderFrameSize =
                encoderContext->frame_size > 0 ? encoderContext->frame_size : av_audio_fifo_size(audioFifo);

            while (encoderFrameSize > 0 && av_audio_fifo_size(audioFifo) >= encoderFrameSize) {
                if (!encodeFromFifo(encoderFrameSize, false)) {
                    cleanup();
                    return false;
                }
            }

            if (encoderContext->frame_size == 0 && av_audio_fifo_size(audioFifo) > 0) {
                if (!encodeFromFifo(av_audio_fifo_size(audioFifo), false)) {
                    cleanup();
                    return false;
                }
            }
        }
    }

    avcodec_send_packet(decoderContext, nullptr);
    while (avcodec_receive_frame(decoderContext, decodedFrame) >= 0) {
        if (!pushConvertedToFifo(decodedFrame)) {
            cleanup();
            return false;
        }
        av_frame_unref(decodedFrame);

        const int encoderFrameSize =
            encoderContext->frame_size > 0 ? encoderContext->frame_size : av_audio_fifo_size(audioFifo);

        while (encoderFrameSize > 0 && av_audio_fifo_size(audioFifo) >= encoderFrameSize) {
            if (!encodeFromFifo(encoderFrameSize, false)) {
                cleanup();
                return false;
            }
        }

        if (encoderContext->frame_size == 0 && av_audio_fifo_size(audioFifo) > 0) {
            if (!encodeFromFifo(av_audio_fifo_size(audioFifo), false)) {
                cleanup();
                return false;
            }
        }
    }

    while (true) {
        int delayedSamples = swr_get_delay(resampler, decoderContext->sample_rate);
        if (delayedSamples <= 0) {
            break;
        }

        int fifoBeforeDrain = av_audio_fifo_size(audioFifo);
        if (!pushConvertedToFifo(nullptr)) {
            cleanup();
            return false;
        }
        int fifoAfterDrain = av_audio_fifo_size(audioFifo);
        if (fifoAfterDrain <= fifoBeforeDrain) {
            break;
        }

        if (encoderContext->frame_size > 0) {
            while (av_audio_fifo_size(audioFifo) >= encoderContext->frame_size) {
                if (!encodeFromFifo(encoderContext->frame_size, false)) {
                    cleanup();
                    return false;
                }
            }
        } else if (av_audio_fifo_size(audioFifo) > 0) {
            if (!encodeFromFifo(av_audio_fifo_size(audioFifo), false)) {
                cleanup();
                return false;
            }
        } else {
            break;
        }
    }

    while (av_audio_fifo_size(audioFifo) > 0) {
        int drainSize = encoderContext->frame_size > 0 ? encoderContext->frame_size : av_audio_fifo_size(audioFifo);
        if (!encodeFromFifo(drainSize, true)) {
            cleanup();
            return false;
        }
    }

    if (!encodeConvertedFrame(nullptr)) {
        cleanup();
        return false;
    }

    status = av_write_trailer(outputFormat);
    if (status < 0) {
        err(("libav: failed writing output trailer: " + AvErrorToString(status)).c_str());
        cleanup();
        return false;
    }

    cleanup();
    return true;
}

// struct MBAudioInfo {
//     std::string MUSICBRAINZ_ALBUMARTISTID;
//     std::string MUSICBRAINZ_ALBUMID;
//     std::string MUSICBRAINZ_ALBUMSTATUS;
//     std::string MUSICBRAINZ_ARTISTID;
//     std::string MUSICBRAINZ_RELEASEGROUPID;
//     std::string MUSICBRAINZ_RELEASETRACKID;
//     std::string MUSICBRAINZ_TRACKID;
//     std::string MUSICBRAINZ_WORKID;
// };

// duh- dah -duh -dah dah- disappointed in this fuck ass API
// (Disappointed - Death Grips)
// MBAudioInfo GetMBIDFromDB(const fs::path &inputPath);
// This function has not been implemented because the API is hard to understand and there is lack of documentation.
} // namespace

namespace Operations {
// Core operations

/*
    This file will define functions for core operations of the program, such as conversion, replaygain calculation,
    mass tagging for a directory, and other nitty gritty functions that will be a hassle to program. Will try to keep
   things as clean and organized as possible, but no promises >:D
*/

bool ConvertToFileType(const fs::path &inputPath, const fs::path &outputPath, AudioFormat format,
                       VBRQualities quality) {
    if (!fs::exists(inputPath)) {
        err("Input path does not exist.");
        return false;
    }

    if (fs::is_directory(inputPath)) {
        if (!fs::exists(outputPath) || !fs::is_directory(outputPath)) {
            err("Output path does not exist or is not a directory!");
            return false;
        }

        std::size_t convertedCount = 0;
        std::size_t skippedCount = 0;

        for (const auto &entry : fs::directory_iterator(inputPath)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            if (!fc::IsValidAudioFile(entry.path())) {
                ++skippedCount;
                continue;
            }

            if (!ConvertToFileType(entry.path(), outputPath, format, quality)) {
                err("Failed to convert file.");
                return false;
            }
            ++convertedCount;
        }

        if (convertedCount == 0) {
            warn("No valid audio files found in input directory for conversion.");
            return false;
        }

        yay(("Directory conversion completed. Converted " + std::to_string(convertedCount) + " file(s).").c_str());
        if (skippedCount > 0) {
            warn(("Skipped " + std::to_string(skippedCount) + " non-audio file(s).").c_str());
        }
        return true;
    }

    if (!fs::is_regular_file(inputPath)) {
        err("Input path is not a regular file.");
        return false;
    }

    if (!fc::IsSpecificAudioFormat(inputPath, op::AudioFormat::FLAC) &&
        !fc::IsSpecificAudioFormat(inputPath, op::AudioFormat::WAV)) {
        warn("Input file is not a lossless file. Conversion may result in quality loss. :(");
    }

    (void)quality;

    fs::path outputFile = outputPath;
    if (fs::is_directory(outputPath)) {
        outputFile = outputPath / (inputPath.stem().string() + OutputExtensionForFormat(format));
    }

    const bool useLibsndfilePath =
        (format == AudioFormat::WAV || format == AudioFormat::FLAC || format == AudioFormat::OGG);

    if (useLibsndfilePath) {
        int outputFormat = 0;
        switch (format) {
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
            err("Unsupported libsndfile output format.");
            return false;
        }

        SF_INFO inInfo{};
        SNDFILE *inFile = sf_open(inputPath.string().c_str(), SFM_READ, &inInfo);
        if (!inFile) {
            err("Failed to open input audio file for conversion.");
            return false;
        }

        SF_INFO outInfo{};
        outInfo.samplerate = inInfo.samplerate;
        outInfo.channels = inInfo.channels;
        outInfo.format = outputFormat;

        if (!sf_format_check(&outInfo)) {
            err("Output format is not supported by the current libsndfile build.");
            sf_close(inFile);
            return false;
        }

        SNDFILE *outFile = sf_open(outputFile.string().c_str(), SFM_WRITE, &outInfo);
        if (!outFile) {
            err("Failed to create output audio file for conversion.");
            sf_close(inFile);
            return false;
        }

        constexpr sf_count_t frameBlockSize = 4096;
        std::vector<float> pcmBuffer(static_cast<std::size_t>(frameBlockSize * inInfo.channels));

        sf_count_t framesRead = 0;
        while ((framesRead = sf_readf_float(inFile, pcmBuffer.data(), frameBlockSize)) > 0) {
            sf_count_t framesWritten = sf_writef_float(outFile, pcmBuffer.data(), framesRead);
            if (framesWritten != framesRead) {
                err("Audio conversion write failed before all frames were written.");
                sf_close(inFile);
                sf_close(outFile);
                return false;
            }
        }

        sf_close(inFile);
        sf_close(outFile);
    } else {
        if (!ConvertWithLibAv(inputPath, outputFile, format, gb::opusBitrateKbps)) {
            err("Library-based conversion failed for requested output format.");
            return false;
        }
    }

    if (InputHasAttachedCover(inputPath)) {
        if (!FormatSupportsAttachedCover(format)) {
            warn("Input has cover art, but selected output format does not support attached cover art copy yet.");
        } else if (!CopyAttachedCover(inputPath, outputFile, format)) {
            warn("Input has cover art, but cover art copy to output failed.");
        }
    }

    TagLib::FileRef inTagRef(inputPath.string().c_str());
    TagLib::FileRef outTagRef(outputFile.string().c_str());
    if (!inTagRef.isNull() && inTagRef.tag() && !outTagRef.isNull() && outTagRef.tag()) {
        outTagRef.tag()->setTitle(inTagRef.tag()->title());
        outTagRef.tag()->setArtist(inTagRef.tag()->artist());
        outTagRef.tag()->setAlbum(inTagRef.tag()->album());
        outTagRef.tag()->setComment(inTagRef.tag()->comment());
        outTagRef.tag()->setGenre(inTagRef.tag()->genre());
        outTagRef.tag()->setYear(inTagRef.tag()->year());
        outTagRef.tag()->setTrack(inTagRef.tag()->track());
        outTagRef.file()->save();
    }

    yay("Conversion completed successfully!");
    plog("Output file:");
    yay(outputFile.c_str());
    return true;
}

void ApplyReplayGain(const fs::path &path, ReplayGainByTrack trackGainInfo, ReplayGainByAlbum albumGainInfo) {
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
        printf("--- %s - %s\nAlbum Gain / Peak: %.2f dB / %.6f\n\n", trackTitle.c_str(), trackArtist.c_str(),
               albumGainInfo.albumGain, albumGainInfo.albumPeak);
        StageMetaDataChanges(properties, "REPLAYGAIN_ALBUM_GAIN", std::to_string(albumGainInfo.albumGain) + " dB");
        StageMetaDataChanges(properties, "REPLAYGAIN_ALBUM_PEAK", std::to_string(albumGainInfo.albumPeak));
        CommitMetaDataChanges(path, properties);
        return;
    }
    if (albumInfoEmpty) {
        const std::string trackTitle = existingMetadata.title.empty() ? "<unknown title>" : existingMetadata.title;
        const std::string trackArtist = existingMetadata.artist.empty() ? "<unknown artist>" : existingMetadata.artist;
        printf("--- %s - %s\nTrack Gain / Peak: %.2f dB / %.6f\n\n", trackTitle.c_str(), trackArtist.c_str(),
               trackGainInfo.trackGain, trackGainInfo.trackPeak);
        StageMetaDataChanges(properties, "REPLAYGAIN_TRACK_GAIN", std::to_string(trackGainInfo.trackGain) + " dB");
        StageMetaDataChanges(properties, "REPLAYGAIN_TRACK_PEAK", std::to_string(trackGainInfo.trackPeak));
        CommitMetaDataChanges(path, properties);
        return;
    }

    // Fall back to applying both if both are provided
    const std::string trackTitle = existingMetadata.title.empty() ? "<unknown title>" : existingMetadata.title;
    const std::string trackArtist = existingMetadata.artist.empty() ? "<unknown artist>" : existingMetadata.artist;
    printf("--- %s - %s\nTrack Gain / Peak: %.2f dB / %.6f\nAlbum Gain / Peak: %.2f dB / %.6f\n\n", trackTitle.c_str(),
           trackArtist.c_str(), trackGainInfo.trackGain, trackGainInfo.trackPeak, albumGainInfo.albumGain,
           albumGainInfo.albumPeak);
    StageMetaDataChanges(properties, "REPLAYGAIN_TRACK_GAIN", std::to_string(trackGainInfo.trackGain) + " dB");
    StageMetaDataChanges(properties, "REPLAYGAIN_TRACK_PEAK", std::to_string(trackGainInfo.trackPeak));
    StageMetaDataChanges(properties, "REPLAYGAIN_ALBUM_GAIN", std::to_string(albumGainInfo.albumGain) + " dB");
    StageMetaDataChanges(properties, "REPLAYGAIN_ALBUM_PEAK", std::to_string(albumGainInfo.albumPeak));
    CommitMetaDataChanges(path, properties);
}

// Replaygain is calculated from EBU R128 integrated loudness,
// with a ReplayGain-style -18 dB reference target.
ReplayGainByTrack CalculateReplayGainTrack(const fs::path &path) {
    ReplayGainByTrack result{0.0f, 0.0f};

    TagLib::FileRef f(path.c_str());
    if (f.isNull() || !f.audioProperties()) {
        err("Failed to read audio file for replaygain calculation. :(");
        return result;
    }

    constexpr double targetLufs = -18.0;

    // Open audio file with libsndfile
    SF_INFO sfinfo{};
    SNDFILE *sndfile = sf_open(path.string().c_str(), SFM_READ, &sfinfo);
    if (!sndfile) {
        err("Failed to open audio file with libsndfile for replaygain calculation. :(");
        return result;
    }

    ebur128_state *st = ebur128_init(sfinfo.channels, sfinfo.samplerate, EBUR128_MODE_I | EBUR128_MODE_TRUE_PEAK);
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

void CalculateReplayGainAlbum(const fs::path &path) {
    if (!fs::exists(path) || !fs::is_directory(path)) {
        err("Album replaygain calculation failed: input path does not exist or is not a directory.");
        warn("Album replaygain calculation requires a directory input containing audio files from the same album.");
        return;
    }

    // 1) Group valid audio files by album tag.
    std::unordered_map<std::string, std::vector<fs::path>> albumMap;
    for (const auto &entry : fs::directory_iterator(path)) {
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
    for (const auto &[album, paths] : albumMap) {
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

    for (auto &worker : workers) {
        worker.get();
    }

    std::unordered_map<std::string, ReplayGainByTrack> trackReplayGainByPath;
    trackReplayGainByPath.reserve(allTracks.size());
    for (std::size_t i = 0; i < allTracks.size(); ++i) {
        trackReplayGainByPath[allTracks[i].string()] = trackResults[i];
    }

    std::unordered_map<std::string, ReplayGainByAlbum> albumReplayGain;

    for (const auto &[album, paths] : albumMap) {
        double totalGain = 0.0;
        double maxPeak = 0.0;
        int trackCount = 0;

        for (const auto &trackPath : paths) {
            ReplayGainByTrack trackGainInfo = trackReplayGainByPath[trackPath.string()];
            totalGain += trackGainInfo.trackGain;
            maxPeak = std::max(maxPeak, static_cast<double>(trackGainInfo.trackPeak));
            trackCount++;
        }

        if (trackCount > 0) {
            albumReplayGain[album] =
                ReplayGainByAlbum{static_cast<float>(totalGain / trackCount), static_cast<float>(maxPeak)};

            const std::string albumName = album.empty() ? "<unknown album>" : album;
            plog(("Album replaygain calculated: " + albumName + " | tracks=" + std::to_string(trackCount) +
                  " | gain=" + std::to_string(albumReplayGain[album].albumGain) + " dB" +
                  " | peak=" + std::to_string(albumReplayGain[album].albumPeak))
                     .c_str());
        }
    }

    // 3) Apply previously calculated album replaygain values in a separate pass.
    for (const auto &[album, paths] : albumMap) {
        auto it = albumReplayGain.find(album);
        if (it == albumReplayGain.end()) {
            continue;
        }

        for (const auto &trackPath : paths) {
            ApplyReplayGain(trackPath, {}, it->second); // only apply album gain info
        }
    }
}

static std::string sanitize_dir_name(const std::string &name) {
    std::string safe;
    safe.reserve(name.size());
    for (size_t i = 0; i < name.size(); i++) {
        char c = name[i];
        if (c == '/' || c == '\\' || c == '\0') {
            safe.push_back('_');
        } else {
            safe.push_back(c);
        }
    }
    while (!safe.empty() && (safe.back() == '.' || safe.back() == ' ')) {
        safe.pop_back();
    }
    if (safe.empty()) {
        safe = "Unsorted";
    }
    return safe;
}

void OrganizeIntoAlbums(const fs::path &inputDir, const fs::path &outputDir) {
    if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
        err("Input path does not exist or is not a directory.");
        return;
    }

    fs::path destRoot = outputDir.empty() ? inputDir : outputDir;
    if (!fs::exists(destRoot) || !fs::is_directory(destRoot)) {
        err("Output directory does not exist or is not a directory.");
        return;
    }

    std::unordered_map<std::string, std::vector<fs::path>> albumMap;
    std::size_t totalFiles = 0;

    // group files by album tag
    for (fs::directory_iterator i(inputDir); i != fs::directory_iterator(); i++) {
        fs::directory_entry entry = *i;
        if (!entry.is_regular_file()) {
            continue;
        }
        if (!fc::IsValidAudioFile(entry.path())) {
            continue;
        }

        std::string album = "Unsorted";
        TagLib::FileRef f(entry.path().string().c_str());
        if (!f.isNull() && f.tag() && !f.tag()->album().isEmpty()) {
            album = f.tag()->album().to8Bit(true);
        }

        albumMap[album].push_back(entry.path());
        totalFiles++;
    }

    if (totalFiles == 0) {
        warn("No valid audio files found in directory to organize.");
        return;
    }

    plog(("Found " + std::to_string(totalFiles) + " audio file(s) across " + std::to_string(albumMap.size()) +
          " album(s).")
             .c_str());

    // sanitize to prevent bad stuff
    auto sanitizeDirName = sanitize_dir_name;

    int movedCount = 0;
    int failedCount = 0;

    // create album dirs and move files
    for (auto i = albumMap.begin(); i != albumMap.end(); i++) {
        const std::string &album = i->first;
        const std::vector<fs::path> &paths = i->second;

        std::string safeName = sanitizeDirName(album);
        fs::path albumDir = destRoot / safeName;

        std::error_code ec;
        if (!fs::exists(albumDir)) {
            fs::create_directories(albumDir, ec);
            if (ec) {
                err(("Failed to create album directory: " + albumDir.string() + " (" + ec.message() + ")").c_str());
                failedCount += paths.size();
                continue;
            }
            plog(("Created album directory: " + safeName).c_str());
        }

        // move files into album dir
        for (size_t i = 0; i < paths.size(); ++i) {
            const fs::path &filePath = paths[i];
            fs::path dest = albumDir / filePath.filename();

            if (fs::equivalent(filePath, dest, ec)) {
                movedCount++;
                continue;
            }
            if (fs::exists(dest)) {
                warn(("Skipping (already exists in album dir): " + filePath.filename().string()).c_str());
                failedCount++;
                continue;
            }

            fs::rename(filePath, dest, ec);
            if (ec) {
                // fall back
                fs::copy_file(filePath, dest, fs::copy_options::none, ec);
                if (ec) {
                    err(("Failed to move file: " + filePath.filename().string() + " (" + ec.message() + ")").c_str());
                    ++failedCount;
                    continue;
                }
                fs::remove(filePath, ec);
                if (ec) {
                    warn(("File copied but original could not be removed: " + filePath.filename().string()).c_str());
                }
            }

            movedCount++;
        }
    }

    yay(("Organized " + std::to_string(movedCount) + " file(s) into " + std::to_string(albumMap.size()) +
         " album folder(s).")
            .c_str());
    if (failedCount > 0) {
        warn(("Failed to move " + std::to_string(failedCount) + " file(s).").c_str());
    }
}

void OrganizeIntoArtistAlbum(const fs::path &inputDir, const fs::path &outputDir) {
    if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
        err("Input path does not exist or is not a directory.");
        return;
    }

    fs::path destRoot = outputDir.empty() ? inputDir : outputDir;
    if (!fs::exists(destRoot) || !fs::is_directory(destRoot)) {
        err("Output directory does not exist or is not a directory.");
        return;
    }

    struct TrackInfo {
        fs::path path;
        std::string artist;
        std::string album;
    };

    std::vector<TrackInfo> tracks;

    for (fs::directory_iterator i(inputDir); i != fs::directory_iterator(); i++) {
        fs::directory_entry entry = *i;
        if (!entry.is_regular_file()) {
            continue;
        }
        if (!fc::IsValidAudioFile(entry.path())) {
            continue;
        }

        std::string artist = "Unsorted";
        std::string album = "Unsorted";
        TagLib::FileRef f(entry.path().string().c_str());
        // if the file is valid and has tags, try to read artist and album, but fall back to defaults if not present
        if (!f.isNull() && f.tag()) {
            if (!f.tag()->artist().isEmpty()) {
                artist = f.tag()->artist().to8Bit(true);
            }
            if (!f.tag()->album().isEmpty()) {
                album = f.tag()->album().to8Bit(true);
            }
        }

        tracks.push_back({entry.path(), artist, album});
    }

    if (tracks.empty()) {
        warn("No valid audio files found in directory to organize.");
        return;
    }

    plog(("Found " + std::to_string(tracks.size()) + " audio file(s).").c_str());

    int movedCount = 0;
    int failedCount = 0;

    // move files into artist/album dir
    for (size_t i = 0; i < tracks.size(); ++i) {
        const TrackInfo &track = tracks[i];
        std::string safeArtist = sanitize_dir_name(track.artist);
        std::string safeAlbum = sanitize_dir_name(track.album);
        fs::path targetDir = destRoot / safeArtist / safeAlbum;

        std::error_code ec;
        if (!fs::exists(targetDir)) {
            fs::create_directories(targetDir, ec);
            if (ec) {
                err(("Failed to create directory: " + targetDir.string() + " (" + ec.message() + ")").c_str());
                failedCount++;
                continue;
            }
            plog(("Created directory: " + safeArtist + "/" + safeAlbum).c_str());
        }

        fs::path dest = targetDir / track.path.filename();

        if (fs::equivalent(track.path, dest, ec)) {
            movedCount++;
            continue;
        }
        if (fs::exists(dest)) {
            warn(("Skipping (already exists): " + track.path.filename().string()).c_str());
            failedCount++;
            continue;
        }

        fs::rename(track.path, dest, ec);
        if (ec) {
            fs::copy_file(track.path, dest, fs::copy_options::none, ec);
            if (ec) {
                err(("Failed to move file: " + track.path.filename().string() + " (" + ec.message() + ")").c_str());
                failedCount++;
                continue;
            }
            fs::remove(track.path, ec);
            if (ec) {
                warn(("File copied but original could not be removed: " + track.path.filename().string()).c_str());
            }
        }

        movedCount++;
    }

    yay(("Organized " + std::to_string(movedCount) + " file(s) into artist/album folders.").c_str());
    if (failedCount > 0) {
        warn(("Failed to move " + std::to_string(failedCount) + " file(s).").c_str());
    }
}

void MassTagDirectory(const fs::path &dirPath, const std::string &tag, const std::string &value) {
    // fuck

    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        err("Input path does not exist or is not a directory.");
        return;
    }

    if (tag.empty()) {
        err("Tag name cannot be empty.");
        return;
    }

    // ignore checking if a value is provided js incase user wants empty val
    // haii from the guru! :D

    fs::recursive_directory_iterator dirIter(dirPath);
    std::size_t taggedCount = 0;
    std::size_t failedCount = 0;
    for (const auto &entry : dirIter) {
        if (!entry.is_regular_file() || !fc::IsValidAudioFile(entry.path())) {
            ++failedCount;
            continue;
        }

        TagLib::FileRef f(entry.path().string().c_str());
        if (f.isNull() || !f.tag() || !f.file()) {
            ++failedCount;
            continue;
        }

        TagLib::PropertyMap properties = f.file()->properties();
        StageMetaDataChanges(properties, tag, value);
        if (CommitMetaDataChanges(entry.path(), properties)) {
            ++taggedCount;
        } else {
            ++failedCount;
        }
    }

    if (taggedCount > 0) {
        yay(("Successfully updated tag for " + std::to_string(taggedCount) + " file(s).").c_str());
        if (failedCount > 0) {
            warn(("Failed to update tag for " + std::to_string(failedCount) +
                  " file(s). (Don't Panic! It could just be the album cover!)")
                     .c_str());
        }
        return;
    } else {
        err("Failed to update tag for any files in the directory. Are you sure the directory has music? :(");
        return;
    }
}


void GenerateSpectrogram(const fs::path &inputPath, const fs::path &outputImagePath) {
    if (!fs::exists(inputPath) || !fs::is_regular_file(inputPath) || !fc::IsValidAudioFile(inputPath)) {
        err("Input path does not exist or is not a regular file.");
        return;
    }
    std::string tmpTitle, tmpArtist;
    TagLib::FileRef tagFile(inputPath.string().c_str());
    if (!tagFile.isNull() && tagFile.tag()) {
        tmpTitle = tagFile.tag()->title().to8Bit(true);
        tmpArtist = tagFile.tag()->artist().to8Bit(true);
    } else {
        warn("Failed to read tags from input file for spectrogram title. Title and artist will be omitted from the image.");
    }
    SF_INFO sfinfo{};
    SNDFILE *sndfile = sf_open(inputPath.string().c_str(), SFM_READ, &sfinfo);
    if (!sndfile) {
        err("Failed to open input audio file for spectrogram generation.");
        return;
    }

    if (sfinfo.channels <= 0 || sfinfo.samplerate <= 0) {
        err("Input audio stream has invalid channel count or sample rate.");
        sf_close(sndfile);
        return;
    }

    constexpr int FFT_SIZE = 2048;

    std::vector<double> monoSamples;
    if (sfinfo.frames > 0) {
        monoSamples.reserve(static_cast<std::size_t>(sfinfo.frames));
    }

    constexpr int READ_FRAMES = 4096;
    std::vector<float> interleavedBuffer(static_cast<std::size_t>(READ_FRAMES * sfinfo.channels));

    while (true) {
        sf_count_t framesRead = sf_readf_float(sndfile, interleavedBuffer.data(), READ_FRAMES);
        if (framesRead <= 0) {
            break;
        }

        for (sf_count_t frame = 0; frame < framesRead; ++frame) {
            double sampleSum = 0.0;
            for (int channel = 0; channel < sfinfo.channels; ++channel) {
                sampleSum += interleavedBuffer[static_cast<std::size_t>(frame) * sfinfo.channels + channel];
            }
            monoSamples.push_back(sampleSum / sfinfo.channels);
        }
    }

    sf_close(sndfile);

    if (monoSamples.size() < FFT_SIZE) {
        err("Not enough decoded samples to generate spectrogram.");
        return;
    }

    const int numBins = FFT_SIZE / 2 + 1;
    const double nyquistHz = static_cast<double>(sfinfo.samplerate) / 2.0;
    const double nyquistKHz = nyquistHz / 1000.0;
    const double durationSec = static_cast<double>(monoSamples.size()) / sfinfo.samplerate;

    constexpr int imageWidth = 1920;
    constexpr int imageHeight = 1080;
    constexpr int marginLeft = 90;
    constexpr int marginTop = 40;
    constexpr int marginRight = 220;
    constexpr int marginBottom = 70;
    constexpr int colorBarWidth = 28;
    constexpr int colorBarGap = 24;

    const int plotX = marginLeft;
    const int plotY = marginTop;
    const int plotWidth = imageWidth - marginLeft - marginRight;
    const int plotHeight = imageHeight - marginTop - marginBottom;
    const int colorBarX = plotX + plotWidth + colorBarGap;
    const int colorBarY = plotY;

    if (plotWidth <= 0 || plotHeight <= 0) {
        err("Invalid spectrogram image dimensions.");
        return;
    }

    std::vector<double> hannWindow(FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; ++i) {
        hannWindow[i] = 0.5 * (1.0 - std::cos((2.0 * M_PI * i) / (FFT_SIZE - 1)));
    }

    std::vector<double> fftIn(FFT_SIZE, 0.0);
    fftw_complex* fftOut = fftw_alloc_complex(static_cast<std::size_t>(numBins));
    fftw_plan plan = fftw_plan_dft_r2c_1d(FFT_SIZE, fftIn.data(), fftOut, FFTW_ESTIMATE);
    if (!plan) {
        err("Failed to allocate FFT plan for spectrogram generation.");
        return;
    }

    constexpr double minDb = -120.0;
    constexpr double maxDb = 0.0;

    std::vector<std::uint8_t> image(static_cast<std::size_t>(imageWidth * imageHeight * 3), 0);
    const Rgb background{5, 5, 8};
    const Rgb plotBackground{8, 8, 12};
    const Rgb axisColor{190, 190, 190};
    const Rgb gridColor{42, 42, 54};
    const Rgb labelColor{220, 220, 220};
    FillRect(image, imageWidth, imageHeight, 0, 0, imageWidth, imageHeight, background);
    FillRect(image, imageWidth, imageHeight, plotX, plotY, plotWidth, plotHeight, plotBackground);

    std::vector<double> dbBins(static_cast<std::size_t>(numBins), minDb);

    const std::size_t usableSamples = monoSamples.size() - FFT_SIZE;
    const int denominator = std::max(1, plotWidth - 1);
    for (int x = 0; x < plotWidth; ++x) {
        const std::size_t startIndex = (usableSamples * static_cast<std::size_t>(x)) / denominator;

        for (int i = 0; i < FFT_SIZE; ++i) {
            fftIn[static_cast<std::size_t>(i)] = monoSamples[startIndex + static_cast<std::size_t>(i)] * hannWindow[static_cast<std::size_t>(i)];
        }

        fftw_execute(plan);

        for (int bin = 0; bin < numBins; ++bin) {
            const double realPart = fftOut[static_cast<std::size_t>(bin)][0];
            const double imagPart = fftOut[static_cast<std::size_t>(bin)][1];
            const double magnitude = std::sqrt(realPart * realPart + imagPart * imagPart) / (FFT_SIZE / 2.0);
            const double dbValue = std::max(minDb, std::min(maxDb, 20.0 * std::log10(magnitude + 1e-12)));
            dbBins[static_cast<std::size_t>(bin)] = dbValue;
        }

        for (int y = 0; y < plotHeight; ++y) {
            const double frequencyRatio = 1.0 - static_cast<double>(y) / std::max(1, plotHeight - 1);
            const double binPosition = frequencyRatio * (numBins - 1);
            const int lowerBin = static_cast<int>(binPosition);
            const int upperBin = std::min(numBins - 1, lowerBin + 1);
            const double blend = binPosition - lowerBin;

            const double dbValue = dbBins[static_cast<std::size_t>(lowerBin)] * (1.0 - blend) +
                                   dbBins[static_cast<std::size_t>(upperBin)] * blend;

            const double normalized = (dbValue - minDb) / (maxDb - minDb);
            const Rgb color = SpekLikeColor(normalized);
            PutPixel(image, imageWidth, imageHeight, plotX + x, plotY + y, color);
        }
    }

    fftw_destroy_plan(plan);
    fftw_free(fftOut); 

    const int xGridTicks = 10;
    for (int i = 0; i <= xGridTicks; ++i) {
        const int x = plotX + (plotWidth * i) / xGridTicks;
        DrawVerticalLine(image, imageWidth, imageHeight, x, plotY, plotY + plotHeight, gridColor);

        DrawVerticalLine(image, imageWidth, imageHeight, x, plotY + plotHeight, plotY + plotHeight + 8, axisColor);
        const std::string timeLabel = FormatMinutesSeconds(durationSec * i / xGridTicks);
        const int textWidth = NumberTextWidth(timeLabel, 2);
        DrawNumberText(image, imageWidth, imageHeight, x - textWidth / 2, plotY + plotHeight + 14, 2, timeLabel,
                       labelColor);
    }

    const int yGridTicks = 11;
    for (int i = 0; i <= yGridTicks; ++i) {
        const int y = plotY + (plotHeight * i) / yGridTicks;
        DrawHorizontalLine(image, imageWidth, imageHeight, plotX, plotX + plotWidth, y, gridColor);

        DrawHorizontalLine(image, imageWidth, imageHeight, plotX - 8, plotX, y, axisColor);
        const double kHz = nyquistKHz * (1.0 - static_cast<double>(i) / yGridTicks);
        std::ostringstream labelStream;
        labelStream << std::fixed << std::setprecision(kHz < 10.0 ? 1 : 0) << kHz;
        const std::string yLabel = labelStream.str();
        DrawNumberText(image, imageWidth, imageHeight, 8, y - 5, 2, yLabel, labelColor);
    }

    DrawHorizontalLine(image, imageWidth, imageHeight, plotX, plotX + plotWidth, plotY, axisColor);
    DrawHorizontalLine(image, imageWidth, imageHeight, plotX, plotX + plotWidth, plotY + plotHeight, axisColor);
    DrawVerticalLine(image, imageWidth, imageHeight, plotX, plotY, plotY + plotHeight, axisColor);
    DrawVerticalLine(image, imageWidth, imageHeight, plotX + plotWidth, plotY, plotY + plotHeight, axisColor);

    for (int y = 0; y < plotHeight; ++y) {
        const double normalized = 1.0 - static_cast<double>(y) / std::max(1, plotHeight - 1);
        const Rgb color = SpekLikeColor(normalized);
        FillRect(image, imageWidth, imageHeight, colorBarX, colorBarY + y, colorBarWidth, 1, color);
    }

    DrawHorizontalLine(image, imageWidth, imageHeight, colorBarX, colorBarX + colorBarWidth, colorBarY, axisColor);
    DrawHorizontalLine(image, imageWidth, imageHeight, colorBarX, colorBarX + colorBarWidth, colorBarY + plotHeight,
                       axisColor);
    DrawVerticalLine(image, imageWidth, imageHeight, colorBarX, colorBarY, colorBarY + plotHeight, axisColor);
    DrawVerticalLine(image, imageWidth, imageHeight, colorBarX + colorBarWidth, colorBarY, colorBarY + plotHeight,
                     axisColor);

    for (int db = 0; db >= static_cast<int>(minDb); db -= 20) {
        const double normalized = (db - minDb) / (maxDb - minDb);
        const int y = colorBarY + static_cast<int>((1.0 - normalized) * plotHeight);
        DrawHorizontalLine(image, imageWidth, imageHeight, colorBarX + colorBarWidth, colorBarX + colorBarWidth + 8,
                           y, axisColor);
        DrawNumberText(image, imageWidth, imageHeight, colorBarX + colorBarWidth + 14, y - 5, 2, std::to_string(db),
                       labelColor);
    }

    std::ostringstream titleStream;
    titleStream << std::fixed << std::setprecision(1) << "("<< tmpTitle << " - " << tmpArtist << ")" << " - SPECTROGRAM - SR " << (sfinfo.samplerate / 1000.0)
                << " KHZ / DUR " << FormatMinutesSeconds(durationSec);
    const std::string mainTitle = titleStream.str();
    const int mainTitleScale = 3;
    const int mainTitleWidth = LabelTextWidth(mainTitle, mainTitleScale);
    DrawLabelText(image, imageWidth, imageHeight, (imageWidth - mainTitleWidth) / 2, 8, mainTitleScale, mainTitle,
                  labelColor);

    const std::string xAxisTitle = "TIME (S)";
    const int xAxisTitleScale = 2;
    const int xAxisWidth = LabelTextWidth(xAxisTitle, xAxisTitleScale);
    DrawLabelText(image, imageWidth, imageHeight, plotX + (plotWidth - xAxisWidth) / 2, plotY + plotHeight + 40,
                  xAxisTitleScale, xAxisTitle, labelColor);

    const std::string yAxisTitle = "FREQUENCY (KHZ)";
    DrawLabelText(image, imageWidth, imageHeight, 8, plotY - 22, 2, yAxisTitle, labelColor);

    const std::string colorBarTitle = "LEVEL (DB)";
    const int colorBarTitleWidth = LabelTextWidth(colorBarTitle, 2);
    DrawLabelText(image, imageWidth, imageHeight,
                  colorBarX + (colorBarWidth - colorBarTitleWidth) / 2,
                  plotY - 22, 2, colorBarTitle, labelColor);

    fs::path outputPath = outputImagePath;
    if (outputPath.extension().empty()) {
        outputPath += ".png";
    }

    std::error_code directoryError;
    if (outputPath.has_parent_path()) {
        fs::create_directories(outputPath.parent_path(), directoryError);
        if (directoryError) {
            err(("Failed to create output directory: " + directoryError.message()).c_str());
            return;
        }
    }

    std::string writeError;
    if (!WriteRgbPng(outputPath, imageWidth, imageHeight, image, writeError)) {
        err(("Failed to write spectrogram PNG: " + writeError).c_str());
        return;
    }

    yay(("Spectrogram image created at: " + outputPath.string()).c_str());
    // dude what the fuck is this spectrogram code i am so sorry for this abomination of a function
}

} // namespace Operations
