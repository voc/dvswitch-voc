// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include "video_effect.h"

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
	    // Halve all components.  U and V are signed and biased
	    // by 128 so take that into account.
	    *p++ /= 2;
	    *p++ = (*p + 128) / 2;
	}
	end = p + FRAME_BYTES_PER_PIXEL * (FRAME_WIDTH - border_horiz);
	if (y >= border_vert && y < dest.height - border_vert)
	    // Skip to right border
	    p += FRAME_BYTES_PER_PIXEL * (FRAME_WIDTH - 2 * border_horiz);
	// else continue across top border or bottom border
	while (p != end)
	{
	    *p++ /= 2;
	    *p++ = (*p + 128) / 2;
	}
    }
}
