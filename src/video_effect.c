// Copyright 2007 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <assert.h>
#include <string.h>

#include "video_effect.h"

enum {
    luma_bias = 16,   // black level (lower values are reserved for sync)
    luma_max = 235,
    chroma_bias = 128 // neutral level (chroma components are signed)
};

void video_effect_show_title_safe(struct raw_frame_ref dest)
{
    int chroma_shift_horiz, chroma_shift_vert;    
    avcodec_get_chroma_sub_sample(dest.pix_fmt,
				  &chroma_shift_horiz, &chroma_shift_vert);

    unsigned width = FRAME_WIDTH;
    unsigned height = dest.height;
    unsigned border_horiz = (FRAME_WIDTH + 5) / 10;
    unsigned border_vert = (dest.height + 5) / 10;
    unsigned bias = luma_bias;

    // Darken the non-title-safe area
    for (int plane = 0; plane != 3; ++plane)
    {
	if (plane == 1)
	{
	    width >>= chroma_shift_horiz;
	    border_horiz >>= chroma_shift_horiz;
	    height >>= chroma_shift_vert;
	    border_vert >>= chroma_shift_vert;
	    bias = chroma_bias;
	}

	for (unsigned y = 0; y != height; ++y)
	{
	    uint8_t * p, * end;

	    // Do left border
	    p = dest.planes.data[plane] + dest.planes.linesize[plane] * y;
	    end = p + border_horiz;
	    while (p != end)
		*p = (*p + bias) / 2, ++p;

	    end = p + width - border_horiz;
	    if (y >= border_vert && y < height - border_vert)
		// Skip to right border
		p += width - 2 * border_horiz;
	    // else continue across top border or bottom border
	    while (p != end)
		*p = (*p + bias) / 2, ++p;
	}
    }
}

void video_effect_brighten(struct raw_frame_ref dest,
			   unsigned left, unsigned top,
			   unsigned right, unsigned bottom)
{
    int chroma_shift_horiz, chroma_shift_vert;    
    avcodec_get_chroma_sub_sample(dest.pix_fmt,
				  &chroma_shift_horiz, &chroma_shift_vert);

    unsigned bias = luma_max;

    for (int plane = 0; plane != 3; ++plane)
    {
	if (plane == 1)
	{
	    left >>= chroma_shift_horiz;
	    right >>= chroma_shift_horiz;
	    top >>= chroma_shift_vert;
	    bottom >>= chroma_shift_vert;
	    bias = chroma_bias;
	}

	for (unsigned y = top; y != bottom; ++y)
	{
	    uint8_t * p = (dest.planes.data[plane]
			   + dest.planes.linesize[plane] * y + left);
	    uint8_t * end = p + (right - left);
	    while (p != end)
		*p = (*p + bias) / 2, ++p;
	}
    }
}

void video_effect_pic_in_pic(struct raw_frame_ref dest,
			     struct raw_frame_ref source,
			     unsigned left, unsigned top,
			     unsigned right, unsigned bottom)
{
    int chroma_shift_horiz, chroma_shift_vert;
    avcodec_get_chroma_sub_sample(dest.pix_fmt,
				  &chroma_shift_horiz, &chroma_shift_vert);

    // Round coordinates so they include whole numbers of chroma pixels
    left &= -(1U << chroma_shift_horiz);
    right &= -(1U << chroma_shift_horiz);
    top &= -(1U << chroma_shift_vert);
    bottom &= -(1U << chroma_shift_vert);

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
    struct weights col_weights[FRAME_WIDTH];
    struct weights row_weights[FRAME_HEIGHT_MAX];
    unsigned e, x, y;

    e = 0;
    for (x = 0; x != FRAME_WIDTH; ++x)
    {
	e += right - left;
	col_weights[x].cur = ((right - left) << 16) / FRAME_WIDTH;
	if (e >= FRAME_WIDTH)
	{
	    e -= FRAME_WIDTH;
	    unsigned next_weight = (e << 16) / FRAME_WIDTH;
	    col_weights[x].spill = 1 + next_weight;
	    col_weights[x].cur -= next_weight;
	}
	else
	{
	    col_weights[x].spill = 0;
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

    assert(col_weights[FRAME_WIDTH - 1].spill == 1);
    assert(col_weights[(FRAME_WIDTH >> chroma_shift_horiz) - 1].spill == 1);
    assert(row_weights[source.height - 1].spill == 1);
    assert(row_weights[(source.height >> chroma_shift_vert) - 1].spill == 1);

    unsigned width = FRAME_WIDTH;
    unsigned height = source.height;

    for (unsigned plane = 0; plane != 3; ++plane)
    {
	if (plane == 1)
	{
	    left >>= chroma_shift_horiz;
	    right >>= chroma_shift_horiz;
	    width >>= chroma_shift_horiz;
	    top >>= chroma_shift_vert;
	    bottom >>= chroma_shift_vert;
	    height >>= chroma_shift_vert;
	}
	uint8_t * dest_p =
	    dest.planes.data[plane] + top * dest.planes.linesize[plane] + left;
	const unsigned dest_gap = dest.planes.linesize[plane] - (right - left);
	uint32_t row_buffer[FRAME_WIDTH], * row_p;
	memset(row_buffer, 0, (right - left) * sizeof(uint32_t));

	// Loop over source rows
	for (y = 0; y != height; ++y)
	{
	    unsigned row_weight = row_weights[y].cur;
	    unsigned row_spill = row_weights[y].spill;

	    for (;;)
	    {
		// Loop over source columns
		const uint8_t * source_p = (source.planes.data[plane] +
					    source.planes.linesize[plane] * y);
		row_p = row_buffer;
		for (x = 0; x != width; ++x)
		{
		    unsigned value = *source_p++;
		    *row_p += (row_weight * col_weights[x].cur >> 8) * value;
		    if (col_weights[x].spill)
			*++row_p += ((row_weight * (col_weights[x].spill - 1)
				      >> 8) * value);
		}

		if (!row_spill)
		    break;

		// Spit out destination row
		row_p = row_buffer;
		for (x = left; x != right; ++x)
		    *dest_p++ = *row_p++ >> 24;

		if (y == height - 1)
		    break;

		dest_p += dest_gap;
		memset(row_buffer, 0, (right - left) * sizeof(uint32_t));

		// Scale source row to next dest row if it overlaps
		row_weight = row_spill - 1;
		row_spill = 0;
		if (!row_weight)
		    break;
	    }
	}

	assert(dest_p == (dest.planes.data[plane]
			  + (bottom - 1) * dest.planes.linesize[plane]
			  + right));
    }
}
