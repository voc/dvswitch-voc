// Copyright 2009 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <limits.h>
#include <math.h>

#include "dif.h"

static int16_t decode_12bit(unsigned code)
{
    if (code < 0x200)
    {
	return code;
    }
    else if (code < 0x800)
    {
        unsigned scale = (code >> 8) - 1;
        return ((code & 0xff) + 0x100) << scale;
    }
    else if (code == 0x800)
    {
	return 0;
    }
    else if (code < 0xe00)
    {
	unsigned scale = 14 - (code >> 8);
	return (((int)(code & 0xff) - 0x100) << scale) - 1;
    }
    else
    {
	return (int)code - 0x1000;
    }
}

int dv_buffer_get_audio_level(const uint8_t * buffer)
{
    const struct dv_system * system = dv_buffer_system(buffer);
    const uint8_t * as_pack = buffer + (6 + 3 * 16) * DIF_BLOCK_SIZE + 3;

    if (as_pack[0] != 0x50)
	return INT_MIN;

    enum dv_sample_rate sample_rate_code = (as_pack[4] >> 3) & 7;
    if (sample_rate_code >= dv_sample_rate_count)
	return INT_MIN;

    unsigned quant = as_pack[4] & 7;
    if (quant > 1)
	return INT_MIN;

    unsigned sample_count = (system->sample_counts[sample_rate_code].min +
			     (as_pack[1] & 0x3f));
    // Total of squares of samples, so we can calculate average power.  We shift
    // right to avoid overflow.
    static const unsigned total_shift = 9;
    unsigned total = 0;

    for (unsigned seq = 0;
	 seq != (quant ? system->seq_count / 2 : system->seq_count);
	 ++seq)
    {
	for (unsigned block_n = 0; block_n != 9; ++block_n)
	{
	    const uint8_t * block =
		&buffer[seq * DIF_SEQUENCE_SIZE +
			(6 + 16 * block_n) * DIF_BLOCK_SIZE];

	    if (quant) // 12-bit
	    {
		for (unsigned i = 0; i != 24; ++i)
		{
		    unsigned pos = (system->audio_shuffle[seq][block_n] +
				    i * system->seq_count * 9);
		    if (pos < sample_count)
		    {
			unsigned code = ((block[8 + 3 * i] << 4) +
					 (block[8 + 3 * i + 2] >> 4));
			int16_t sample = decode_12bit(code);
			total += ((unsigned)(sample * sample)
				  >> total_shift);
		    }

		    pos = (system->audio_shuffle[
			       seq + system->seq_count / 2][block_n] +
			   i * system->seq_count * 9);
		    if (pos < sample_count)
		    {
			unsigned code = ((block[8 + 3 * i + 1] << 4) +
					  (block[8 + 3 * i + 2] & 0xf));
			int16_t sample = decode_12bit(code);
			total += ((unsigned)(sample * sample)
				  >> total_shift);
		    }
		}
	    }
	    else // 16-bit
	    {
		for (unsigned i = 0; i != 36; ++i)
		{
		    unsigned pos = (system->audio_shuffle[seq][block_n] +
				    i * system->seq_count * 9);
		    if (pos < sample_count)
		    {
			int16_t sample = (block[8 + 2 * i] +
					  (block[8 + 2 * i] << 8));
			if (sample == -0x8000)
			    sample = 0;
			total += ((unsigned)(sample * sample)
				  >> total_shift);
		    }
		}
	    }
	}
    }

    if (total == 0)
	return INT_MIN;

    // Calculate average power and convert to dB
    return (int)(log10((double)total * (1 << total_shift) /
		       ((double)sample_count * (0x7fff * 0x7fff)))
		 * 10.0);
}
