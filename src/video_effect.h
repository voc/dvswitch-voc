// Copyright 2007 Ben Hutchings.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_VIDEO_EFFECT_H
#define DVSWITCH_VIDEO_EFFECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "frame.h"

void video_effect_show_title_safe(struct raw_frame_ref dest);
void video_effect_brighten(struct raw_frame_ref dest,
			   unsigned left, unsigned top,
			   unsigned right, unsigned bottom);
void video_effect_pic_in_pic(struct raw_frame_ref dest,
                             struct raw_frame_ref source,
			     unsigned left, unsigned top,
			     unsigned right, unsigned bottom);

#ifdef __cplusplus
}
#endif

#endif // !defined(DVSWITCH_VIDEO_EFFECT_H)
