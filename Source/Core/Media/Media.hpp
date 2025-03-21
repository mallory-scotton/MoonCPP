///////////////////////////////////////////////////////////////////////////////
/// Header guard
///////////////////////////////////////////////////////////////////////////////
#pragma once

///////////////////////////////////////////////////////////////////////////////
// Dependencies
///////////////////////////////////////////////////////////////////////////////
#include "Core/Config/Config.hpp"
#include "Core/Media/Stream.hpp"

///////////////////////////////////////////////////////////////////////////////
// Namespace Moon
///////////////////////////////////////////////////////////////////////////////
namespace Moon
{

///////////////////////////////////////////////////////////////////////////////
/// \brief
///
///////////////////////////////////////////////////////////////////////////////
class Media
{
public:
    ///////////////////////////////////////////////////////////////////////////
    //
    ///////////////////////////////////////////////////////////////////////////
    Path filePath;
    Path fullFilePath;
    Uint64 duration;
    Uint64 bitrate;
    Map<String, String> metadata;
    Vector<SharedPtr<Stream>> streams;

public:
    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    /// \param filePath
    ///
    ///////////////////////////////////////////////////////////////////////////
    explicit Media(const Path& filePath);

    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    ///////////////////////////////////////////////////////////////////////////
    ~Media();

private:
    ///////////////////////////////////////////////////////////////////////////
    /// \brief
    ///
    ///////////////////////////////////////////////////////////////////////////
    void ParseFile(void);
};

} // namespace Moon
