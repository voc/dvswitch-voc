/* Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
 * See the file "COPYING" for licence details.
 */

#ifndef DVSWITCH_DIF_H
#define DVSWITCH_DIF_H

#include <stddef.h>

static const size_t dif_block_size = 80;
static const size_t dif_pack_size = 6 * 80;
static const size_t frame_blocks_max = 1800;

static const int frame_time_ns_625_50 = 1000000000 / 25;
static const int frame_time_ns_525_60 = 1001000000 / 30;

#endif /* !defined(DVSWITCH_DIF_H) */
