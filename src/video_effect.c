// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// Scaling code is based on SDL_rotozoom.c; copyright A Schiffler.
// See the file "COPYING" for licence details.

#include <assert.h>

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

void video_effect_pic_in_pic(struct frame_decoded_ref dest,
			     struct frame_decoded_ref source,
			     unsigned left, unsigned top,
			     unsigned right, unsigned bottom)
{
    // Subtract one from odd horizontal coordinates so that we don't
    // need to merge subsampled colour information.
    left &= ~1;
    right &= ~1;

    assert(left < right && right <= FRAME_WIDTH);
    assert(top < bottom && bottom <= dest.height);

    unsigned x, y, sx, sy, *csaxy, *csaxc, *csay, cs, exy, exc, ey, t1, t2, sstep;
    uint8_t *c0y, *c0c, *c1y, *c1c;
    uint8_t *sp, *csp, *dp;
    unsigned dgap;

    /*
     * For interpolation: assume source dimension is one pixel 
     */
    /*
     * smaller to avoid overflow on right and bottom edge.     
     */
    sx = (unsigned) (65536.0 * (float) (FRAME_WIDTH - 1) / (float) (right - left));
    sy = (unsigned) (65536.0 * (float) (source.height - 1) / (float) (bottom - top));

    /*
     * Precalculate row increments 
     */
    unsigned saxy[FRAME_WIDTH + 1];
    unsigned saxc[FRAME_WIDTH / 2 + 1];
    unsigned say[FRAME_HEIGHT_MAX + 1];

    cs = 0;
    csaxy = saxy;
    for (x = left; x <= right; x++) {
	*csaxy = cs;
	csaxy++;
	cs &= 0xffff;
	cs += sx;
    }
    cs = 0;
    csaxc = saxc;
    for (x = left; x <= right; x += 2) {
	*csaxc = cs;
	csaxc++;
	cs &= 0xffff;
	cs += sx;
    }
    cs = 0;
    csay = say;
    for (y = top; y <= bottom; y++) {
	*csay = cs;
	csay++;
	cs &= 0xffff;
	cs += sy;
    }

    sp = csp = source.pixels;
    dp = dest.pixels + top * dest.pitch + left * FRAME_BYTES_PER_PIXEL;

    dgap = dest.pitch - (right - left) * FRAME_BYTES_PER_PIXEL;

    /*
     * Scan destination 
     */
    csay = say;
    for (y = top; y < bottom; y++) {
	ey = (*csay & 0xffff);

	/*
	 * Setup color source pointers 
	 */
	c0y = csp;
	c0c = c0y;
	c1y = csp + source.pitch;
	c1c = c1y;
	csaxy = saxy;
	csaxc = saxc;
	for (x = left; x < right; x += 2) {

	    /*
	     * Interpolate Y', Cb
	     */
	    exy = (*csaxy & 0xffff);
	    exc = (*csaxc & 0xffff);
	    t1 = ((((c0y[2] - c0y[0]) * exy) >> 16) + c0y[0]) & 0xff;
	    t2 = ((((c1y[2] - c1y[0]) * exy) >> 16) + c1y[0]) & 0xff;
	    dp[0] = (((t2 - t1) * ey) >> 16) + t1;
	    t1 = ((((c0c[5] - c0c[1]) * exc) >> 16) + c0c[1]) & 0xff;
	    t2 = ((((c1c[5] - c1c[1]) * exc) >> 16) + c1c[1]) & 0xff;
	    dp[1] = (((t2 - t1) * ey) >> 16) + t1;

	    /*
	     * Advance Y' source pointer
	     */
	    csaxy++;
	    sstep = (*csaxy >> 16) * 2;
	    c0y += sstep;
	    c1y += sstep;

	    /*
	     * Interpolate Y', Cr
	     */
	    exy = (*csaxy & 0xffff);
	    t1 = ((((c0y[2] - c0y[0]) * exy) >> 16) + c0y[0]) & 0xff;
	    t2 = ((((c1y[2] - c1y[0]) * exy) >> 16) + c1y[0]) & 0xff;
	    dp[2] = (((t2 - t1) * ey) >> 16) + t1;
	    t1 = ((((c0c[7] - c0c[3]) * exc) >> 16) + c0c[3]) & 0xff;
	    t2 = ((((c1c[7] - c1c[3]) * exc) >> 16) + c1c[3]) & 0xff;
	    dp[3] = (((t2 - t1) * ey) >> 16) + t1;

	    /*
	     * Advance source pointers 
	     */
	    csaxy++;
	    sstep = (*csaxy >> 16) * 2;
	    c0y += sstep;
	    c1y += sstep;
	    csaxc++;
	    sstep = (*csaxc >> 16) * 4;
	    c0c += sstep;
	    c1c += sstep;
	    /*
	     * Advance destination pointer 
	     */
	    dp += 4;
	}
	/*
	 * Advance source pointer 
	 */
	csay++;
	csp += (*csay >> 16) * source.pitch;
	/*
	 * Advance destination pointers 
	 */
	dp += dgap;
    }
}
