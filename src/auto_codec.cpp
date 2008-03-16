// Copyright 2008 Ben Hutchings <ben@decadent.org.uk>.
// See the file "COPYING" for licence details.

#include <boost/thread/mutex.hpp>

#include "auto_codec.hpp"
#include "os_error.hpp"

namespace
{
    boost::mutex avcodec_mutex;

    class avcodec_initialiser
    {
	avcodec_initialiser()
	{
	    avcodec_init();
	}

	static avcodec_initialiser instance_;
    };
}

auto_codec auto_codec_open(AVCodec * codec)
{
    auto_codec result(avcodec_alloc_context());
    if (!result.get())
	throw std::bad_alloc();
    auto_codec_open(result, codec);
    return result;
}

void auto_codec_open(const auto_codec & context, AVCodec * codec)
{
    boost::mutex::scoped_lock lock(avcodec_mutex);
    os_check_error("avcodec_open", -avcodec_open(context.get(), codec));
}

void auto_codec_closer::operator()(AVCodecContext * context) const
{
    if (context)
    {
	if (context->codec)
	{
	    boost::mutex::scoped_lock lock(avcodec_mutex);
	    avcodec_close(context);
	}
	av_free(context);
    }
}
