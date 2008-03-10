// Copyright 2007 Ben Hutchings.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_FRAME_H
#define DVSWITCH_FRAME_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <sys/types.h>

#include <libdv/dv.h>

#include "dif.h"

struct dv_frame
{
    uint64_t timestamp;           // set by mixer
    unsigned serial_num;          // set by mixer
    bool do_record;               // set by mixer
    bool cut_before;              // set by mixer
    uint8_t buffer[DIF_MAX_FRAME_SIZE];
};

static inline
const struct dv_system * dv_frame_system(const struct dv_frame * frame)
{
    return dv_buffer_system(frame->buffer);
}

static inline
dv_system_t dv_frame_system_code(const struct dv_frame * frame)
{
    return (frame->buffer[3] & 0x80) ? e_dv_system_625_50 : e_dv_system_525_60;
}

#define FRAME_WIDTH           720
#define FRAME_HEIGHT_MAX      576

#define FRAME_PIXEL_FORMAT    0x32595559 // 'YUY2'
#define FRAME_BYTES_PER_PIXEL 2 // Y' and alternately Cb or Cr

struct raw_frame
{
    const struct dv_system * system;
    uint8_t buffer[FRAME_BYTES_PER_PIXEL * FRAME_WIDTH * FRAME_HEIGHT_MAX];
};

struct raw_frame_ref
{
    uint8_t * pixels;
    unsigned pitch; // number of bytes per row; width is always FRAME_WIDTH
    unsigned height;
};

#endif // !defined(DVSWITCH_FRAME_H)
