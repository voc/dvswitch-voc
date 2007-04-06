// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_DIF_H
#define DVSWITCH_DIF_H

#define DIF_BLOCK_SIZE 80
#define DIF_BLOCKS_PER_SEQUENCE 150
#define DIF_SEQUENCE_SIZE (DIF_BLOCK_SIZE * DIF_BLOCKS_PER_SEQUENCE)
#define DIF_MAX_FRAME_SIZE (DIF_SEQUENCE_SIZE * 12)

#define DIF_BLOCK_HEADER_SIZE 3
#define DIF_AAUX_HEADER_SIZE 5

#endif // !defined(DVSWITCH_DIF_H)
