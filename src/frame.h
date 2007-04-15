// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_FRAME_H
#define DVSWITCH_FRAME_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <sys/types.h>

#include <libdv/dv.h>

#include "dif.h"

struct frame
{
    uint64_t timestamp;           // set by mixer
    unsigned serial_num;          // set by mixer
    bool cut_before;              // set by mixer
    dv_system_t system;           // set by source
    size_t size;                  // set by source
    uint8_t buffer[DIF_MAX_FRAME_SIZE];
};

#define FRAME_WIDTH           720
#define FRAME_HEIGHT_625_50   576
#define FRAME_HEIGHT_525_60   480
#define FRAME_HEIGHT_MAX      576

#define FRAME_PIXEL_FORMAT    0x32595559 // 'YUY2'
#define FRAME_BYTES_PER_PIXEL 2 // Y and alternately U or V

struct frame_decoded
{
    dv_system_t system;
    uint8_t buffer[FRAME_BYTES_PER_PIXEL * FRAME_WIDTH * FRAME_HEIGHT_MAX];
};

struct frame_decoded_ref
{
    uint8_t * pixels;
    unsigned pitch; // number of bytes per row; width is always FRAME_WIDTH
    unsigned height;
};

#endif // !defined(DVSWITCH_FRAME_H)
