// Copyright 2007 Ben Hutchings.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_PROTOCOL_H
#define DVSWITCH_PROTOCOL_H

#define GREETING_SIZE 4
#define GREETING_SOURCE "SORC"
#define GREETING_SINK "SINK"
#define GREETING_RAW_SINK "RSNK"
#define GREETING_REC_SINK "SNKR"
#define GREETING_ACT_SOURCE "ASRC"

#define SINK_FRAME_HEADER_SIZE 4
#define SINK_FRAME_CUT_FLAG_POS 0

// Length of an activation message.
#define ACT_MSG_SIZE 4
// Position of the video active flag byte in the message.  All non-zero
// values indicate that the mixer is using video frames from the source.
#define ACT_MSG_VIDEO_POS 0
// The remaining bytes of the activation message are reserved and should
// be 0.

#endif // !defined(DVSWITCH_PROTOCOL_H)
