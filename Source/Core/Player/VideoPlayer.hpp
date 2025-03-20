///////////////////////////////////////////////////////////////////////////////
// Header guard
///////////////////////////////////////////////////////////////////////////////
#pragma once

///////////////////////////////////////////////////////////////////////////////
// Dependencies
///////////////////////////////////////////////////////////////////////////////
#include "Core/Config/Config.hpp"
#include "Core/Media/Media.hpp"
#include <SFML/Graphics.hpp>
#include <queue>
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
}

///////////////////////////////////////////////////////////////////////////////
// Namespace Moon
///////////////////////////////////////////////////////////////////////////////
namespace Moon
{

///////////////////////////////////////////////////////////////////////////////
/// \brief Frame structure to hold decoded video frame data
///
///////////////////////////////////////////////////////////////////////////////
struct VideoFrame
{
    Uint8* data;
    Int64 pts;
    double timestamp;
    
    VideoFrame() : data(nullptr), pts(AV_NOPTS_VALUE), timestamp(0.0) {}
    
    VideoFrame(Uint8* frameData, Int64 framePts, double frameTimestamp)
        : data(frameData), pts(framePts), timestamp(frameTimestamp) {}
        
    ~VideoFrame() {
        if (data) {
            av_free(data);
            data = nullptr;
        }
    }
};

///////////////////////////////////////////////////////////////////////////////
/// \brief
///
///////////////////////////////////////////////////////////////////////////////
class VideoPlayer
{
private:
    ///////////////////////////////////////////////////////////////////////////
    //
    ///////////////////////////////////////////////////////////////////////////
    SharedPtr<Media> mMedia;
    AVFormatContext* mFormatContext;
    AVCodecContext* mCodecContext;
    AVFrame* mFrame;
    AVFrame* mFrameRGB;
    AVPacket* mPacket;
    struct SwsContext* mSwsContext;
    Uint8* mBuffer;
    mutable sf::Texture mTexture;
    int mVideoStreamIndex;
    bool mIsPlaying;
    double mPlaybackSpeed;

    Thread mDecodeThread;
    Mutex mFrameMutex;
    ConditionVariable mFrameCV;
    Atomic<bool> mStopDecoding{false};
    Atomic<bool> mNewFrameReady{false};

    static constexpr size_t MAX_QUEUE_SIZE = 30;
    std::queue<SharedPtr<VideoFrame>> mFrameQueue;
    Mutex mQueueMutex;
    ConditionVariable mQueueFullCV;
    ConditionVariable mQueueEmptyCV;
    Atomic<double> mCurrentTimestamp{0.0};

    sf::Clock mPlaybackClock;
    double mLastFrameTime{0.0};

public:
    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    /// \param filePath
    ///
    ///////////////////////////////////////////////////////////////////////////
    VideoPlayer(const Path& filePath);

    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    ///////////////////////////////////////////////////////////////////////////
    ~VideoPlayer();

private:
    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    ///////////////////////////////////////////////////////////////////////////
    void Initialize(void);

    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    ///////////////////////////////////////////////////////////////////////////
    void DecodeFrame(void);

public:
    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    ///////////////////////////////////////////////////////////////////////////
    void Play(void);

    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    ///////////////////////////////////////////////////////////////////////////
    void Pause(void);

    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    ///////////////////////////////////////////////////////////////////////////
    void Stop(void);

    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    ///////////////////////////////////////////////////////////////////////////
    void TogglePause(void);

    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    /// \param seconds
    ///
    ///////////////////////////////////////////////////////////////////////////
    void Seek(double seconds);

    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    /// \param speed
    ///
    ///////////////////////////////////////////////////////////////////////////
    void SetPlaybackSpeed(double speed);

    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    /// \return
    ///
    ///////////////////////////////////////////////////////////////////////////
    double GetPlaybackSpeed(void) const;

    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    /// \return
    ///
    ///////////////////////////////////////////////////////////////////////////
    double GetDuration(void) const;

    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    /// \return
    ///
    ///////////////////////////////////////////////////////////////////////////
    double GetCurrentTime(void) const;

    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    ///////////////////////////////////////////////////////////////////////////
    void Update(void);

    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    /// \return
    ///
    ///////////////////////////////////////////////////////////////////////////
    sf::Texture& GetCurrentFrameTexture(void) const;

    ///////////////////////////////////////////////////////////////////////////
    /// \brief Get the current queue size
    ///
    /// \return Number of frames in the queue
    ///
    ///////////////////////////////////////////////////////////////////////////
    size_t GetQueueSize(void);

    ///////////////////////////////////////////////////////////////////////////
    /// \brief Check if video has reached the end
    ///
    /// \return True if video has reached the end
    ///
    ///////////////////////////////////////////////////////////////////////////
    bool IsEndOfVideo(void) const;
};

} // namespace Moon
