///////////////////////////////////////////////////////////////////////////////
// Dependencies
///////////////////////////////////////////////////////////////////////////////
#include "Core/Player/VideoPlayer.hpp"
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/opt.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
    #include <libavformat/avformat.h>
    #include <libavutil/timestamp.h>
    #include <libavfilter/avfilter.h>
    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
    #include <libswresample/swresample.h>
    #include <libavutil/audio_fifo.h>
    #include <libavdevice/avdevice.h>
}

///////////////////////////////////////////////////////////////////////////////
// Namespace Moon
///////////////////////////////////////////////////////////////////////////////
namespace Moon
{

///////////////////////////////////////////////////////////////////////////////
VideoPlayer::VideoPlayer(const Path& filePath)
    : mMedia(std::make_shared<Media>(filePath))
    , mFormatContext(nullptr)
    , mCodecContext(nullptr)
    , mFrame(nullptr)
    , mFrameRGB(nullptr)
    , mSwsContext(nullptr)
    , mBuffer(nullptr)
    , mTexture({1U, 1U})
    , mVideoStreamIndex(-1)
    , mIsPlaying(false)
{
    Initialize();
}

///////////////////////////////////////////////////////////////////////////////
VideoPlayer::~VideoPlayer()
{
    if (mBuffer) {
        av_free(mBuffer);
        mBuffer = nullptr;
    }

    if (mFrameRGB) {
        av_frame_free(&mFrameRGB);
    }

    if (mFrame) {
        av_frame_free(&mFrame);
    }

    if (mSwsContext) {
        sws_freeContext(mSwsContext);
        mSwsContext = nullptr;
    }

    if (mCodecContext) {
        avcodec_free_context(&mCodecContext);
    }

    if (mFormatContext) {
        avformat_close_input(&mFormatContext);
    }
}

///////////////////////////////////////////////////////////////////////////////
void VideoPlayer::Initialize(void)
{
    if (avformat_open_input(&mFormatContext, mMedia->filePath.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Could not open input file: " << mMedia->filePath << std::endl;
        return;
    }

    if (avformat_find_stream_info(mFormatContext, nullptr) < 0) {
        std::cerr << "Could not find stream information" << std::endl;
        avformat_close_input(&mFormatContext);
        return;
    }

    for (unsigned int i = 0; i < mFormatContext->nb_streams; i++) {
        if (mFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            mVideoStreamIndex = i;
            break;
        }
    }

    if (mVideoStreamIndex == -1) {
        std::cerr << "Could not find video stream" << std::endl;
        avformat_close_input(&mFormatContext);
        return;
    }

    AVCodecParameters* codecParams = mFormatContext->streams[mVideoStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);

    if (!codec) {
        std::cerr << "Unsupported video codec" << std::endl;
        return;
    }

    mCodecContext = avcodec_alloc_context3(codec);

    if (!mCodecContext) {
        std::cerr << "Could not allocate video codec context" << std::endl;
        return;
    }

    if (avcodec_parameters_to_context(mCodecContext, codecParams) < 0) {
        std::cerr << "Failed to copy video codec parameters to decoder context" << std::endl;
        avformat_close_input(&mFormatContext);
        return;
    }

    if (avcodec_open2(mCodecContext, codec, nullptr) < 0) {
        std::cerr << "Could not open video codec" << std::endl;
        avformat_close_input(&mFormatContext);
        return;
    }

    mFrame = av_frame_alloc();
    mFrameRGB = av_frame_alloc();

    if (!mFrame || !mFrameRGB) {
        std::cerr << "Could not allocate frames" << std::endl;
        return;
    }

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, mCodecContext->width, mCodecContext->height, 1);
    mBuffer = (Uint8*)av_malloc(numBytes);

    if (!mBuffer) {
        std::cerr << "Could not allocate buffer" << std::endl;
        return;
    }

    av_image_fill_arrays(mFrameRGB->data, mFrameRGB->linesize, mBuffer, AV_PIX_FMT_RGBA, mCodecContext->width, mCodecContext->height, 1);

    mSwsContext = sws_getContext(mCodecContext->width, mCodecContext->height, mCodecContext->pix_fmt, mCodecContext->width, mCodecContext->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!mSwsContext) {
        std::cerr << "Could not initialize SWS context" << std::endl;
        return;
    }

    if (!mTexture.resize({
        static_cast<Uint32>(mCodecContext->width),
        static_cast<Uint32>(mCodecContext->height)
    })) {
        std::cerr << "Could not resize SFML texture" << std::endl;
    }
}

///////////////////////////////////////////////////////////////////////////////
void VideoPlayer::DecodeFrame(void)
{
    AVPacket packet;
    bool frameDecoded = false;

    while (!frameDecoded && av_read_frame(mFormatContext, &packet) >= 0) {
        if (packet.stream_index == mVideoStreamIndex) {
            int ret = avcodec_send_packet(mCodecContext, &packet);
            if (ret == 0) {
                ret = avcodec_receive_frame(mCodecContext, mFrame);
                if (ret == 0) {
                    sws_scale(mSwsContext, mFrame->data, mFrame->linesize, 0, mCodecContext->height, mFrameRGB->data, mFrameRGB->linesize);
                    frameDecoded = true;
                }
            }
        }
        av_packet_unref(&packet);
    }

    if (!frameDecoded) {
        // TODO: Handle end of stream
        // You could add logic here to loop the video, pause, etc.
    }
}

///////////////////////////////////////////////////////////////////////////////
void VideoPlayer::Play(void)
{
    mIsPlaying = true;
}

///////////////////////////////////////////////////////////////////////////////
void VideoPlayer::Pause(void)
{
    mIsPlaying = false;
}

///////////////////////////////////////////////////////////////////////////////
void VideoPlayer::TogglePause(void)
{
    mIsPlaying = !mIsPlaying;
}

///////////////////////////////////////////////////////////////////////////////
void VideoPlayer::Seek(double seconds)
{
    Int64 timestamp = static_cast<Int64>(seconds * AV_TIME_BASE);
    if (av_seek_frame(mFormatContext, mVideoStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
        std::cerr << "Could not seek to timestamp: " << seconds << std::endl;
        return;
    }
    avcodec_flush_buffers(mCodecContext);
}

///////////////////////////////////////////////////////////////////////////////
double VideoPlayer::GetDuration(void) const
{
    if (mFormatContext && mFormatContext->duration != AV_NOPTS_VALUE) {
        return (static_cast<double>(mFormatContext->duration) / AV_TIME_BASE);
    }
    return (0.0);
}

///////////////////////////////////////////////////////////////////////////////
double VideoPlayer::GetCurrentTime(void) const
{
    if (mFrame && mFrame->pts != AV_NOPTS_VALUE) {
        return (av_q2d(mFormatContext->streams[mVideoStreamIndex]->time_base) * mFrame->pts);
    }
    return (0.0);
}

///////////////////////////////////////////////////////////////////////////////
void VideoPlayer::Update(void)
{
    bool newFrameDecoded = false;
    if (mIsPlaying) {
        Int64 previousPts = mFrame ? mFrame->pts : AV_NOPTS_VALUE;
        DecodeFrame();
        newFrameDecoded = mFrame && (mFrame->pts != previousPts);
    }
    if (mFrameRGB && mBuffer && (newFrameDecoded || !mIsPlaying)) {
        mTexture.update(mBuffer, {
            static_cast<Uint32>(mCodecContext->width),
            static_cast<Uint32>(mCodecContext->height)
        }, {0U, 0U});
    }
}

///////////////////////////////////////////////////////////////////////////////
sf::Texture& VideoPlayer::GetCurrentFrameTexture(void) const
{
    return (mTexture);
}

} // namespace Moon
