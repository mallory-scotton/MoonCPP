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
    , mPacket(nullptr)
    , mSwsContext(nullptr)
    , mBuffer(nullptr)
    , mTexture({1U, 1U})
    , mVideoStreamIndex(-1)
    , mIsPlaying(false)
    , mPlaybackSpeed(1.0)
    , mPlaybackClock()
    , mLastFrameTime((double)mPlaybackClock.getElapsedTime().asSeconds())
{
    Initialize();
}

///////////////////////////////////////////////////////////////////////////////
VideoPlayer::~VideoPlayer()
{
    mStopDecoding = true;

    mQueueFullCV.notify_all();
    mQueueEmptyCV.notify_all();
    mFrameCV.notify_all();

    if (mDecodeThread.joinable()) {
        mDecodeThread.join();
    }

    {
        std::unique_lock<Mutex> lock(mQueueMutex);
        while (!mFrameQueue.empty()) {
            mFrameQueue.pop();
        }
    }

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

    if (mPacket) {
        av_packet_free(&mPacket);
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
    if (avformat_open_input(
        &mFormatContext, mMedia->filePath.c_str(), nullptr, nullptr) != 0
    ) {
        std::cerr << "Could not open input file: " << mMedia->filePath << std::endl;
        return;
    }

    if (avformat_find_stream_info(mFormatContext, nullptr) < 0) {
        std::cerr << "Could not find stream information" << std::endl;
        avformat_close_input(&mFormatContext);
        return;
    }

    for (unsigned int i = 0; i < mFormatContext->nb_streams; i++) {
        AVStream *stream = mFormatContext->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            mVideoStreamIndex = i;
            break;
        }
    }

    if (mVideoStreamIndex == -1) {
        std::cerr << "Could not find video stream" << std::endl;
        avformat_close_input(&mFormatContext);
        return;
    }

    AVCodecParameters* codecParams =
        mFormatContext->streams[mVideoStreamIndex]->codecpar;
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

    mPacket = av_packet_alloc();
    if (!mPacket) {
        std::cerr << "Could not allocate packet" << std::endl;
        return;
    }

    int numBytes = av_image_get_buffer_size(
        AV_PIX_FMT_RGBA, mCodecContext->width, mCodecContext->height, 1);
    mBuffer = (Uint8*)av_malloc(numBytes);

    if (!mBuffer) {
        std::cerr << "Could not allocate buffer" << std::endl;
        return;
    }

    av_image_fill_arrays(
        mFrameRGB->data, mFrameRGB->linesize, mBuffer, AV_PIX_FMT_RGBA,
        mCodecContext->width, mCodecContext->height, 1
    );

    mSwsContext = sws_getContext(
        mCodecContext->width, mCodecContext->height, mCodecContext->pix_fmt,
        mCodecContext->width, mCodecContext->height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

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

    mDecodeThread = Thread(&VideoPlayer::DecodeFrame, this);
}

///////////////////////////////////////////////////////////////////////////////
void VideoPlayer::DecodeFrame(void)
{
    AVRational timeBase =
        mFormatContext->streams[mVideoStreamIndex]->time_base;
    bool endOfFile = false;

    while (!mStopDecoding) {
        {
            std::unique_lock<Mutex> lock(mQueueMutex);
            if (mFrameQueue.size() >= MAX_QUEUE_SIZE && !endOfFile) {
                mQueueFullCV.wait(lock, [this]{
                    return (mFrameQueue.size() < MAX_QUEUE_SIZE || mStopDecoding);
                });

                if (mStopDecoding) {
                    break;
                }
            }
        }

        if (!mIsPlaying) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        int readResult = av_read_frame(mFormatContext, mPacket);
        if (readResult < 0) {
            if (readResult == AVERROR_EOF) {
                endOfFile = true;
                avcodec_send_packet(mCodecContext, nullptr);
            } else {
                break;
            }
        }

        if (!endOfFile && mPacket->stream_index != mVideoStreamIndex) {
            av_packet_unref(mPacket);
            continue;
        }

        if (!endOfFile && avcodec_send_packet(mCodecContext, mPacket) < 0) {
            av_packet_unref(mPacket);
            continue;
        }

        while (true) {
            int ret = avcodec_receive_frame(mCodecContext, mFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                mStopDecoding = true;
                break;
            }

            double timestamp = 0.0;
            if (mFrame->pts != AV_NOPTS_VALUE) {
                timestamp = av_q2d(timeBase) * mFrame->pts;
            }

            int numBytes = av_image_get_buffer_size(
                AV_PIX_FMT_RGBA, mCodecContext->width, mCodecContext->height, 1);
            Uint8* frameBuffer = (Uint8*)av_malloc(numBytes);

            if (!frameBuffer) {
                continue;
            }

            AVFrame* tempFrame = av_frame_alloc();
            av_image_fill_arrays(
                tempFrame->data, tempFrame->linesize, frameBuffer, AV_PIX_FMT_RGBA,
                mCodecContext->width, mCodecContext->height, 1
            );

            sws_scale(
                mSwsContext, mFrame->data, mFrame->linesize, 0,
                mCodecContext->height, tempFrame->data, tempFrame->linesize
            );

            {
                std::unique_lock<Mutex> lock(mQueueMutex);
                mFrameQueue.push(std::make_shared<VideoFrame>(
                    frameBuffer, mFrame->pts, timestamp
                ));
                mCurrentTimestamp = timestamp;

                mQueueEmptyCV.notify_one();
            }

            av_frame_free(&tempFrame);

            if (mPlaybackSpeed != 1.0) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(static_cast<int>(10 / mPlaybackSpeed))
                );
            }
        }

        if (!endOfFile) {
            av_packet_unref(mPacket);
        }

        if (endOfFile && avcodec_receive_frame(mCodecContext, mFrame) == AVERROR_EOF) {
            break;
        }
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
void VideoPlayer::Stop(void)
{
    mIsPlaying = false;
    mStopDecoding = true;
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
    if (av_seek_frame(mFormatContext,
        mVideoStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD) < 0
    ) {
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
    return (mCurrentTimestamp);
}

///////////////////////////////////////////////////////////////////////////////
void VideoPlayer::SetPlaybackSpeed(double speed)
{
    mPlaybackSpeed = std::max(0.25, std::min(speed, 4.0));
}

///////////////////////////////////////////////////////////////////////////////
double VideoPlayer::GetPlaybackSpeed(void) const
{
    return (mPlaybackSpeed);
}

///////////////////////////////////////////////////////////////////////////////
void VideoPlayer::Update(void)
{
    if (!mIsPlaying) {
        return;
    }

    SharedPtr<VideoFrame> frame = nullptr;

    {
        static double frameDuration = 1.0 / av_q2d(
            mFormatContext->streams[mVideoStreamIndex]->avg_frame_rate
        );

        if (mPlaybackClock.getElapsedTime().asSeconds() - mLastFrameTime < frameDuration / mPlaybackSpeed) {
            return;
        }
        mLastFrameTime = mPlaybackClock.getElapsedTime().asSeconds();

        std::unique_lock<Mutex> lock(mQueueMutex);
        if (mFrameQueue.empty()) {
            return;
        }

        frame = mFrameQueue.front();
        mFrameQueue.pop();

        mQueueFullCV.notify_one();
    }

    if (frame && frame->data) {
        mTexture.update(frame->data, {
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

///////////////////////////////////////////////////////////////////////////////
size_t VideoPlayer::GetQueueSize(void)
{
    std::unique_lock<Mutex> lock(mQueueMutex);
    return (mFrameQueue.size());
}

///////////////////////////////////////////////////////////////////////////////
bool VideoPlayer::IsEndOfVideo(void) const
{
    return (mStopDecoding && mFrameQueue.empty());
}

} // namespace Moon
