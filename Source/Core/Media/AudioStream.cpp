///////////////////////////////////////////////////////////////////////////////
// Dependencies
///////////////////////////////////////////////////////////////////////////////
#include "Core/Media/AudioStream.hpp"

///////////////////////////////////////////////////////////////////////////////
// Namespace Moon
///////////////////////////////////////////////////////////////////////////////
namespace Moon
{

///////////////////////////////////////////////////////////////////////////////
AudioStream::AudioStream(Uint32 index)
{
    this->type = Stream::Type::Audio;
    this->index = index;
}

} // namespace Moon
