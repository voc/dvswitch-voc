// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_VIDEO_EFFECT_H
#define DVSWITCH_VIDEO_EFFECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "frame.h"

void video_effect_show_title_safe(struct frame_decoded_ref dest);

#ifdef __cplusplus
}
#endif

#endif // !defined(DVSWITCH_VIDEO_EFFECT_H)
