// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include "video_effect.h"

enum {
    luma_bias = 16,   // black level (lower values are reserved for sync)
    chroma_bias = 128 // neutral level (chroma components are signed)
};

void video_effect_show_title_safe(struct frame_decoded_ref dest)
{
    // Darken the non-title-safe area
    static const unsigned border_horiz = (FRAME_WIDTH + 5) / 10;
    const unsigned border_vert = (dest.height + 5) / 10;
    for (unsigned y = 0; y != dest.height; ++y)
    {
	uint8_t * p, * end;
	// Do left border
	p = dest.pixels + dest.pitch * y;
	end = p + FRAME_BYTES_PER_PIXEL * border_horiz;
	while (p != end)
	{
	    // Halve all components
	    *p = (*p + luma_bias) / 2, ++p;
	    *p = (*p + chroma_bias) / 2, ++p;
	}
	end = p + FRAME_BYTES_PER_PIXEL * (FRAME_WIDTH - border_horiz);
	if (y >= border_vert && y < dest.height - border_vert)
	    // Skip to right border
	    p += FRAME_BYTES_PER_PIXEL * (FRAME_WIDTH - 2 * border_horiz);
	// else continue across top border or bottom border
	while (p != end)
	{
	    // Halve all components
	    *p = (*p + luma_bias) / 2, ++p;
	    *p = (*p + chroma_bias) / 2, ++p;
	}
    }
}
