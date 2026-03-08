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
        int maxOutSamples = av_rescale_rnd(swr_get_delay(resampler, decoderContext->sample_rate) + inputSamples,
                                           encoderContext->sample_rate, decoderContext->sample_rate, AV_ROUND_UP);

        if (maxOutSamples <= 0) {
            return true;
        }

        uint8_t **convertedData = nullptr;
        int convertedLineSize = 0;

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

        if (av_audio_fifo_realloc(audioFifo, av_audio_fifo_size(audioFifo) + convertedSamples) < 0) {
            av_freep(&convertedData[0]);
            av_freep(&convertedData);
            err("libav: failed to grow audio FIFO.");
            return false;
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

    if (encoderContext->frame_size > 0 && av_audio_fifo_size(audioFifo) > 0) {
        if (!encodeFromFifo(encoderContext->frame_size, true)) {
            cleanup();
            return false;
        }
    } else if (encoderContext->frame_size == 0 && av_audio_fifo_size(audioFifo) > 0) {
        if (!encodeFromFifo(av_audio_fifo_size(audioFifo), false)) {
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
} // namespace

namespace Operations {
// Core operations

/*
    This file will define functions for core operations of the program, such as conversion, replaygain calculation,
    mass tagging for a directory, and other nitty gritty functions that will be a hassle to program. Will try to keep
   things as clean and organized as possible, but no promises >:D
*/

// void ConvertToFileType(const fs::path& inputPath, const fs::path& outputPath, AudioFormat format);
// void MassTagDirectory(const fs::path& dirPath, const std::string& tag, const std::string& value);
// void ApplyReplayGain(const fs::path& path, float trackGain, float albumGain);
// void CalculateReplayGain(const fs::path& path);

// We can now begin implementing these fuckass features:

void ConvertToFileType(const fs::path &inputPath, const fs::path &outputPath, AudioFormat format,
                       VBRQualities quality) {
    if (!fs::exists(inputPath)) {
        err("Input path does not exist.");
        return;
    }

    if (fs::is_directory(inputPath)) {
        if (!fs::exists(outputPath) || !fs::is_directory(outputPath)) {
            err("Output path does not exist or is not a directory!");
            return;
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

            ConvertToFileType(entry.path(), outputPath, format, quality);
            ++convertedCount;
        }

        if (convertedCount == 0) {
            warn("No valid audio files found in input directory for conversion.");
            return;
        }

        yay(("Directory conversion completed. Converted " + std::to_string(convertedCount) + " file(s).").c_str());
        if (skippedCount > 0) {
            warn(("Skipped " + std::to_string(skippedCount) + " non-audio file(s).").c_str());
        }
        return;
    }

    if (!fs::is_regular_file(inputPath)) {
        err("Input path is not a regular file.");
        return;
    }

    if (!fc::IsSpecificAudioFormat(inputPath, op::AudioFormat::FLAC) &&
        !fc::IsSpecificAudioFormat(inputPath, op::AudioFormat::WAV)) {
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
            return;
        }

        SF_INFO inInfo{};
        SNDFILE *inFile = sf_open(inputPath.string().c_str(), SFM_READ, &inInfo);
        if (!inFile) {
            err("Failed to open input audio file for conversion.");
            return;
        }

        SF_INFO outInfo{};
        outInfo.samplerate = inInfo.samplerate;
        outInfo.channels = inInfo.channels;
        outInfo.format = outputFormat;

        if (!sf_format_check(&outInfo)) {
            err("Output format is not supported by the current libsndfile build.");
            sf_close(inFile);
            return;
        }

        SNDFILE *outFile = sf_open(outputFile.string().c_str(), SFM_WRITE, &outInfo);
        if (!outFile) {
            err("Failed to create output audio file for conversion.");
            sf_close(inFile);
            return;
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
                return;
            }
        }

        sf_close(inFile);
        sf_close(outFile);
    } else {
        if (!ConvertWithLibAv(inputPath, outputFile, format, gb::opusBitrateKbps)) {
            err("Library-based conversion failed for requested output format.");
            return;
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

} // namespace Operations