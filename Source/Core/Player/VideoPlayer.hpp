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
};

} // namespace Moon
