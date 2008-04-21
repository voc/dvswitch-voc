// Copyright 2008 Ben Hutchings.
// See the file "COPYING" for licence details.

#include "dif.h"

const struct dv_system dv_system_625_50 =
{
    .common_name = "pal",
    .frame_width = 720,
    .frame_height = 576,
    .active_region = { .left = 9, .top = 0, .right = 711, .bottom = 576 },
    .frame_rate_numer = 25,
    .frame_rate_denom = 1,
    .pixel_aspect_normal = { .width =  59, .height = 54 },
    .pixel_aspect_wide =   { .width = 118, .height = 81 },
    .seq_count = 12,
    .size = 12 * DIF_SEQUENCE_SIZE,
    .sample_limits = {
	[dv_sample_rate_48k] =  { .min_count = 1896, .max_count = 1944 },
	[dv_sample_rate_44k1] = { .min_count = 1742, .max_count = 1786 },
	[dv_sample_rate_32k] =  { .min_count = 1264, .max_count = 1296 }
    }
};

const struct dv_system dv_system_525_60 =
{
    .common_name = "ntsc",
    .frame_width = 720,
    .frame_height = 480,
    .active_region = { .left = 4, .top = 0, .right = 716, .bottom = 480 },
    .frame_rate_numer = 30000,
    .frame_rate_denom = 1001,
    .pixel_aspect_normal = { .width = 10, .height = 11 },
    .pixel_aspect_wide =   { .width = 40, .height = 33 },
    .seq_count = 10,
    .size = 10 * DIF_SEQUENCE_SIZE,
    .sample_limits = {
	[dv_sample_rate_48k] =  { .min_count = 1580, .max_count = 1620 },
	[dv_sample_rate_44k1] = { .min_count = 1452, .max_count = 1489 },
	[dv_sample_rate_32k] =  { .min_count = 1053, .max_count = 1080 }
    }
};
