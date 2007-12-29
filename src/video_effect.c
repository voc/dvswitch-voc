// Copyright 2007 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <assert.h>
#include <string.h>

#include "video_effect.h"

enum {
    luma_bias = 16,   // black level (lower values are reserved for sync)
    chroma_bias = 128 // neutral level (chroma components are signed)
};

void video_effect_show_title_safe(struct raw_frame_ref dest)
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

void video_effect_pic_in_pic(struct raw_frame_ref dest,
			     struct raw_frame_ref source,
			     unsigned left, unsigned top,
			     unsigned right, unsigned bottom)
{
    // Subtract one from odd horizontal coordinates so that we don't
    // need to merge subsampled colour information.
    left &= ~1;
    right &= ~1;

    assert(left < right && right <= FRAME_WIDTH);
    assert(top < bottom && bottom <= dest.height);
    assert(right - left < FRAME_WIDTH);
    assert(bottom - top < dest.height);

    // Scaling tables

    struct weights {
	// Weight of source column/row on current dest column/row
	uint16_t cur;
	// Weight of source column/row on next dest column/row, plus 1
	// if this the last source column/row for this dest column/row.
	uint16_t spill;
    };
    struct weights y_col_weights[FRAME_WIDTH];
    struct weights c_col_weights[FRAME_WIDTH / 2];
    struct weights row_weights[FRAME_HEIGHT_MAX];
    unsigned e, x, y;

    e = 0;
    for (x = 0; x != FRAME_WIDTH; ++x)
    {
	e += right - left;
	y_col_weights[x].cur = ((right - left) << 16) / FRAME_WIDTH;
	if (e >= FRAME_WIDTH)
	{
	    e -= FRAME_WIDTH;
	    unsigned next_weight = (e << 16) / FRAME_WIDTH;
	    y_col_weights[x].spill = 1 + next_weight;
	    y_col_weights[x].cur -= next_weight;
	}
	else
	{
	    y_col_weights[x].spill = 0;
	}
    }

    e = 0;
    for (x = 0; x != FRAME_WIDTH / 2; ++x)
    {
	e += right - left;
	c_col_weights[x].cur = ((right - left) << 16) / FRAME_WIDTH;
	if (e >= FRAME_WIDTH)
	{
	    e -= FRAME_WIDTH;
	    unsigned next_weight = (e << 16) / FRAME_WIDTH;
	    c_col_weights[x].spill = 1 + next_weight;
	    c_col_weights[x].cur -= next_weight;
	}
	else
	{
	    c_col_weights[x].spill = 0;
	}
    }

    e = 0;
    for (y = 0; y != source.height; ++y)
    {
	e += bottom - top;
	row_weights[y].cur = ((bottom - top) << 16) / source.height;
	if (e >= source.height)
	{
	    e -= source.height;
	    unsigned next_weight = (e << 16) / source.height;
	    row_weights[y].spill = 1 + next_weight;
	    row_weights[y].cur -= next_weight;
	}
	else
	{
	    row_weights[y].spill = 0;
	}
    }

    assert(y_col_weights[FRAME_WIDTH - 1].spill == 1);
    assert(c_col_weights[FRAME_WIDTH / 2 - 1].spill == 1);
    assert(row_weights[source.height - 1].spill == 1);

    uint8_t * dp =
	dest.pixels + top * dest.pitch + left * FRAME_BYTES_PER_PIXEL;

    unsigned dest_gap = dest.pitch - (right - left) * FRAME_BYTES_PER_PIXEL;

    uint32_t y_row_buffer[FRAME_WIDTH], c_row_buffer[FRAME_WIDTH], *yp, *cp;
    memset(y_row_buffer, 0, (right - left) * sizeof(uint32_t));
    memset(c_row_buffer, 0, (right - left) * sizeof(uint32_t));

    // Loop over source rows
    for (y = 0; y != source.height; ++y)
    {
	const uint8_t * sp = source.pixels + y * source.pitch;
	unsigned row_weight = row_weights[y].cur;
	unsigned row_spill = row_weights[y].spill;

	for (;;)
	{
	    // Loop over source columns
	    yp = y_row_buffer;
	    cp = c_row_buffer;
	    for (x = 0; x != FRAME_WIDTH; x += 2)
	    {
		// Process two Y' values
		*yp += (row_weight * y_col_weights[x].cur >> 8) * sp[0];
		if (y_col_weights[x].spill)
		    *++yp += ((row_weight * (y_col_weights[x].spill - 1) >> 8)
			      * sp[0]);
		*yp += (row_weight * y_col_weights[x + 1].cur >> 8) * sp[2];
		if (y_col_weights[x + 1].spill)
		    *++yp += ((row_weight * (y_col_weights[x + 1].spill - 1)
			       >> 8)
			      * sp[2]);
		// Then the Cb and Cr values
		unsigned c_col_weight = c_col_weights[x / 2].cur;
		cp[0] += (row_weight * c_col_weight >> 8) * sp[1];
		cp[1] += (row_weight * c_col_weight >> 8) * sp[3];
		if (c_col_weights[x / 2].spill)
		{
		    cp += 2;
		    c_col_weight = c_col_weights[x / 2].spill - 1;
		    cp[0] += (row_weight * c_col_weight >> 8) * sp[1];
		    cp[1] += (row_weight * c_col_weight >> 8) * sp[3];
		}
		sp += 4;
	    }

	    if (!row_spill)
		break;

	    // Spit out destination row, interleaving Y' and Cb/Cr again
	    yp = y_row_buffer;
	    cp = c_row_buffer;
	    for (x = left; x != right; ++x)
	    {
		*dp++ = *yp++ >> 24;
		*dp++ = *cp++ >> 24;
	    }

	    if (y == source.height - 1)
		break;

	    dp += dest_gap;
	    memset(y_row_buffer, 0, (right - left) * sizeof(uint32_t));
	    memset(c_row_buffer, 0, (right - left) * sizeof(uint32_t));

	    // Scale source row to next dest row if it overlaps
	    row_weight = row_spill - 1;
	    row_spill = 0;
	    if (!row_weight)
		break;
	}
    }

    assert(dp == (dest.pixels + (bottom - 1) * dest.pitch
		  + right * FRAME_BYTES_PER_PIXEL));
}
