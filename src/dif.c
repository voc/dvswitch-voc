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
    .pixel_aspect = {
	[dv_frame_aspect_normal] = { .width =  59, .height = 54 },
	[dv_frame_aspect_wide] =   { .width = 118, .height = 81 }
    },
    .seq_count = 12,
    .size = 12 * DIF_SEQUENCE_SIZE,
    .sample_counts = {
	[dv_sample_rate_48k] =  { .min = 1896, .max = 1944, .std_cycle_len = 1, .std_cycle = { 1920 } },
	[dv_sample_rate_44k1] = { .min = 1742, .max = 1786, .std_cycle_len = 1, .std_cycle = { 1764 } },
	[dv_sample_rate_32k] =  { .min = 1264, .max = 1296, .std_cycle_len = 1, .std_cycle = { 1280 } }
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
    .pixel_aspect = {
	[dv_frame_aspect_normal] = { .width = 10, .height = 11 },
	[dv_frame_aspect_wide] =   { .width = 40, .height = 33 }
    },
    .seq_count = 10,
    .size = 10 * DIF_SEQUENCE_SIZE,
    .sample_counts = {
	[dv_sample_rate_48k] = {
	    .min = 1580, .max = 1620,
	    .std_cycle_len = 5, .std_cycle = { 1602, 1601, 1602, 1601, 1602 }
	},
	[dv_sample_rate_44k1] = {
	    .min = 1452, .max = 1489,
	    .std_cycle_len = 100,
	    .std_cycle = {
		1471, 1472, 1471, 1472, 1471, 1472, 1471, 1472, 1471, 1472,
		1471, 1472, 1471, 1472, 1471, 1472, 1471, 1471, 1472, 1471,
		1472, 1471, 1472, 1471, 1472, 1471, 1472, 1471, 1472, 1471,
		1472, 1471, 1472, 1471, 1471, 1472, 1471, 1472, 1471, 1472,
		1471, 1472, 1471, 1472, 1471, 1472, 1471, 1472, 1471, 1472,
		1471, 1471, 1472, 1471, 1472, 1471, 1472, 1471, 1472, 1471,
		1472, 1471, 1472, 1471, 1472, 1471, 1471, 1472, 1471, 1472,
		1471, 1472, 1471, 1472, 1471, 1472, 1471, 1472, 1471, 1472,
		1471, 1472, 1471, 1471, 1472, 1471, 1472, 1471, 1472, 1471,
		1472, 1471, 1472, 1471, 1472, 1471, 1472, 1471, 1472, 1471
	    }
	},
	[dv_sample_rate_32k] = {
	    .min = 1053, .max = 1080,
	    .std_cycle_len = 15,
	    .std_cycle = {
		1068, 1067, 1068, 1068, 1068, 1067, 1068, 1068, 1068, 1067, 1068, 1068, 1068, 1067, 1068
	    }
	},
    }
};

enum dv_frame_aspect dv_buffer_get_aspect(const uint8_t * buffer)
{
    const uint8_t * vsc_pack = buffer + 5 * DIF_BLOCK_SIZE + 53;

    // If no VSC pack present, assume normal (4:3) aspect
    if (vsc_pack[0] != 0x61)
	return dv_frame_aspect_normal;

    // Check the aspect code (depends partly on the DV variant)
    int aspect = vsc_pack[2] & 7;
    int apt = buffer[4] & 7;
    if (aspect == 2 || (apt == 0 && aspect == 7))
	return dv_frame_aspect_wide;
    else
	return dv_frame_aspect_normal;
}

enum dv_sample_rate dv_buffer_get_sample_rate(const uint8_t * buffer)
{
    const uint8_t * as_pack = buffer + (6 + 3 * 16) * DIF_BLOCK_SIZE + 3;

    if (as_pack[0] == 0x50)
    {
	unsigned sample_rate = (as_pack[4] >> 3) & 7;
	if (sample_rate < dv_sample_rate_count)
	    return sample_rate;
    }

    // If no AS pack present or sample rate is unrecognised, assume 48 kHz.
    // XXX Does this make any sense?
    return dv_sample_rate_48k;
}   
