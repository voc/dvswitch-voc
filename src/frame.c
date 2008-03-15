// Copyright 2008 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "frame.h"

enum dv_frame_aspect dv_frame_aspect(const struct dv_frame * frame)
{
    const uint8_t * vsc_pack = frame->buffer + 5 * DIF_BLOCK_SIZE + 53;

    // If no VSC pack present, assume normal (4:3) aspect
    if (vsc_pack[0] != 0x61)
	return dv_frame_aspect_normal;

    // Check the aspect code (depends partly on the DV variant)
    int aspect = vsc_pack[2] & 7;
    int apt = frame->buffer[4] & 7;
    if (aspect == 2 || (apt == 0 && aspect == 7))
	return dv_frame_aspect_wide;
    else
	return dv_frame_aspect_normal;
}

int raw_frame_get_buffer(AVCodecContext * context, AVFrame * header)
{
    struct raw_frame * frame = context->opaque;

    if (context->pix_fmt == PIX_FMT_YUV420P)
    {
	header->data[0] = frame->buffer._420.y;
	header->linesize[0] = FRAME_LINESIZE_4;
	header->data[1] = frame->buffer._420.cb;
	header->linesize[1] = FRAME_LINESIZE_2;
	header->data[2] = frame->buffer._420.cr;
	header->linesize[2] = FRAME_LINESIZE_2;
    }
    else if (context->pix_fmt == PIX_FMT_YUV411P)
    {
	header->data[0] = frame->buffer._411.y;
	header->linesize[0] = FRAME_LINESIZE_4;
	header->data[1] = frame->buffer._411.cb;
	header->linesize[1] = FRAME_LINESIZE_1;
	header->data[2] = frame->buffer._411.cr;
	header->linesize[2] = FRAME_LINESIZE_1;
    }
    else
    {
	assert(!"unexpected pixel format");
    }

    frame->pix_fmt = context->pix_fmt;
    header->type = FF_BUFFER_TYPE_USER;

    return 0;
}

void raw_frame_release_buffer(AVCodecContext * context __attribute__((unused)),
			      AVFrame * header)
{
    for (int i = 0; i != 4; ++i)
	header->data[i] = 0;
}

int raw_frame_reget_buffer(AVCodecContext * context __attribute__((unused)),
			   AVFrame * header __attribute__((unused)))
{
    return 0;
}
