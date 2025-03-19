///////////////////////////////////////////////////////////////////////////////
// Dependencies
///////////////////////////////////////////////////////////////////////////////
#include "Core/Media/Media.hpp"
#include "Core/Media/VideoStream.hpp"
#include "Core/Media/AudioStream.hpp"
#include "Core/Media/SubtitleStream.hpp"
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
}

///////////////////////////////////////////////////////////////////////////////
// Namespace Moon
///////////////////////////////////////////////////////////////////////////////
namespace Moon
{

///////////////////////////////////////////////////////////////////////////////
Media::Media(const Path& filePath)
    : filePath(filePath)
    , fullFilePath(std::filesystem::absolute(filePath))
{
    ParseFile();
}

///////////////////////////////////////////////////////////////////////////////
Media::~Media()
{
    metadata.clear();
    streams.clear();
}

///////////////////////////////////////////////////////////////////////////////
void Media::ParseFile(void)
{
    AVFormatContext* formatContext = nullptr;

    if (avformat_open_input(&formatContext, filePath.c_str(), nullptr, nullptr)) {
        // TODO: Handle Error
        return;
    }

    if (avformat_find_stream_info(formatContext, nullptr) < 0)
    {
        // TODO: Handle error
        avformat_close_input(&formatContext);
        return;
    }

    duration = static_cast<Uint64>(formatContext->duration / AV_TIME_BASE);
    bitrate = static_cast<Uint64>(formatContext->bit_rate);

    AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_get(formatContext->metadata, "", tag, 2))) {
        metadata[tag->key] = tag->value;
    }

    for (Uint32 i = 0; i < formatContext->nb_streams; i++) {
        AVStream* stream = formatContext->streams[i];
        AVCodecParameters* codecParams = stream->codecpar;

        if (codecParams->codec_type == AVMEDIA_TYPE_VIDEO) {
            auto video = std::make_shared<VideoStream>(i);
            const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);

            if (codec) {
                video->codec.name = codec->name;
                video->codec.longName = codec->long_name;
            }

            if (codecParams->profile != FF_PROFILE_UNKNOWN) {
                video->profile = avcodec_profile_name(
                    codecParams->codec_id, codecParams->profile
                );
            }

            video->width = static_cast<Uint32>(codecParams->width);
            video->height = static_cast<Uint32>(codecParams->height);

            if (stream->sample_aspect_ratio.num) {
                video->sampleAspectRatio =
                    static_cast<float>(stream->sample_aspect_ratio.num) /
                    static_cast<float>(stream->sample_aspect_ratio.den);
            } else if (codecParams->sample_aspect_ratio.num) {
                video->sampleAspectRatio =
                    static_cast<float>(codecParams->sample_aspect_ratio.num) /
                    static_cast<float>(codecParams->sample_aspect_ratio.den);
            } else {
                video->sampleAspectRatio = 1.0f;
            }

            video->displayAspectRatio =
                video->sampleAspectRatio *
                static_cast<float>(video->width) /
                static_cast<float>(video->height);

            streams.push_back(video);
        } else if (codecParams->codec_type == AVMEDIA_TYPE_AUDIO) {
            auto audio = std::make_shared<AudioStream>(i);
            const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);

            if (codec) {
                audio->codec.name = codec->name;
                audio->codec.longName = codec->long_name;
            }

            if (codecParams->profile != FF_PROFILE_UNKNOWN) {
                audio->profile = avcodec_profile_name(
                    codecParams->codec_id, codecParams->profile
                );
            }

            audio->sampleRate = static_cast<Uint32>(codecParams->sample_rate);
            audio->channels = static_cast<Uint32>(
                codecParams->ch_layout.nb_channels);
            audio->bitsPerSample = static_cast<Uint32>(
                codecParams->bits_per_coded_sample);

            char channelLayout[64] = {0};
            av_channel_layout_describe(
                &codecParams->ch_layout, channelLayout, sizeof(channelLayout));
            audio->channelLayout = channelLayout;

            streams.push_back(audio);
        } else if (codecParams->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            auto subtitle = std::make_shared<SubtitleStream>(i);

            streams.push_back(subtitle);
        }
    }

    avformat_close_input(&formatContext);
}

} // namespace Moon
