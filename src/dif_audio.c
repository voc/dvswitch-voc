// Copyright 2009 Ben Hutchings.
// See the file "COPYING" for licence details.

#include <assert.h>
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

unsigned dv_buffer_get_audio(const uint8_t * buffer, int16_t * samples)
{
    const struct dv_system * system = dv_buffer_system(buffer);
    const uint8_t * as_pack = buffer + (6 + 3 * 16) * DIF_BLOCK_SIZE + 3;

    if (as_pack[0] != 0x50)
	return 0;

    enum dv_sample_rate sample_rate_code = (as_pack[4] >> 3) & 7;
    if (sample_rate_code >= dv_sample_rate_count)
	return 0;

    unsigned quant = as_pack[4] & 7;
    if (quant > 1)
	return 0;

    unsigned sample_count = 2 * (system->sample_counts[sample_rate_code].min +
				 (as_pack[1] & 0x3f));

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
			samples[pos] = decode_12bit(code);
		    }

		    pos = (system->audio_shuffle[
			       seq + system->seq_count / 2][block_n] +
			   i * system->seq_count * 9);
		    if (pos < sample_count)
		    {
			unsigned code = ((block[8 + 3 * i + 1] << 4) +
					  (block[8 + 3 * i + 2] & 0xf));
			samples[pos] = decode_12bit(code);
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
			samples[pos] = sample;
		    }
		}
	    }
	}
    }

    return sample_count / 2;
}

void dv_buffer_get_audio_levels(const uint8_t * buffer, int * levels)
{
    int16_t samples[2 * 2000];
    unsigned sample_count = dv_buffer_get_audio(buffer, samples);

    assert(2 * sample_count * sizeof(int16_t) <= sizeof(samples));

    // Total of squares of samples, so we can calculate average power.  We shift
    // right to avoid overflow.
    static const unsigned total_shift = 9;
    unsigned total_l = 0, total_r = 0;

    for (unsigned i = 0; i != sample_count; ++i)
    {
	int16_t sample = samples[2 * i];
	total_l += ((unsigned)(sample * sample)) >> total_shift;
	sample = samples[2 * i + 1];
	total_r += ((unsigned)(sample * sample)) >> total_shift;
    }

    // Calculate average power and convert to dB
    levels[0] = (total_l == 0 ? INT_MIN
		 : (int)(log10((double)total_l * (1 << total_shift) /
			       ((double)sample_count * (0x7fff * 0x7fff)))
			 * 10.0));
    levels[1] = (total_r == 0 ? INT_MIN
		 : (int)(log10((double)total_r * (1 << total_shift) /
			       ((double)sample_count * (0x7fff * 0x7fff)))
			 * 10.0));
}
