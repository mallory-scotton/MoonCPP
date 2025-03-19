///////////////////////////////////////////////////////////////////////////////
// Dependencies
///////////////////////////////////////////////////////////////////////////////
#include "Core/Media/VideoStream.hpp"

///////////////////////////////////////////////////////////////////////////////
// Namespace Moon
///////////////////////////////////////////////////////////////////////////////
namespace Moon
{

///////////////////////////////////////////////////////////////////////////////
VideoStream::VideoStream(Uint32 index)
{
    this->type = Stream::Type::Video;
    this->index = index;
}

} // namespace Moon
