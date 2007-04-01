// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_FRAME_H
#define DVSWITCH_FRAME_H

#include <sys/types.h>

#include <libdv/dv.h>

#include "dif.h"

struct frame
{
    struct timeval time_received; // set by source
    unsigned serial_num;          // set by mixer
    dv_system_t system;           // set by source
    size_t size;                  // set by source
    uint8_t buffer[dif_block_size * frame_blocks_max];
};

#endif // !defined(DVSWITCH_FRAME_H)
